# 6. Custom C++ city generator; PCG framework for scatter only

- **Status:** Accepted
- **Date:** 2026-08-31

## Context

The game generates a full city district per seed, with enterable multi-floor
interiors, a room graph that doubles as the sound-propagation graph, a power
grid, and validated objective reachability (`docs/ROADMAP.md` §5). Three hard
requirements fall out:

1. **Strict determinism** — same seed, same city, forever. The save system
   (seed + delta log) silently corrupts without it.
2. **Headless testability** — nightly 10,000-seed validation soaks with no
   editor and no actors.
3. **Reviewability** — generation logic must be diffable text (ADR-0005).

UE 5.8's PCG framework is genuinely strong (and the rebuilt City Sample PCG
is impressive reference material), but PCG graphs are binary assets, are
awkward to unit-test headlessly, optimize for an artist-in-editor workflow
this project does not have — and City Sample generates exteriors only; the
hardest problem (validated, enterable, portal-aligned interiors) is custom
work under any approach.

## Decision

- The city generator is **custom C++** in a dedicated runtime module
  (`FootcandleGen`) with no editor dependencies and no dependency on the game
  module. It is a pure function from `(Seed, Ruleset)` to a plain data
  structure; spawning is a separate streaming-driven consumer.
- All generation randomness uses seeded `FRandomStream`s derived from
  `Hash(GlobalSeed, StageId, Cell)`; no `TMap`/`TSet` iteration where order
  affects output; stable logical IDs for every generated entity.
- The **PCG framework may be used only for scatter-type detail** (debris,
  clutter) that carries no gameplay identity, no save state, and no
  validation obligations. A time-boxed spike at M5 decides whether even that
  earns its place.

## Consequences

- We build and maintain grammar/subdivision/validation code that PCG would
  partially provide — accepted, because the deterministic, testable core is
  most of the project's engineering value.
- Generation is trivially soak-testable at thousands of seeds per hour,
  which becomes the project's main QA engine.
- City Sample PCG remains a reference for *what to generate* (road/massing
  aesthetics), not *how*.
- If this proves wrong, the seam is clean: `FootcandleGen` outputs data; a
  PCG-based producer of the same data structure could replace a stage
  without touching consumers. That would be a superseding ADR.
