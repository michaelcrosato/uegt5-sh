# Install the pinned NVIDIA DLSS 4.5 plugin set into Plugins/ (ADR-0007).
# The vendored plugins are gitignored; this script IS the source of truth.
# Version pin: v8.7.2 for UE 5.8 (2026-07-21 package). Upgrades are a
# deliberate decision-log change re-tested against the M2 capture set.
param(
    [string]$Url = "https://developer.nvidia.com/downloads/assets/gameworks/downloads/secure/dlss/UE-DLSS-5.8/8.7.2/2026.07.21_UE5.8_DLSS4.5Plugin_v8.7.2.zip"
)
$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path "$PSScriptRoot\..\.."
$Zip = Join-Path $RepoRoot "Saved\dlss_plugin.zip"
$Extract = Join-Path $RepoRoot "Saved\dlss_extract"
$Dst = Join-Path $RepoRoot "Plugins"

if (-not (Test-Path "$Extract\Plugins\DLSS")) {
    if (-not (Test-Path $Zip)) {
        Write-Host "[dlss] downloading $Url"
        Invoke-WebRequest -Uri $Url -OutFile $Zip -UserAgent "Mozilla/5.0"
    }
    Write-Host "[dlss] extracting"
    Expand-Archive $Zip $Extract -Force
}

# DLSS: Super Resolution + Ray Reconstruction + DLAA. NIS: sharpening.
# StreamlineCore/NGXCommon/Reflex/DLSSG: Reflex + Frame Generation
# (FG never counts toward perf gates - 40/50-series only).
New-Item -ItemType Directory -Force $Dst | Out-Null
foreach ($p in @("DLSS", "NIS", "StreamlineCore", "StreamlineNGXCommon", "StreamlineReflex", "StreamlineDLSSG")) {
    Copy-Item "$Extract\Plugins\$p" "$Dst\$p" -Recurse -Force
    Write-Host "[dlss] installed $p"
}
Write-Host "[dlss] done - enable in Footcandle.uproject and rebuild"
