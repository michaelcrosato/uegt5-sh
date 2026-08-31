# 7. DLSS 4.5 via the official plugin; stock engine; upscaler abstraction

- **Status:** Accepted
- **Date:** 2026-08-31

## Context

The project requires the latest DLSS technology, optimized for RTX ray
tracing. The frame budget (`docs/ROADMAP.md` §6.1) assumes upscaling: 1080p60
on the reference RTX 3060 Ti at DLSS Quality (~720p internal).

As of Aug 2026 the current stack is **DLSS 4.5**: transformer Super
Resolution, transformer Ray Reconstruction (all RTX GPUs), DLAA, Reflex, and
Frame Generation / Multi Frame Generation (RTX 40/50-series only). The
official NVIDIA DLSS plugin **v8.7.2** supports UE 5.8 and exposes the entire
set — including Ray Reconstruction, which historically required NVIDIA's
NvRTX engine fork. NvRTX 5.8 exists only as a preview branch; its remaining
exclusives (Mega Geometry, RTXDI, path tracing, SER) do not serve a low-poly
game already committed to MegaLights.

The reference GPU is an RTX 30-series card: it runs SR, RR, DLAA, and Reflex,
but can never run Frame Generation.

## Decision

- **Stock Epic UE 5.8.2 + the official NVIDIA DLSS plugin (v8.7.2, pinned).**
  No NvRTX fork.
- DLSS Super Resolution is the default upscaler on RTX hardware; DLAA exposed
  where there is headroom.
- **Ray Reconstruction is evaluated at M2** on this game's actual content
  (near-black, flicker-lit Lumen HWRT + MegaLights scenes — the RR ×
  MegaLights combination is officially undocumented) and adopted per tier on
  the evidence. It is the most promising fix for the project's largest visual
  risk: temporal denoiser noise in dark RT scenes. The spike tests the RR
  generation shipped in the pinned plugin; the 2nd-gen transformer RR model
  (rolling out Sept 2026) arrives via a deliberate plugin upgrade re-tested
  against the M2 capture set, not automatically.
- Reflex Low Latency wherever supported. Nothing depends on the unreleased
  Reflex 2.
- Frame Generation / MFG ship for players whose hardware has them, but
  **generated frames never count toward any performance gate** — perf
  reports state base rendered frame rate, always.
- AMD FSR (UE 5.8 plugin) and Intel XeSS (3.1.0) ship as fallbacks; TSR is
  the vendor-free baseline. All upscalers sit behind one internal selection
  abstraction; the settings UI gates options by detected hardware.
- Plugin versions are pinned and upgraded deliberately, regression-tested
  against the M2 lighting capture set.

## Consequences

- Latest-DLSS requirement is met without carrying a vendor engine fork
  through every Epic hotfix — significant maintenance saved for a
  one-director team.
- FG/MFG code paths cannot be verified on the reference machine; they ship
  flagged as such until borrowed 40/50-series hardware verifies them
  (backlog item).
- If Mega Geometry or path tracing ever becomes genuinely necessary, that is
  a superseding ADR and an engine-fork migration — priced accordingly.
- One abstraction layer to maintain over four upscalers; paid once, at M2,
  where it is cheapest.
