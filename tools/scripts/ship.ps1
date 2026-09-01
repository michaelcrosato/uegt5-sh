# FOOTCANDLE goal-completion ship: compile -> repackage -> commit -> push.
# The standing end-of-goal protocol (CLAUDE.md rule 6, authorized by the
# director 2026-08-31): every finished /goal ships through this script so the
# repo, the clickable Dist exe, and GitHub never drift apart.
# Usage: ./tools/scripts/ship.ps1 -Message "<full commit message>" [-SkipPackage]
param(
    [Parameter(Mandatory = $true)][string]$Message,
    [switch]$SkipPackage
)
$ErrorActionPreference = "Stop"
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.8" }
$RepoRoot = Resolve-Path "$PSScriptRoot\..\.."
$Project = Join-Path $RepoRoot "Footcandle.uproject"

$Branch = git -C $RepoRoot branch --show-current
if (-not $Branch) { throw "detached HEAD - check out a branch first" }
if ($Branch -eq "main") { throw "ship.ps1 refuses to ship from main - branch first (CLAUDE.md rule 6)" }

$Dirty = git -C $RepoRoot status --porcelain
if (-not $Dirty) { Write-Host "[ship] tree clean - nothing to ship"; exit 0 }

# Compile + repackage only when compiled inputs changed - a docs/tools-only
# goal must not pay for (or invalidate) a cook.
$CodeChanged = git -C $RepoRoot status --porcelain -- Source Config Plugins "*.uproject"
if ($CodeChanged -and -not $SkipPackage) {
    Write-Host "[ship] compiled inputs changed - build + repackage Dist"
    & (Join-Path $PSScriptRoot "build.ps1")
    if ($LASTEXITCODE -ne 0) { throw "build failed - nothing committed" }
    & "$UERoot\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="$Project" `
        -noP4 -platform=Win64 -clientconfig=Development `
        -cook -build -stage -pak -archive -archivedirectory="$RepoRoot\Dist" -unattended
    if ($LASTEXITCODE -ne 0) { throw "package failed - nothing committed" }
}
else {
    Write-Host "[ship] no compiled inputs changed - skipping build/package"
}

# Same hygiene gate CI runs; a hygiene failure must block the commit.
Push-Location $RepoRoot
try {
    bash tools/scripts/check-repo-hygiene.sh
    if ($LASTEXITCODE -ne 0) { throw "repo hygiene failed - nothing committed" }

    git add -A
    git commit -m $Message
    if ($LASTEXITCODE -ne 0) { throw "commit failed" }
    git push origin $Branch
    if ($LASTEXITCODE -ne 0) { throw "push failed - commit is local only" }
    Write-Host "[ship] shipped $(git rev-parse --short HEAD) -> origin/$Branch"
}
finally {
    Pop-Location
}
