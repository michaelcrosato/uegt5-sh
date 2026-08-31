# Run FOOTCANDLE automation tests headless (no RHI, no window).
# Usage: ./tools/scripts/run-tests.ps1 [-Filter Footcandle.]
param(
    [string]$Filter = "Footcandle."
)
$ErrorActionPreference = "Stop"
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.8" }
$RepoRoot = Resolve-Path "$PSScriptRoot\..\.."
$Project = Join-Path $RepoRoot "Footcandle.uproject"
$ReportDir = Join-Path $RepoRoot "Saved\TestReports"

& "$UERoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Project" `
    -ExecCmds="Automation RunTests $Filter;Quit" `
    -TestExit="Automation Test Queue Empty" `
    -ReportOutputPath="$ReportDir" `
    -unattended -nopause -nullrhi -nosplash -log
$ExitCode = $LASTEXITCODE

$ReportJson = Join-Path $ReportDir "index.json"
if (Test-Path $ReportJson) {
    $Report = Get-Content $ReportJson -Raw | ConvertFrom-Json
    $Failed = @($Report.tests | Where-Object { $_.state -ne "Success" })
    Write-Host ("Tests: {0} total, {1} failed" -f $Report.tests.Count, $Failed.Count)
    foreach ($t in $Failed) { Write-Host ("  FAILED: {0}" -f $t.fullTestPath) }
    if ($Failed.Count -gt 0) { exit 1 }
}
exit $ExitCode
