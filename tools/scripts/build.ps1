# Build the FOOTCANDLE editor target.
# Usage: ./tools/scripts/build.ps1 [-Target FootcandleEditor] [-Config Development]
param(
    [string]$Target = "FootcandleEditor",
    [string]$Config = "Development"
)
$ErrorActionPreference = "Stop"
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.8" }
$Project = Join-Path (Resolve-Path "$PSScriptRoot\..\..") "Footcandle.uproject"

& "$UERoot\Engine\Build\BatchFiles\Build.bat" $Target Win64 $Config -project="$Project" -waitmutex
exit $LASTEXITCODE
