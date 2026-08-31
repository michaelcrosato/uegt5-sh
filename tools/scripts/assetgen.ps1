# FOOTCANDLE asset pipeline: JSON spec -> Blender -> validate -> UE import.
# Usage: ./tools/scripts/assetgen.ps1 -Spec tools/assetgen/specs/SM_Kit_Wall_01.json
param(
    [Parameter(Mandatory = $true)][string]$Spec,
    [string]$DestPath = "/Game/Generated/Kit"
)
$ErrorActionPreference = "Stop"
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.8" }
$RepoRoot = Resolve-Path "$PSScriptRoot\..\.."
$Project = Join-Path $RepoRoot "Footcandle.uproject"
$UEPython = "$UERoot\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"

# Locate Blender (newest install wins).
$Blender = Get-ChildItem "C:\Program Files\Blender Foundation\Blender*\blender.exe" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (-not $Blender) { throw "Blender not found - install it first." }

$SpecPath = Resolve-Path $Spec
$Name = (Get-Content $SpecPath -Raw | ConvertFrom-Json).name
$OutDir = Join-Path $RepoRoot "Saved\AssetGen"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$Glb = Join-Path $OutDir "$Name.glb"

Write-Host "[assetgen] generate: $Name"
& $Blender -b --factory-startup -P (Join-Path $RepoRoot "tools\assetgen\generate_mesh.py") -- "$SpecPath" "$Glb"
if ($LASTEXITCODE -ne 0) { throw "blender generate failed" }

Write-Host "[assetgen] validate"
& $UEPython (Join-Path $RepoRoot "tools\assetgen\validate_mesh.py") "$SpecPath" "$Glb"
if ($LASTEXITCODE -ne 0) { throw "validation failed" }

Write-Host "[assetgen] import into UE"
# NOTE: repo paths must stay space-free; UE's -script quoting cannot nest,
# and backslashes get escape-mangled - always hand it forward slashes.
$ImportScript = (Join-Path $RepoRoot "tools\assetgen\import_mesh.py").Replace('\', '/')
$GlbFwd = $Glb.Replace('\', '/')
& "$UERoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Project" `
    -run=pythonscript -script="$ImportScript $GlbFwd $Name $DestPath" `
    -unattended -nopause -nosplash -stdout
if ($LASTEXITCODE -ne 0) { throw "UE import failed" }
Write-Host "[assetgen] DONE: $DestPath/$Name"
