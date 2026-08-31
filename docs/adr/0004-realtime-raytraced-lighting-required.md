# 4. 100% real-time lighting; hardware ray tracing required

- **Status:** Accepted
- **Date:** 2026-08-31

## Context

The game's design (see `docs/ROADMAP.md`, pillar P4) makes lighting the
primary gameplay system: lights switch, flicker, break, and betray the player;
darkness is safety; the AI's perception of the player derives from the same
light state the renderer draws. That only works if lighting responds to the
world at runtime, everywhere, always.

`docs/design/art-direction.md` originally hedged: "be ready to fall back to
baked lighting if the target hardware demands it." A procedurally generated
city also makes baking practically impossible — there is no fixed level to
bake — and a baked fallback would mean authoring and validating a second,
different-looking, differently-behaving game.

UE 5.8 makes the all-dynamic path defensible on mid-range hardware: Lumen
hardware ray tracing is Epic's default and recommended GI path (including
60 fps targets), and MegaLights is production-ready, decoupling shadowed
light count from cost. Software Lumen works from a coarse merged distance
field rather than actual geometry — the wrong tool for a game of thin walls
and doorways where "is this room dark" must be trustworthy.

## Decision

- All illumination and shadowing is computed at runtime. No lightmaps, no
  precomputed GI, no baked AO, no painted-in shadows, ever — on any quality
  tier. `Force No Precomputed Lighting` is on; static lighting support is
  disabled.
- **Hardware ray tracing is a hard system requirement** (D3D12, SM6, RTX
  20-series / RX 6000 class or newer). The packaged game verifies HWRT at
  boot and refuses with a clear message rather than silently falling back to
  software tracing.
- Lumen (HWRT) for GI and reflections on gameplay tiers; Lumen Lite only as
  the Medium/Low concession tier; MegaLights (ray-traced shadows) for local
  lights; VSM for the single directional. Details and budgets:
  `docs/ROADMAP.md` §6.
- Shadow **presence, accuracy, and count** are prioritized over shadow
  resolution.

This supersedes the baked-lighting fallback language in
`docs/design/art-direction.md`.

## Consequences

- The store page carries an RT-capable minimum spec. Players below it are
  excluded; this is accepted and stated up front rather than discovered.
- Performance work cannot ever reach for "bake it" — the escape hatches are
  internal resolution (DLSS), Lumen tier, culling radius, and instance
  budgets. The perf gates in `docs/ROADMAP.md` §13.4 exist because of this.
- The art direction's lighting hedge is void; low-poly-under-dynamic-light is
  now a bet with no fallback, mitigated by validating on the reference GPU at
  every milestone from M2.
- Every light can be a gameplay object with zero extra lighting-pipeline
  work, which is the payoff the whole decision buys.
