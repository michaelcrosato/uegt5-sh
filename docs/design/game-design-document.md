# Game design document

> **Status: skeleton.** Only the pillars below are settled. Everything marked
> _TBD_ is deliberately open — fill it in rather than letting it be decided by
> accident in code.

## Pillars (settled)

1. **First person.** The camera is the player's eyes.
2. **Low poly.** Readability over fidelity. See [art direction](art-direction.md).
3. **Minimal animation.** Code-driven motion first, clips as a last resort.

## High concept

_TBD — one or two sentences. "You are X, doing Y, because Z."_

## Genre & references

_TBD_

## Player fantasy

_TBD — what does the player feel competent at?_

## Core loop

_TBD — the 30-second loop, then the 10-minute loop, then the session loop._

```
[ moment-to-moment ] → [ short-term goal ] → [ progression ] ↺
```

## Verbs

The complete list of things the player can do. Keeping this list short is a
design goal, not a limitation.

| Verb | Input | Notes |
| --- | --- | --- |
| Move | _TBD_ | |
| Look | _TBD_ | |
| Interact | _TBD_ | |

## Systems

_TBD — one subsection per system, each with: what it does, what it does not do,
and how the player learns it._

## World & structure

_TBD — level structure, progression shape (linear / hub / open), scale._

## Narrative

_TBD, if any. A first-person low-poly game can carry story entirely through
environment; decide early whether there is dialogue at all, because it is the
single biggest content-cost lever._

## Scope guardrails

Written before scope creep arrives, so there is something to point at later:

- Content that requires a new animation state machine gets an ADR first.
- Content that requires a new master material gets scrutiny.
- No multiplayer until single-player is proven fun. (Retrofitting replication is
  painful; if multiplayer is *ever* likely, decide **now** and write an ADR —
  building single-player-only and converting later is the expensive path.)

## Target platform & performance

- Primary: _TBD (assume Windows / PC first)_
- Target frame rate: _TBD — pick one; it changes every budget in the art doc._

## Open questions

- [ ] Is this multiplayer, ever? (Decide before writing gameplay code.)
- [ ] Enhanced Input from day one? (Yes, almost certainly — it is the UE5 default.)
- [ ] Lumen or baked lighting?
- [ ] Blueprint-heavy or C++-heavy? (Recommend: C++ for systems, Blueprint for
      composition and tuning.)
