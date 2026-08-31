# 3. Code-driven motion over skeletal animation

- **Status:** Accepted
- **Date:** 2026-08-31

## Context

Animation is the highest-cost content type per second of player experience. It
needs a rig, an animator, an authoring tool, an import pipeline, a state machine,
and blend tuning — and every one of those is a place for a small project to
stall. It is also the least reversible: once gameplay code assumes an
`AnimInstance` state machine, unwinding it touches everything.

This is a **first-person** game, which changes the calculus sharply. The player
never sees a full body. The visible animated surface is: the viewmodel (arms and
whatever they hold), and whatever moves in the world.

Meanwhile the **low-poly** direction actively tolerates — often benefits from —
motion that is snappy and stepped rather than smoothly interpolated. Smooth,
naturalistic animation would fight the art direction.

## Decision

Motion is authored in this order of preference, and each step must be ruled out
before moving to the next:

1. **No motion at all.**
2. **Transform / curve driven in code.** Timelines, `FInterpTo`, rotators,
   material parameter animation. This covers doors, lifts, pickups, hover, spin,
   weapon bob, sway and recoil.
3. **Short hand-authored clips.** Under a second where possible, few keyframes,
   snappy easing.
4. **A full skeletal animation state machine.** Requires a superseding ADR.

Explicitly excluded: mocap, retargeted marketplace animation packs, and
locomotion blendspaces for a game that renders no legs.

## Consequences

**Good:**

- No rigging or animation pipeline to build or maintain.
- Motion parameters live in code and data assets — diffable, reviewable,
  tunable at runtime. Animation assets are none of those things.
- Iteration is a recompile or a slider, not a round trip through a DCC tool.
- Consistent with the art direction rather than in tension with it.

**Bad, and accepted:**

- Organic motion (creatures, cloth, expressive characters) is off the table
  without revisiting this. If the design later demands a living, expressive NPC,
  that is precisely when a superseding ADR gets written.
- Some motion is genuinely awkward to express as curves, and code-driven motion
  can accumulate into hard-to-read tuning logic. Keep it in data assets, not
  scattered constants.
