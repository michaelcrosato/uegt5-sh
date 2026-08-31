# FOOTCANDLE visual check: launch the game with the dev scene, tour every
# test station, write one screenshot per station, exit. Look at the output.
# Usage: ./tools/scripts/visual-check.ps1 [-OutDir <dir>] [-Map /Engine/Maps/Entry]
param(
    [string]$OutDir = "",
    [string]$Map = "/Engine/Maps/Entry",
    [ValidateSet("devscene", "address")][string]$Scene = "devscene",
    [int]$TimeoutSec = 300
)
$ErrorActionPreference = "Stop"
$UERoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.8" }
$RepoRoot = Resolve-Path "$PSScriptRoot\..\.."
$Project = Join-Path $RepoRoot "Footcandle.uproject"
if (-not $OutDir) { $OutDir = Join-Path $RepoRoot "Saved\VisualCheck" }
New-Item -ItemType Directory -Force $OutDir | Out-Null

# ?game= on the URL beats any per-map GameMode override.
$MapUrl = "$Map`?game=/Script/Footcandle.FCGameMode"

$SceneFlag = if ($Scene -eq "address") { "-fcaddress" } else { "-fcdevscene" }
$UeArgs = @(
    "$Project", $MapUrl, "-game",
    "-windowed", "-resx=1280", "-resy=720",
    "-fcspectator", $SceneFlag, "-fctour=$OutDir",
    "-nosplash", "-log"
)
$Proc = Start-Process -FilePath "$UERoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    -ArgumentList $UeArgs -PassThru
if (-not $Proc.WaitForExit($TimeoutSec * 1000)) {
    Write-Host "TIMEOUT after $TimeoutSec s - killing."
    $Proc.Kill()
    exit 2
}

$Shots = Get-ChildItem $OutDir -Filter "FC_*.png" -ErrorAction SilentlyContinue
Write-Host ("Visual check done: {0} screenshots in {1}" -f @($Shots).Count, $OutDir)
foreach ($s in $Shots) { Write-Host ("  {0}  {1:N0} bytes" -f $s.Name, $s.Length) }
if (@($Shots).Count -eq 0) { exit 1 }
exit 0
