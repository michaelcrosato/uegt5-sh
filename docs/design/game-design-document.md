# Game design document

> **Status: settled at roadmap level.** The full treatment — systems specs,
> budgets, milestones — lives in [`docs/ROADMAP.md`](../ROADMAP.md). This file
> is the short-form design reference; where the two disagree, the roadmap
> wins and this file gets fixed.

## Pillars (settled)

1. **First person.** The camera is the player's eyes.
2. **Low poly.** Readability over fidelity. See [art direction](art-direction.md).
3. **Minimal animation.** Code-driven motion first, clips as a last resort (ADR-0003).
4. **Light is the game.** 100% real-time, hardware ray-traced, nothing baked
   (ADR-0004). Lighting is a gameplay system, not decoration.
5. **Sound is the second channel.** One noise model feeds the player's ears
   and the enemies' perception alike.
6. **Few enemies, total attention.** 0–2 hunters, usually 1.
7. **The seed is the run.** Deterministic procedural generation is the
   replayability and the engineering foundation.

## High concept

You wake in a procedurally generated night city that still has its lights on.
Something hunts by light and by sound. Find what the way out needs, take it,
and leave — every light you cross and every sound you make is information you
are handing the thing hunting you.

**Working title: FOOTCANDLE** — a photometric unit, and literally
footsteps + candlelight: the game's two channels in one word.

## Genre & references

First-person survival-horror escape with roguelike-adjacent runs. Feel
references (not clones): *Alien: Isolation* (patient AI, few enemies),
*Amnesia/Outlast* (no-combat vulnerability), *Thief* (light discipline,
rewritten for dynamic GI), *Lethal Company* (run structure, noise as death),
*INSIDE* (low-poly readability under rich light).

## Player fantasy

*"I out-thought a city that was trying to notice me."* Competence is reading
light and sound — not aim, not stats. Progression inside a run is knowledge
of *this* city; progression across runs is systemic literacy.

## Core loop

```
[ observe light & sound → plan route → commit → the world reacts ]   (30 s)
[ reach block → satisfy a key condition → Pressure rises ]           (10 min)
[ spawn → 2–3 conditions → extraction commit window → out or dead ]  (25–45 min session)
```

## Verbs

Complete list — keeping it short is a design goal (details: roadmap §4.6):
move (walk/sneak/sprint/crouch), lean, interact (**hold = quiet** everywhere),
flashlight, throw, hide (behind/in/under), vault, **listen/hold breath**.
No jump-for-fun, no attack.

## Systems (one-line index; specs in roadmap)

- **Procedural city** — seeded pure-function generation, every building
  enterable, portal-aligned interiors, validated before play (§5, ADR-0006).
- **Light & visibility** — MegaLights + Lumen HWRT rendering; CPU-side light
  registry drives AI perception so "looks dark" and "is dark" agree (§6, §8.3).
- **Power grid** — substation → block → building → circuit; the player
  authors light and darkness at district scale, at the cost of noise and
  attention (§5.2, §8.5).
- **Noise & propagation** — portal-graph flood, one model for ears and AI;
  weather sets the noise floor (rain is cover) (§7).
- **Enemies** — the Watcher (hunts light) and the Listener (hunts sound);
  Director paces encounters with Pressure and a rest guarantee (§8).
- **Survival economy** — battery, stamina, three-state health. Nothing else (§4.4).
- **Two-strike death** — contact wounds then kills; death names the system
  that caught you; retry same seed (§4.5).
- **Saves** — seed + delta log, kilobytes (§12).

## World & structure

One ~800 × 800 m dense, vertical night district per run; four district types;
always night; weather as a systemic modifier. Extraction locked behind 2–3
key conditions placed in distinct blocks. Hard edge (water/rail/ring road),
no invisible walls. No minimap — compass, landmarks, street signs, paper
schematic.

## Narrative

Minimal and environmental. The event is never named; there is no dialogue,
no NPCs, no quest log, no collectible lore pipeline. Tone: quiet dread — an
evacuated city whose timers kept working.

## Scope guardrails

- Content that requires a new animation state machine gets an ADR first.
- Content that requires a new master material gets scrutiny.
- **No multiplayer, decided now** (see open questions — closed). Single-player
  systems may assume no replication.
- The out-of-scope list in roadmap §4.8 is enforcement, not suggestion.

## Target platform & performance

- Primary: Windows / PC (Steam). Console is out of scope for v1.
- **60 fps at 1080p with DLSS Quality on the reference RTX 3060 Ti 8 GB**,
  hardware ray tracing required on every tier (ADR-0004, ADR-0007).
  Budgets: roadmap §6.1.

## Open questions — closed 2026-08-31

- [x] Multiplayer? **No, ever, for this game.** Decided before gameplay code.
- [x] Enhanced Input from day one? **Yes.**
- [x] Lumen or baked? **Lumen HWRT; baked is prohibited** (ADR-0004).
- [x] Blueprint-heavy or C++-heavy? **C++-first, text as source of truth**
  (ADR-0005).
