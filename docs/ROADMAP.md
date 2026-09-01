# FOOTCANDLE — Project Roadmap

**First-person procedural survival-horror escape · Unreal Engine 5.8.2 · ray tracing required**

| | |
| --- | --- |
| **Status** | Accepted — this is the plan of record to Alpha |
| **Date** | 2026-08-31 |
| **Engine** | Unreal Engine 5.8.2 (pinned — installed at `C:\Program Files\Epic Games\UE_5.8`) |
| **Reference hardware** | NVIDIA RTX 3060 Ti 8 GB (the actual dev machine — every performance gate runs here) |
| **Production model** | AI-authored code and assets, human-directed (prompting, review, playtest, QA) |
| **Working title** | **FOOTCANDLE** — a foot-candle is a photometric unit of illuminance; the word itself fuses the game's two channels: footsteps (sound) and candle (light). Rename is free until M0 exits. |

---

## 0. How to use this document

This is the source of truth for *what we are building and in what order*. It was
written after reviewing five prior proposals (see Appendix A for what was
adopted and rejected from each) and independent research into UE 5.8 and
current NVIDIA DLSS technology (Appendix B).

Three readers:

1. **The director** (Michael): milestones, exit criteria, risks, decision log.
2. **AI agents doing implementation**: sections 4–13 specify systems precisely
   enough to implement without further design input. Read the referenced ADRs
   before touching anything they govern.
3. **Future us**, reconstructing why a call was made: §17 decision log.

Rules for editing: every substantive change gets a decision-log line with a date
and one-sentence rationale. Pillars do not change silently. Irreversible
technical choices get an ADR (`docs/adr/`), not a paragraph here.

Related repo documents this roadmap builds on (they remain authoritative for
their domains): [`docs/design/art-direction.md`](design/art-direction.md),
[`docs/design/game-design-document.md`](design/game-design-document.md),
[`docs/engineering/coding-standards.md`](engineering/coding-standards.md),
[`docs/adr/`](adr/).

---

## 1. Vision

You wake in a city that still has its lights on. Everyone is gone; the timers
kept working. Streetlights hum. An apartment lamp burns behind a curtain.
Something in the district hunts by light and by sound, and every light you pass
through and every footstep you place is information you are handing it.

**FOOTCANDLE** is a first-person survival-horror escape game in a procedurally
generated night district. Every run is a new city, a new power grid, a new way
out. Every building can be entered. The art is low-poly and nearly
animation-free; the entire visual budget is spent on the one thing the player
actually feels: **fully dynamic, hardware ray-traced light** — and its absence.

You do not fight. You read the light, you listen harder than the thing
listening for you, and you leave.

**Player fantasy in one sentence:** *I out-thought a city that was trying to
notice me.*

**The replay loop:** the escape run is the session; the seed is the variety.
Layout, power state, weather, extraction type, objective placement, and enemy
dens all derive from the seed. Runs are 25–45 minutes, shareable by seed, and
the skill that carries between runs is systemic literacy — reading light and
sound — not map memorization.

---

## 2. Pillars

P1–P3 are the repo's founding pillars (see `README.md`). P4–P7 extend them for
this game. Every feature must serve at least one; a feature serving none is a
cut candidate.

- **P1 — First person.** Arms-only viewmodel at most. The world is authored at
  eye height.
- **P2 — Low poly.** Readability over fidelity. Silhouette, palette, and light
  carry the image. (`docs/design/art-direction.md`)
- **P3 — Minimal animation.** Code-driven motion first; clips are a last
  resort. (ADR-0003)
- **P4 — Light is the game.** Nothing is baked, ever. Every photon is computed
  at runtime with hardware ray tracing (ADR-0004). Because lighting is
  dynamic, lighting is *gameplay*: lights switch, flicker, break, and betray.
  An unlit room is safety; a lit room is a committed risk. Shadow **presence,
  accuracy, and count** outrank shadow resolution.
- **P5 — Sound is the second channel.** One noise model drives both what the
  player hears and what enemies perceive. They must never diverge.
- **P6 — Few enemies, total attention.** 0–2 active hunters, usually 1. An
  encounter stops everything else. Scarcity is the mechanic.
- **P7 — The seed is the run.** Generation is a deterministic pure function of
  the seed. Same seed, same city, any machine, forever. This is replayability
  *and* the foundation of the save system, QA, and soak testing.

---

## 3. Design tensions, resolved in advance

Places where the requirements pull against each other, with the binding
resolution. Agents must not re-resolve these ad hoc.

| Tension | Resolution |
| --- | --- |
| Open world + full real-time RT + 8 GB mid-range GPU | The district is **dense, not vast**: ~800 × 800 m walkable with strong verticality. RT cost scales with instances in the RT scene, not world size; the lever is visible instance count, controlled by merging, culling radius, and streaming (§6.4). |
| "Enter any building" + streaming | Interiors exist as **data** always, **geometry** only near the player (§5.5). No facades inside the playable bound — a building you can walk to, you can enter (§5.4). |
| Minimal animation + readable enemies | Enemy readability comes from silhouette, audio signature, light interaction, and movement cadence — not limbs (§8, §9). Archetypes are chosen specifically to need zero walk cycles. |
| MegaLights fixed cost + dark sparse interiors | Lean in: the art direction is deliberately light-dense (§6.2). Where the district is dark, it is dark *by gameplay decision* (dead circuits), not by author sparsity. |
| "Accurate lighting" + 60 fps on the dev card | 60 fps is the target **with DLSS**. The scalability lever is internal resolution and Lumen quality tier — never switching GI to software tracing on gameplay tiers, because software Lumen leaks light through the thin walls this game is made of, which breaks P4. |
| AI-authored everything + Unreal's binary assets | **C++ and text data are the primary authoring surface** (ADR-0005). Blueprint graphs, Behavior Trees, and PCG graphs are binary and unreviewable; they get thin-wrapper roles only. |
| Latest DLSS + RTX 30-series dev card | Integrate the full current DLSS feature set (§6.8); gate frame-rate targets on what the 3060 Ti actually renders. Frame Generation ships for 40/50-series players but **never counts** toward any performance gate. |

---

## 4. Game design

### 4.1 The run

1. **Arrival.** Spawn at a district edge. Flashlight, low battery, no map.
2. **Orientation.** The extraction is known by direction only — a distant
   landmark (radio mast, harbor crane, lit tower), visible from open ground,
   occluded in alleys.
3. **The gate.** Extraction is locked behind **2 key conditions** (tunable to
   3), placed in distinct blocks by the generator with reachability validated
   before play (§5.6).
4. **Escalation.** Satisfying conditions raises **Pressure** (§8.5). The last
   leg is the hardest.
5. **Commit.** Extraction is a *commit window*, not a doorway: a noisy, timed
   final action (power the dock, hold the strobe, force the gate) that the
   player prepares for and then survives.

**Run length:** 25–45 minutes fluent, longer on first seeds.
**Loss:** two-strike contact model (§4.5).

### 4.2 Core loop

```
OBSERVE → scan light, listen, read the room
PLAN    → route trades light exposure against noise
COMMIT  → move, open, climb, hide
DISTURB → every action emits light and/or noise
REACT   → the world and its hunters respond; adapt
```

Early run: exploration-flavored. Late run: pure evasion.

### 4.3 Key condition types

Each is a distinct block, validated reachable, and each raises Pressure when
satisfied:

- **Power.** Restore a substation → an entire block lights up permanently.
  Navigable and lethal at once. The purest expression of P4; in from the first
  playable city.
- **Key / keycard.** In a marked objective room, in a specific interior, on a
  specific floor.
- **Signal.** Rooftop transmitter. Rooftops are exposed — the vertical
  objective.
- **Mechanical.** Valve/fuse/part retrieval. Same shape as key, different
  fiction and district.

### 4.4 Survival economy — three resources, no more

| Resource | Drain | Restore | The point |
| --- | --- | --- | --- |
| **Battery** (0–100) | Flashlight on ~0.15 %/s (≈ 11 min continuous beam; CSV placeholder, tuned in playtest) | Batteries in interiors | Light = navigation and safety-from-fear; light = exposure. The central dial. |
| **Stamina** (0–100) | Sprint 12 %/s, climb 8/action | Regen walking/still | Sprinting is loud *and* finite; exhausted breathing is itself a noise event. |
| **Health** (Fine / Hurt / Critical) | Contact, falls > 4 m | Hurt→Fine slow regen; Critical needs a consumable | Hurt raises your passive noise floor (limp, breathing). Getting hurt makes you louder makes you hunted — the pressure spiral. |

**No hunger, no thirst, no temperature, no crafting tree.** They add
clock-watching, not tension, in a 35-minute run. (Decision log #7.)

### 4.5 Contact and death

First landed contact ⇒ **Critical** + heavy audiovisual hit + a short escape
window (break line of sight, gain distance). Second contact while Critical ⇒
death. This preserves stakes without the cheap-grab restart that kills
replayability at this run length.

Death screen delivers a **system attribution sentence** — *It heard the glass.
It saw your beam. It felt the stairs.* — plus run stats and the seed, with
**Retry Same Seed** and **New Run**. The death screen is a teaching tool.

### 4.6 Player verbs

| Verb | Input (default) | Noise (§7.3 scale) | Notes |
| --- | --- | --- | --- |
| Walk | WASD | 25 | Baseline |
| Sneak | hold Ctrl | 8 | 55 % speed |
| Sprint | hold Shift | 70 | Drains stamina |
| Crouch | C | ×0.6 modifier | Lowers camera, shrinks silhouette |
| Lean | Q / E | 0 | Peek without exposing |
| Interact | F (tap) / **hold F = quiet** | varies | **Hold-to-be-quiet is a global pattern**: doors, drawers, switches all have a slow-quiet and fast-loud mode. F, not E — E belongs to lean (decision #19) |
| Flashlight | T | 6 (click on toggle) | The beam itself is light, not noise — but the click is real (§7.3), and some things hear it |
| Throw | G | high at impact | The core distraction verb; throwables are found props |
| Hide | E on hideable | 5 on entry | Behind / in / under (§5.4); camera cut + latch, no animation |
| Climb / vault | Space near ledge | 30 | Camera-space parabola, no mesh (§9.3) |
| **Listen / hold breath** | hold Alt | reduces self-noise to 0 | Stop, suppress breathing, raise gain on distant audio. Converts patience into information. In the first playable build. |

### 4.7 Navigation without a minimap

No GPS, no auto-map. Compass strip, skyline landmarks, generated street signs,
and a found **paper schematic** that shows district layout only. The city being
a puzzle is the point; a minimap deletes it.

### 4.8 Explicitly out of scope (v1)

No combat or weapons that damage enemies. No multiplayer. No crafting. No
dialogue/NPCs/quest log. No day/night cycle (always night; a cycle multiplies
the lighting validation surface enormously). No driveable vehicles. No
character customization. No meta-progression that changes stats — unlocks are
cosmetic/informational only (seed gallery, accessibility presets), because
progression is knowledge of *this* city, not numbers.

---

## 5. Procedural city generation

### 5.1 The contract

`City = Generate(uint64 Seed, Ruleset)` — a **pure function**, producing a
plain data structure (`FCityData`, no UObjects, no actors). Spawning is a
separate streaming-driven step. Determinism is enforced by byte-comparison
tests and nightly seed soaks (§13.3), not by good intentions. See ADR-0006 for
why this is a custom C++ generator and not PCG-graph-driven.

Determinism rules (restated in `AGENTS.md`, linted where possible):

- All RNG via seeded `FRandomStream` derived `Hash(GlobalSeed, StageId, Cell)`.
  Never `FMath::Rand()`, never a shared stream.
- Never iterate `TMap`/`TSet` where order affects output.
- Generation never reads GPU-side, frame-timing, or wall-clock state.
- Stable logical IDs for every building, room, door, prop, light, and objective,
  derived from generation data — never actor pointers or array indices. Save
  identity depends on this (§12).

### 5.2 Pipeline stages

Ten ordered stages, each independently testable, each with a derived sub-seed:

| # | Stage | Produces |
| --- | --- | --- |
| 1 | Terrain & bounds | Ground plane, gentle elevation, hard edge (water/rail/ring road) |
| 2 | Districts | 4 types via seeded Voronoi: Residential, Commercial, Industrial, Civic. Districts drive archetypes, light density, palettes, sound floors, and enemy den preference |
| 3 | Road graph | Arterials → streets → alleys. A graph, not a grid |
| 4 | Blocks & lots | Road-bounded polygons, recursive subdivision with jitter |
| 5 | Footprint & massing | Building footprint, floor count (1–8, district-weighted), roof type; **portal list** (doors/windows/fire escapes) recorded per facade |
| 6 | Floor plans | Room subdivision per floor + a **room graph** (rooms = nodes, apertures = edges). Backbone of navigation *and* sound propagation |
| 7 | Apertures | Interior doors, windows, stair openings — **must consume the stage-5 portal list**; a portal that cannot be satisfied regenerates the floor. Facade doors that open onto walls are the classic procedural-city corpse; this is the contract that prevents them |
| 8 | Power grid & lighting | Substation → block breaker → building panel → circuit topology; fixture placement per room type and district; street lighting. Owns the light-density budget (§6.2) |
| 9 | Population | Props, hideables, throwables, physics/noise objects, batteries — weighted by room type; noise-class tagged (§7.3) |
| 10 | Objectives & validation | Extraction, key conditions, enemy dens; full validation pass (§5.6) |

### 5.3 The modular kit and the grid

Everything snaps. Constants frozen at end of M1:

Base grid 100 cm · wall module 400 cm · floor-to-floor 320 cm · door 100 × 210
· window 150 × 130 (sill 90) · corridor ≥ 200 · stair run 400 × 400 per floor.

Interior kit: **25–35 meshes total** plus district variants (`Wall`,
`WallDoorway`, `WallWindow`, `Floor`, `Ceiling`, `StairStraight`,
`StairLanding`, `DoorLeaf`, `DoorFrame`, `PillarSquare`, `RailStraight`,
`RoofFlat`, `RoofParapet`, …). A small rigid kit is the correct shape for
AI-generated assets: constraints are machine-checkable and any piece is cheap
to regenerate (§10.3).

### 5.4 Enterable by construction

Every building inside the playable bound gets a floor plan. No fake-building
category — the moment some doors are lies, the player learns to distrust all of
them. Per building: ≥ 1 street entrance, stairwells connecting all floors, roof
access at ≥ 4 floors, windows above ground floor, every room reachable from the
entrance (validated). Skyline shells beyond the playable edge are visually
distinct (welded gates, no handles) so the player never tries them.

Hide verbs are a generation guarantee, not set dressing: every interior has ≥ 1
**hide-in** (closet/locker/dumpster), ≥ 3 **hide-behind**, and ≥ 4 noise props.
Hunter search profiles interact with hide classes (§8).

### 5.5 Streaming: three tiers of existence

| Tier | Trigger | Exists |
| --- | --- | --- |
| **Data** | always | `FBuildingData`, a few KB. The whole city, permanently |
| **Shell** | ~250 m | Merged exterior mesh, window emissive proxies + a real light just inside key windows so GI still spills. In the RT scene |
| **Interior** | ~40 m or entered | Full kit as ISM/HISM, lights registered, props spawned, nav built, room graph live for sound |

Interior spawn is amortized (budgeted async spawner, never a hitch). Interiors
evict at > 80 m for > 30 s — unless **dirty** (player-disturbed state), which
persists in the delta layer (§12) even after geometry unloads. Shell tier rides
World Partition; the interior tier is a **custom streamer** — World Partition
is not designed for runtime-generated content, and fighting it costs more than
writing the streamer.

Hunters are not bound by these tiers: outside streamed interiors they
simulate **abstractly on the city data graph** (room/street-graph walk over
`FCityData`, timed by archetype speed), handing over to full
navmesh-and-perception simulation when their surroundings stream in — with
continuous position, no knowledge gain, and no teleports at the boundary
(tests AI-08 / AI-12). The Watcher entering an unstreamed building is a
graph-walk, not an error.

### 5.6 Validation (the highest-value infrastructure in the project)

Every generated city passes validation before play; failures log the seed and
either auto-repair or re-roll with a recorded reason code (bounded retries,
then a validated fallback — never a silent lie).

**Data-level checks** — pure functions over `FCityData`, run in the headless
soak (§13.3):

1. Extraction reachable from spawn without a condition-locked cycle.
2. All key conditions reachable, independently (no ordering deadlock).
3. No sealed rooms; every room connects to the exterior.
4. No orphan geometry (floating buildings, stairs into solid wall).
5. Portal alignment: every facade portal satisfied by an interior opening.
6. Light budget per streamed region within the §6.2 band.
7. Projected RT instance count within the §6.4 ceiling.
8. Hideable density: ≥ 1 hide within 25 m of any point on the primary route.

**Engine-level checks** — need spawned geometry, so they run as functional
tests on sampled seeds per milestone, not in the headless soak:

9. Navmesh continuity for enemy patrol regions.
10. Capsule clearance on doors, stairs, and hide exits.

### 5.7 PCG framework: scatter only

The city generator is custom C++ (ADR-0006). PCG is used **only** for
scatter-type detail (debris, gutter clutter) where node workflows add value and
determinism pressure is low. One time-boxed spike at M5 validates even that
use; if it does not earn its place, it is cut with a decision-log line.

---

## 6. Rendering and lighting bible

The technical expression of P4. The most-specified section because it is the
star and the thing most likely to eat the frame.

### 6.1 Targets and budgets

| | |
| --- | --- |
| Reference GPU (perf gate) | **RTX 3060 Ti 8 GB** — the dev machine. Every number below is measured here |
| Stated min spec | **Provisional:** any GPU with hardware ray tracing (RTX 20-series+, RX 6000+, Arc). RT is **required** — on the store page, in the system requirements, and at boot via a capability check that refuses (with a clear message) rather than silently falling back. Honesty note: every gate runs on the 3060 Ti only; AMD/Intel/20-series behavior is *asserted, not measured* until a borrowed-hardware validation pass (post-alpha backlog, alongside FG verification) — the published min spec stays provisional until then |
| Output | 1920 × 1080 |
| Frame target | 60 fps with DLSS Quality (≈ 720p internal) on High (default) tier |
| VRAM ceiling | **6.8 GB peak** (8 GB card minus OS/driver headroom) |

**GPU frame budget @ 60 fps (16.67 ms):**

| Pass | ms |
| --- | --- |
| RT scene build/refit | 1.0 |
| Lumen GI (final gather) | 3.5 |
| Lumen reflections | 1.2 |
| MegaLights | 2.5 |
| Base pass / GBuffer | 2.0 |
| VSM (directional only) | 1.0 |
| Volumetric fog | 0.8 |
| Post + upscale | 1.7 |
| UI | 0.3 |
| **Headroom** | **2.7** |

CPU: game ≤ 8 ms, render ≤ 8 ms, RHI ≤ 8 ms. Budgets are wired into an
automated perf gate from M2 (§13.4). A budget not enforced by automation is a
wish.

### 6.2 MegaLights

Production-ready in 5.8 and the single feature that makes this art direction
viable: ray-traced visibility for an importance-sampled subset of lights per
pixel, decoupling cost from light count.

- Project-wide on; shadow method **ray traced** for all gameplay lights
  (Epic's default and recommendation: no extra per-light cost, correct area
  shadows).
- The moon directional stays on **VSM** — Epic's documented split is exactly
  this: VSM for the strong directional, MegaLights RT shadows for local/area
  lights.
- 5.8 specifics we rely on: production-ready status, **first-person viewmodel
  support** (new in 5.8 — the flashlight shadows correctly), IES profiles on
  volumetrics, lighting channels.
- **Design for light density.** MegaLights cost is mostly constant per pixel
  regardless of light count; under ~10 lights you pay the floor and get
  nothing back. Streetlights, window spill, signage, emergency fixtures,
  fire, screens: aim **150–400 shadow-casting lights in view** exterior,
  **20–60** interior. Where a space is dark, it is dark because the *grid*
  says so, not because lights were never placed — dead circuits are how the
  game authors darkness (§5.2 stage 8).
- **The sparse-interior floor cost is the project's riskiest unknown.** No
  official base-cost number exists and no shipped title has published one.
  The **first M2 measurement** is a 3-light dark interior on the 3060 Ti:
  MegaLights on vs conventional shadowed lights. If the floor is too dear in
  sparse interiors, the answer is still not per-room toggling (popping,
  validation surface) — it is light-density authorship or a tier decision,
  recorded in the log.
- Prefer larger, dimmer emitters over tiny bright ones (sampling noise — and
  better for the look). Set attenuation radii tightly; overlapping radii
  breed noise/ghosting. Never place lights inside geometry.
- **Every gameplay light is a visible fixture** (`AFCLightFixture`, decision
  #29): a physical body — ceiling bulb on a cord, streetlight pole with a
  cobra head, TV set, emergency LED — that can be switched, wired to
  circuits, and shattered by a thrown prop (glass noise 95, permanent dark,
  a light-delta the Watcher notices). The bulb mesh sits just *beside* its
  light point, never around it: a mesh enclosing the origin blocks its own
  RT shadows and the registry's occlusion trace, and the point-blank
  inverse-square blaze on the nearby mesh is what makes "on" read hot
  without an emissive material.
- **Flicker is designed, not random.** Per-frame random flicker breaks
  temporal accumulation (denoiser ghosting/boiling — documented community
  failure mode) *and* is a photosensitivity hazard. All flicker routes
  through one flicker component with intensity changes spread over several
  frames and a global frequency/amplitude cap — which is also exactly the
  hook the accessibility photosensitivity mode needs (§11.4). One mechanism,
  two requirements served.
- **Dark-scene noise QA is a standing item** from M2: this game lives at the
  bottom of the exposure range where sampling noise is most visible. Every
  lighting milestone diff-checks a flicker room and a near-black interior
  capture.

### 6.3 Lumen

- **Hardware ray tracing, always, on every gameplay tier.** HWRT is Epic's
  default and recommended Lumen path in 5.8, including 60 fps console
  targets; Epic's guidance envelope is ~4 ms GPU for Lumen at 60 fps at
  1080p internal, and §6.1's 4.7 ms (GI + reflections) at ~720p internal
  sits comfortably inside it. Software (distance-field) tracing works
  from a coarse merged distance field rather than real geometry, which is
  exactly wrong for a game made of thin walls, doorways, and
  interior/exterior reads: the player's judgment of "that room is dark" must
  be trustworthy. Light leaks are gameplay bugs, not visual bugs (test
  LGT-08, §13.2).
- Reflections via Lumen HWRT; hit-lighting only on the top tier — low-poly
  matte surfaces rarely earn it. Wet asphalt, glass, and polished floors are
  where reflections pay.
- **Lumen Lite** (new in 5.8: irradiance-fields GI, ~2× faster than Lumen
  High, currently **Beta**) is the **Medium tier**, not the default: accuracy
  is the pillar; Lite is the concession tier that still refuses to bake. Its
  Beta status is one more reason it cannot be the primary path.
- The riskiest visual case is the interior/exterior transition (doorway,
  window-lit room, stairwell). A dedicated transition test level is built at M2
  and regression-captured every milestone after.

### 6.4 The RT scene is the real budget

RT cost scales with **instances in the acceleration structure**, not world
size: BLAS builds once per mesh at load, but the **TLAS rebuilds every frame**
at a cost proportional to instance count across render thread, RHI thread, and
GPU. A procedural city can casually emit 200 k instances and die. Controls, in
leverage order:

1. `r.RayTracing.Culling` (default mode 3: distance OR angle) + `.Radius` —
   engine default 10 000 uu; City Sample shipped 15 000 uu / 0.5°. Start at
   8 000 and profile; pull tighter indoors (fog hides the pop). Street-level
   occlusion means far geometry rarely needs RT presence.
2. Merge at generation time: a building shell is **one mesh**, not 40 modular
   pieces. ISM/HISM instances share one BLAS — the cheap way to get kit
   geometry into the TLAS.
3. RT proxies for props: a 400-tri chair shadows fine with a 40-tri proxy.
4. Tiny props excluded from the RT scene entirely (per-component flag).
5. WPO/vertex-animated geometry is excluded from RT or explicitly budgeted —
   it forces dynamic per-frame BLAS updates, and **WPO on instanced
   components converts every instance into a separate BLAS** (documented
   "extreme cost"). Kit materials never carry WPO (§9.4).

**Ceiling: ≤ 20 000 RT instances in scene** — deliberately far under Epic's
≤ 100 000-after-culling guidance for 30 fps consoles, because our target is
60 fps on a mid-range card and the ceiling doubles as a generator sanity
bound. Dynamic-geometry updates stay under the ~20 k primitives/frame
guidance for 60 fps. On the debug HUD from M2; a validation check from M5; a
CI gate from M7. Watch `stat SceneRendering` (RT active instances) and
`stat RHI` for long-session VRAM creep — 5.8's reference-based BLAS residency
helps, but meshes without LODs are never evicted.

### 6.5 Nanite policy: selective, verified

Genuinely contested territory — no authoritative low-poly ruling exists, so
we hold a hypothesis and profile it. The real forces: Nanite's fixed pipeline
cost and large-triangle raster weakness argue *against* it on sub-1 000-tri
kit pieces; but **VSM is designed around Nanite** (non-Nanite geometry is
much more expensive in VSM), GPU instance culling pays off at procedural-city
instance counts, and disabling Nanite has broken Lumen surface-cache
generation in community reports. Meanwhile the RT scene is largely neutral:
it traces fallback meshes / **streamed RT proxies** either way — and
per-platform we can disable "Generate Nanite Fallback Meshes" in favor of RT
proxies, which Epic documents as "usually saving a lot of memory" (relevant
on 8 GB). Starting policy, **verified by profiling one city block both ways
at M2 and recorded in the decision log**:

| Class | Nanite |
| --- | --- |
| Building shells, terrain, roads | Yes |
| Modular interior kit | Yes |
| Small props (< 500 tris) | No — ISM/HISM + explicit RT proxy |
| Physics props | No — cheap dynamic instances + low-poly RT geometry |
| Enemies / skinned | No |
| Anything with WPO animation | Avoid (RT update cost) |

### 6.6 Materials

- **One master material**, instanced. Parameters: palette index, roughness,
  metallic, emissive.
- **Palette atlas** (256 × 16 flat colors); all UVs point at palette texels.
  One material, one texture, near-perfect batching, trivially script-generable.
- No normal maps, no albedo textures in v1. Visual interest comes from
  geometry or light.
- **Albedo discipline: 0.15–0.65.** Near-black kills GI bounce ("Lumen looks
  broken"); near-white blows out. This single rule prevents most lighting bug
  reports.
- Roughness is the storytelling parameter: dry brick vs wet asphalt vs glass.
- Emissives feed GI but cast no MegaLights shadows — signage/screens pair with
  a real light when shadows matter.

### 6.7 Scalability tiers — every tier keeps hardware RT

| Tier | Internal res | GI | MegaLights | Target |
| --- | --- | --- | --- | --- |
| Cinematic | native 1080p | Lumen HWRT + hit lighting | On | 30 fps, screenshot mode |
| **High (default)** | DLSS Quality | Lumen HWRT | On | 60 fps @ 3060 Ti |
| Medium | DLSS Balanced | Lumen Lite | On | 60 fps @ lower-end RTX |
| Low | DLSS Performance | Lumen Lite | On, reduced (downsampled, fewer samples/pixel) | courtesy floor, RTX 2060-class |

There is no MegaLights-off tier: per-light shadow maps on a scene authored for
150–400 shadow-casting locals would be *slower*, not a floor — and a non-RT
shadow path would be exactly the second lighting game ADR-0004 exists to
forbid. The Low-tier levers are MegaLights' own quality knobs (downsample
mode, samples per pixel), internal resolution, and Lumen Lite.

Every tier launches and is eyeballed at every milestone from M2. A tier
unvisited for two months is broken.

### 6.8 DLSS and upscaling — first-class, integrated early (M2)

The stated project requirement is the **latest DLSS technology, optimized for
RTX ray tracing**. As of Aug 2026 that is **DLSS 4.5**, and the integration
path is the **official NVIDIA DLSS plugin v8.7.2 for UE 5.8** (Streamline
2.11.1 / NGX 310.6.0) — which now carries the *entire* feature set in the
stock plugin, including Ray Reconstruction. **The NvRTX engine fork is not
required** and not adopted: its remaining exclusives (Mega Geometry, RTXDI,
path tracing) don't serve this game, and carrying a vendor fork through
engine hotfixes is real cost for a one-director team. Policy (ADR-0007):

- **DLSS Super Resolution** (2nd-gen transformer model — all RTX GPUs, 20
  through 50 series) is the default upscaler on NVIDIA hardware. **DLAA**
  exposed for GPUs with headroom. Quality mode (≈720p internal at 1080p out)
  is the High-tier assumption in every §6.1 budget.
- **DLSS Ray Reconstruction** (all RTX GPUs) is the single most promising
  answer to this game's biggest visual risk: temporal denoiser
  noise/ghosting in near-black, flicker-lit RT scenes (§6.2). The M2 spike
  evaluates **the RR generation shipped in the pinned plugin**; the 2nd-gen
  transformer RR model (official release Sept 2026) is adopted later as a
  deliberate pinned-plugin upgrade, re-run against the M2 capture set.
  Two knowns from research: RR replaces the engine's RT denoisers (config
  such as `r.Lumen.Reflections.BilateralFilter=0` set at ini level, per
  NVIDIA guidance), and **RR × MegaLights interaction is officially
  undocumented** — MegaLights has its own stochastic denoiser and nobody has
  published the combination. The M2 spike answers it empirically; adoption
  per tier recorded in the decision log.
- **Reflex Low Latency** on wherever supported — latency matters in a
  lean-and-listen game. (Reflex 2 Frame Warp remains unreleased as of
  mid-2026; nothing in this plan depends on it.)
- **Frame Generation / Multi Frame Generation** (FG: 40/50-series; MFG +
  Dynamic MFG: 50-series) integrated for players who have it, **never counted
  toward any performance gate** — the reference card cannot run it, and
  generated frames are not rendered frames. Perf reports state base frame
  rate, always. FG paths are code-complete but marked untested-on-reference;
  verification needs borrowed 40/50-series time (backlog).
- Fallbacks: **AMD FSR** (UE 5.8 plugin, FSR 4.1.1 upscaling + FG 4),
  **Intel XeSS** (plugin 3.1.0 for 5.8; note XeLL was broken on 5.8.0/5.8.1 —
  we are on 5.8.2, but verify), and **TSR** as the vendor-free baseline. All
  behind one internal upscaler-selection abstraction (DLSS replaces the
  Temporal Upscaler interface and bypasses TSR when active) so settings UI,
  sharpening (NIS or none — DLSS no longer self-sharpens), and
  screen-percentage plumbing are written once. Settings menu gates options by
  detected hardware.
- VRAM cost is small and budgeted: transformer SR ≈ 86 MB at 1080p; RR
  unpublished — budget a few hundred MB total and measure at M2.
- Plugin versions are pinned in the repo and recorded in the decision log;
  upgrades are deliberate, regression-tested against the M2 capture set.

### 6.9 Volumetrics and post

Volumetric fog on all tiers (the flashlight beam and light shafts are core
vocabulary); density is a per-district generation parameter and a weather
output. Exposure locked to a narrow adaptation range — horror lives in
controlled darkness, not auto-brightened mush. Film grain, chromatic
aberration, motion blur: all default off, all optional. Brightness calibration
screen with a proper dark-reference image at first boot (this game is dark;
uncalibrated TVs are the #1 "too dark" refund driver).

---

## 7. Sound, noise, and propagation

### 7.1 One model, two consumers

A single propagation model feeds (a) the player's audio mix (occlusion, reverb
sends) and (b) enemy hearing. If a sound is muffled to the ear but loud to the
AI, stealth becomes unlearnable. The model ships before either consumer gets
fancy.

Corollary: **audio volume sliders never touch enemy hearing** (test AUD-03).

### 7.2 Portal-graph propagation

The stage-6 room graph *is* the acoustic graph: rooms are nodes, apertures are
edges; exterior space partitions into street cells connected via doors and
windows. Propagation is a Dijkstra flood:

```
Loudness(node) = Loudness(origin)
               − DistanceAttenuation(path length through graph)
               − Σ ApertureLoss(edges crossed)
               − NoiseFloor(weather, district)   // heard-above-the-floor
```

Aperture losses (CSV-tunable): open doorway 3 · open door 5 · closed interior
door 22 · closed exterior door 30 · open window 6 · closed window 25 · broken
window 4 · stairwell 4 · floor/ceiling 35 · wall 45.

Properties that make this the right model: cost proportional to graph size,
not geometry; sound turns corners and pours down stairwells the way players
intuit; opening a door genuinely changes a building's acoustics (door state
becomes tactics); fully deterministic and unit-testable; and the same numbers
drive audio occlusion for free. Flood cutoff at the quietest enemy threshold,
hard cap ~40 nodes/event.

**The noise floor is the weather system's gameplay meaning:** rain raises the
floor, so rain is cover — the downpour is when you cross the avenue. Never
tooltip this; let it be learned.

### 7.3 Noise events

`FNoiseEvent { Origin, RoomNodeId, Loudness 0–100, SourceTag, Timestamp,
Instigator }` — reference values in CSV: sneak step 8 · walk 25 · sprint 70 ·
landing 55 · door slow 12 / fast 40 / slam 85 · knocked object 45 · glass 95 ·
shelf topple 90 · throw impact 60 · hurt breathing 15 (passive) · hide entry 5
· flashlight click 6. Surface multipliers via physical material: carpet ×0.5,
wood ×1.0, concrete ×1.1, gravel ×1.4, metal grate ×1.6, broken glass ×1.8 —
and generator-placed glass fields are a deliberate hazard type.

Player passive noise floor: still+listening 0 · still 3 · hurt 15 · critical
22 · exhausted +20 for 8 s. This is what makes the hurt-spiral and the Listen
verb real (§4.4, §4.6).

### 7.4 Implementation

- **MetaSounds** for everything variation-heavy — footsteps are synthesized
  (pitch/timbre/layer variation), not sample libraries. This collapses the
  generated-asset count (§10.4).
- **Audio Modulation** mix states: Normal / Listening / Hiding / Chase / Hurt.
- Occlusion/obstruction values come **from the portal graph**, not engine
  traces — consistency with 7.1 requires it.
- Reverb derived from room volume + material mix (known at generation time).
- Binaural/HRTF spatialization for enemy audio; the player must localize a
  hunter by ear alone. **No silent enemies, ever** — a silent stealth enemy in
  a dark game is not scary, it is unfair.
- Music is scarce: menus, extraction commit, death. In-world, the city is the
  score.
- Debug: `show SoundEvents` draws propagation discs and per-enemy perceived
  origins. Shipped behind a cheat; used daily.

---

## 8. Enemies, perception, and the Director

### 8.1 Population and pacing

0–2 active hunters, usually 1. Encounters every 4–7 minutes, lasting 60–180 s.
Between encounters the player still feels watched via environmental tells (a
door open that wasn't, a light dead that was live, distant audio) — not via
more enemies. **The player must always know whether it is aware of them.**
Ambiguity about *where* is good; ambiguity about *whether it knows* is cruel.
Awareness state is broadcast unambiguously through the audio signature.

### 8.2 The v1 pair — one hunter per channel

Two archetypes at Alpha, chosen so each teaches one pillar channel and neither
needs a single animation clip.

**The Watcher — the light hunter** *(first enemy, M4)*
Tall, hard-edged, faceless. Glides at constant speed — slightly slower than
player walk; it never runs and never stops. Zero clips: translation + two sine
offsets + a yaw bone tracking its interest point. Hunts **contrast, light
deltas, beams, and silhouettes**: a light that just changed interests it more
than one always on; a flashlight beam can be traced to its origin; a doorway
silhouette against a lit room is worse than standing under a steady
streetlight. Signature: a low continuous tone rising in pitch with awareness.
**Its long hard shadow arrives before it does** — the single best P4×P6
interaction in the design, prototyped early. Counterplay: light discipline,
darkness, verticality, doors (it opens them slowly, loudly).

**The Listener — the sound hunter** *(second enemy, M9)*
Near-blind; hears like an array. **Moves when the world is loud, freezes when
it is quiet** — under rain it is fast and the player is masked (double-edged
weather); in a silent night it stands statue-still and hears a pin. Slides and
stops; head-tilt pose is the entire animation budget. Signature: wet clicks
and a hearing-aid squeal when it locks on. Counterplay: freeze with it, throw
sound away from your route, move on carpet, cross under rain. It searches
containers if the last sound came from inside a room.

**Stretch (post-alpha): The Still** — a mannequin that repositions only when
unobserved and unlit; turns the flashlight into a weapon of restraint that
drains your battery and paints you for everything else. Zero animation
(teleport). Third because the Watcher/Listener pair covers the two channels
first.

### 8.3 Perception

Both senses cheap, deterministic, game-thread.

**Sight:** range gate → cone (Watcher 110°) → 3-point trace (head/chest/feet,
partial visibility = partial rate) → **light-level multiplier** → accumulate a
detection meter through Idle → Suspicious → Investigating → Hunting.

**The light model — the critical implementation note.** Never read Lumen for
gameplay: GPU-side, temporal, non-deterministic, untestable. Instead a
CPU-side **`ULightRegistry`** tracks every gameplay-relevant light;
illumination at the player is Σ intensity × attenuation × cone × occlusion
(cached traces), remapped to a visibility multiplier with a darkness floor
(you are never invisible at 2 m). On top of the scalar: **delta bonus**
(recently-changed light state draws attention), **beam trace** (a flashlight
cone is a detectable object pointing home), **silhouette** (backlit in an
aperture = highly visible). Hiding *in* shadow works mechanically for free —
the occlusion trace that blocks the light blocks the detection math. One
deliberate asymmetry: the player reads enemy **cast shadows** as pure
rendering (the Watcher's shadow-first staging), but enemies do **not** detect
the player's cast shadow at Alpha — doing that fairly, without reading as
wallhacks, is a real perception-design problem, deferred to the post-alpha
backlog by name rather than silently absent. Tuned against the rendered image from M4 with a
debug lux overlay until "looks dark" and "is dark" agree (test AI-07) — the
player's visual intuition must transfer, or stealth feels arbitrary.

**Hearing:** direct consumer of §7.2. Propagated loudness above the archetype
threshold yields an investigation target at the event origin *with positional
error* proportional to margin — hunters investigate *near* the noise;
exactness reads as omniscience. Last-known-position discipline: on losing the
player they search outward from where they last *understood* the player to
be, using clues (toggled lights, opened doors). No wallhacks, ever (test
AI-01).

### 8.4 Behavior implementation

Hand-rolled C++ state machine (ADR-0005 logic: Behavior Trees are binary
assets; a seven-state machine is a hundred reviewable lines): `Idle → Patrol →
Suspicious → Investigate → Hunt → Search → Lose → Patrol`. All thresholds,
timers, speeds in CSV. Every transition logs its reason (test AI discipline,
§13.2).

### 8.5 The Director

`UDirectorSubsystem` owns pacing so encounters feel authored. **Pressure
(0–100)** rises with key conditions (+15), elapsed time, noise history, time
spent lit, powered blocks; falls with successful hiding and — sharply — with
cleanly losing a hunter (reward the skill). Pressure drives active hunter
count, spawn proximity, patrol aggression, and ambient dread cues.

**Rest guarantee:** after a resolved encounter, no spawn within 90 s / 60 m.
Without it, procedural pacing degrades into harassment and violates P6.

The Director is deterministic given (seed, **recorded event log**) — QA
reproducibility depends on it. Scope that claim honestly: Chaos physics is
not bit-deterministic across machines, so physics-born noise events cannot be
*re-derived* — they are **recorded as they occur** (the noise-event stream is
part of the run log) and replay at the event level, not the physics level.
Generation (§5.1) is the only layer that promises bit-identical recomputation.

---

## 9. Animation-minimal doctrine

ADR-0003 governs; this section is its application. Agents will repeatedly try
to "add a proper walk cycle." Refuse.

### 9.1 Technique table

| Need | Technique |
| --- | --- |
| Player body | None visible. Arms-only viewmodel at most; held items are static meshes with spring/procedural sway. A hidden low-poly **shadow-proxy body** (casts-hidden-shadow, tracks the camera) makes the player real to the lighting — you cast a shadow past every lamp like everything else does. (MegaLights' screen-space pre-trace is a known artifact source with invisible casters — proxy is validated at M2.) |
| Player feel | Camera craft: spring bob, landing dip, lean offset, breath sway, stamina tremor — all curves in C++, all sliders-to-zero for comfort (§11.4) |
| Doors/drawers/valves | Transform rotation + timeline. A door has open-slow/open-fast/slam as *speeds*, not clips |
| Knock-overs | Chaos impulse. Free motion + free noise event in one |
| Watcher | Glide: translation + sine sway + yaw bone. Zero clips |
| Listener | Slide-and-freeze + one head-tilt pose blend |
| Hide entry | Camera cut + latch sound. No enter animation |
| Vault/climb | Camera-space parabola over 0.35 s (§9.3) |
| Ambient life | Light flicker, Niagara (dust in beams, rain, neon sparks), WPO on cables/tarps — the *world* moves; the entities do not. The contrast is the point |
| Dissolve/materialize | Instead of enter/exit anims where needed |

### 9.2 The authorial test

A player must read the stillness as *wrong*, never as *unfinished*. What sells
it: total commitment (no implied foot contact anywhere), sound doing the work
animation would (no footstep sound + no footstep anim + a low drone =
intentional), and light doing the rest (a slow hard shadow crossing a wall
carries more motion than a walk cycle). External playtest at M6 asks this
question explicitly; if it fails there it is cheap to fix, at M9 it is not.

### 9.3 Traversal

Vault/climb = capsule moved along a computed arc with eased camera pitch/roll.
No mesh moves because no mesh is visible. Ledge detection: two forward traces
+ one down trace; max ledge 140 cm (data-driven).

### 9.4 The WPO trap

Material vertex animation (WPO/VAT) forces per-frame RT geometry updates — a
diffuse, late-arriving cost on a project whose premise is RT. Rule: WPO
geometry is excluded from the RT scene or explicitly budgeted. Never added
casually.

---

## 10. AI-native production pipeline

### 10.1 Authoring surface (ADR-0005)

**C++ and text data are the primary authoring surface.** Blueprint graphs are
binary — undiffable, unreviewable, unmergeable, invisible to text agents. On a
project where AI writes most of the code, Blueprint is where reviewability
goes to die.

| Layer | Authored as |
| --- | --- |
| Gameplay, systems, AI, generation, perception | C++ |
| Tunables | CSV / JSON → DataTables / DataAssets (text committed as source of truth) |
| Materials | One hand-built master; all variants are parameter-driven instances |
| UI | CommonUI in C++; Blueprint widget shells for layout only |
| Blueprint | Thin config wrappers and spawn presets. Nothing else |

No magic numbers in C++; no tunables in Blueprint defaults. An agent should
almost never need the editor open to change gameplay.

### 10.2 Module layout

```
Source/
  Footcandle/          # game module: Player/ Interaction/ Perception/ AI/
                       #   Survival/ World/ Objectives/ Save/
  FootcandleGen/       # generation: pure data out, NO editor deps, NO deps on
                       #   Footcandle — this is what makes headless soak possible
  FootcandleUI/        # CommonUI
  FootcandleEditor/    # editor-only tooling, validators, commandlets
  FootcandleTests/     # automation tests
```

Gameplay Tags are the shared vocabulary (noise sources, surfaces, light
sources, enemy states, room types, interactions); hierarchy established at M0
and append-only after.

### 10.3 Mesh pipeline

```
JSON spec → Blender headless (bpy) → glTF → validator → UE Python import → .uasset
```

For a rigid grid-locked kit, parametric generation beats generative-3D
services: deterministic, regenerable, clean topology, correct pivots. The
agent writes the *script*; the script writes the mesh; the script is the
reviewed artifact. Generative services are for hero props only, through the
same validator. Validator fails (not warns) on: tri budget, pivot, non-applied
transforms, off-grid dims (> 0.5 cm), non-manifold geometry, UVs outside the
palette atlas, missing/complex collision, naming, embedded materials.

**Never commit an asset a script can regenerate. Commit the script.**
(Assets that do get committed follow ADR-0002 / `.gitattributes` LFS rules.)

### 10.4 Audio pipeline

Procedural-first via MetaSounds (built through the Builder API from C++ where
practical, keeping graphs in reviewable code). Generative audio for hero
sounds only: enemy signatures, stingers, extraction. Loudness normalization on
import, validator-enforced — inconsistent loudness wrecks a game whose core
mechanic is hearing.

### 10.5 Unreal MCP (dev-only)

UE 5.8's experimental first-party MCP server lets agents drive the live
editor. Policy: enable with its toolset companion; **localhost only, never on
a shared machine**; commit before every agent editor session; allowlist —
read tools freely, write tools deliberately, destructive tools per-session;
one agent per editor process; used for level population, lighting passes,
material instances, and running tests — never as the primary channel for
gameplay logic (that is §10.1). Stripped from shipping builds. It is
experimental: an accelerator, never a critical-path dependency.

### 10.6 The agent contract

`AGENTS.md` (written at M0) encodes: one system per task · read the system doc
first, write it if missing · determinism rules incl. the TMap-iteration trap ·
no new plugins without approval · no magic numbers · every system ships with
an automation test · no Blueprint logic · perf note required on
rendering/per-frame changes · docs and decision log are part of the change ·
commit scripts, not regenerable assets.

---

## 11. UI, UX, accessibility

Built on **CommonUI** so keyboard/mouse/gamepad all work from day one.

### 11.1 Screens

Boot (legal, **photosensitivity warning** — mandatory for a flicker-heavy
game, brightness calibration) · Main menu (New Run, Continue, **Custom Seed**,
Settings, Controls, Credits, Quit) · Pause (world pauses; single-player horror
respects the doorbell) · Death (attribution sentence, stats, **seed shown**,
Retry Same Seed / New Run) · Victory (stats: time, noise emitted, detections,
battery burned; seed) · Loading (seed display; diegetic radio-tuning bed).

Custom seed entry ships at Alpha: it is free replay value, it makes every bug
report actionable, and P7 already paid for it.

### 11.2 HUD

Near-empty by default; three presets (Full / Minimal / None), default Minimal,
with None positioned as the intended experience. Battery reads through beam
dimming and flicker; stamina through breath and sway; health through
desaturation and limp audio. Compass-edge extraction marker only. No
crosshair; no minimap (§4.7).

### 11.3 The control overlay

A discreet bottom strip, **contextual** — near a door it shows Open/Hold-quiet;
near a hideable, Hide; still, Listen. Auto-fades over the first sessions;
modes Always / Contextual (default) / Off; hold-Tab peek shows the full list
from anywhere; always reflects live remapping.

### 11.4 Settings and accessibility (ship-critical, not optional)

Graphics: preset, resolution, window mode, upscaler (DLSS/FSR/XeSS/TSR/off) +
quality, frame gen toggle (supported hardware), Lumen tier, MegaLights
quality, reflections, fog, FOV 70–110, motion blur off-default,
grain/CA off-default, brightness calibration. Audio: buses, dynamic range
(night mode), output preset, subtitles + size/background. Gameplay: X/Y
sensitivity, invert, hold-vs-toggle for crouch/sneak/lean/listen, HUD preset,
overlay mode, difficulty. Accessibility: **photosensitivity mode** (caps
flicker amplitude/frequency globally — feasible because every light routes
through the registry), head-bob/sway sliders to zero, **visual sound
indicators** (directional pulses — nearly free given §7.1's unified model, and
it makes the core mechanic playable for deaf/HH players), colorblind palettes
(trivial under the palette atlas), high-contrast interactables, hold-to-press
conversion, remapping with conflict detection, text scaling.

Difficulty presets tune hearing thresholds, detection rates, battery drain,
and Director curve — never enemy count (P6).

---

## 12. Save and persistence

A save is `{ Seed, Versions, RunClock, PlayerState, DeltaLog }` — kilobytes.
The city regenerates from the seed; the **delta log** replays divergences:
door states, taken items, moved/broken props, dead lights, powered circuits,
satisfied conditions, hunter states, Director state. Deltas key off the stable
logical IDs from §5.1 — never actor pointers.

Save on: district entry, key condition, save-and-quit. Death ends the run
(retry same seed is the roguelike mercy); there is no mid-run save-scumming.
Generator/content versions are stored; a mismatched save is migrated with
tests or refused with a clear message — never silently regenerated into a
different city.

Determinism tests (§13.3) are therefore save-system tests. If generation
drifts, saves corrupt silently; the byte-comparison gate is what stands
between us and that bug class.

---

## 13. QA and testing

One director is the whole human QA department; automation carries the load.

### 13.1 Layers

| Layer | Tool | Cadence |
| --- | --- | --- |
| Unit | UE Automation | every build |
| Functional in-level | Functional Testing framework | every build |
| Generation soak | headless commandlet (`FootcandleGen` has no editor/actor deps — this is why) | nightly |
| Performance | Gauntlet + CSV profiler, fixed camera routes | every build |
| Visual regression | fixed-camera screenshot diff (incl. dark-scene noise captures) | every build |
| Packaged smoke | cook + boot + seed-gen + screenshot on the dev machine | per milestone |
| Manual | the director, per §13.5 | per milestone |

### 13.2 Named gate tests

Adopted pattern: stable test IDs, tracked per milestone. Highlights:

- **GEN-01..10** — determinism hash, reachability, portal alignment, capsule
  clearance, dependency solvability, invalid-seed safety, retry/fallback
  honesty, stale-async-work cancellation.
- **LGT-01..11** — packaged HWRT active (log-verified, not checkbox-verified);
  no silent software fallback; closed door blocks light; off-screen occluders
  persist; circuit toggle latency (≤ 2 frames direct, ≤ 0.5 s indirect
  settle); no leak that changes a hiding decision; dark-scene noise within
  approved bounds.
- **AUD-01..09** — loudness ordering per surface; portal-rule transmission;
  volume sliders don't touch AI hearing; no runaway physics noise loops;
  caption honesty (never reveal the unheard).
- **AI-01..12** — no live-position cheating; LKP search; door permissions;
  floor separation; hide inspection only with evidence; graphics settings
  never change detection; no cross-chunk teleports; save/load preserves
  knowledge; no unavoidable post-load attacks.

### 13.3 Seed soak

Nightly headless run: **10 000 seeds** through generation + all data-level
validation (§5.6 checks 1–8; the two engine-level checks run as sampled
functional tests instead). Any failure fails the build; every failing seed
joins a permanent regression corpus. Reports distribution stats (buildings, rooms, lights, projected RT
instances, route length) so drift is visible before it is a bug. This single
piece of infrastructure will catch more bugs than all manual play combined —
it is built at M5, before the generator is complex enough to hide things.

### 13.4 Perf gates

Fixed flythroughs: dense street, lit interior, dark interior,
interior/exterior transition, powered block, rain. Fail on: GPU > budget
+10 %, game thread > 8 ms, VRAM > 6.8 GB, RT instances > 20 000, draw calls
> 2 500. Results archived per milestone in `docs/perf/` so regressions have
dates and diffs. Gates run via script on the dev machine from M2; wired to a
self-hosted runner when one exists (GitHub-hosted runners cannot carry UE
builds; repo-hygiene CI stays as-is meanwhile).

### 13.5 The director's manual pass (per milestone)

Three fresh seeds start-to-finish · the regression corpus's five nastiest ·
all four tiers launched and eyeballed · every settings toggle verified to do
something · full gamepad-only pass · one run audio-off, one visuals-minimum ·
one run overlay-off as a stranger would · save/load at three points · the
§9.2 "wrong, not unfinished" question asked of an outsider at M6 and every
milestone after.

---

## 14. Performance practice

Tools: Unreal Insights (primary), `stat unit / gpu / rhi`, `ProfileGPU`, CSV
profiler, `r.RayTracing.Stats` + MegaLights debug views, `stat
SceneRendering` (RT instance count on the debug HUD from M2).

Investigation order when a frame goes over: CPU or GPU (`stat unit`) → if GPU,
per-pass against §6.1 (usual suspects in order: RT scene size, Lumen gather,
MegaLights sampling) → if RT scene, instance count before triangle count →
if game thread, it is the interior streamer or the noise flood — both have
hard caps, check the cap first → if draw thread, batching broke: someone
violated the palette-material discipline.

Profile the packaged build, not PIE, for every gate number. Once per
milestone, profile a thermally-throttled laptop-class run. Never accept "fine
on a 4090" — the gate card is the 3060 Ti.

---

## 15. Milestones — to Alpha

Durations are planning estimates for one director + AI agents; recalibrate
after M1 gives a real velocity signal. **Every milestone has exit criteria; a
milestone is done when the criteria pass, not when the work stops.**

### M0 — Foundation & pipeline (~2 wk)
`.uproject` + module skeleton, plugins, `AGENTS.md`, Gameplay Tag vocabulary,
CI test hook, MCP configured + allowlisted, Blender→validator→import proven
end-to-end on one mesh, automation harness green, capability check (HWRT
present or refuse gracefully) in a packaged scene.
**Exit:** an agent turns a JSON wall-spec into a validated imported `.uasset`
with no human touch; packaged smoke passes with HWRT verified active in logs.

### M1 — Player & interaction slice (~2 wk)
Enhanced Input, camera + spring craft, walk/sneak/sprint/crouch/lean, stamina,
vault, flashlight + battery, interaction system with hold-to-quiet, doors with
speed-based noise, hideables (behind/in/under), throwables, physics noise
props, Listen verb, **player shadow-proxy body** (hidden mesh so the bodiless
first-person player still casts a real shadow). Hand-built two-floor test
building.
**Exit:** traversal feels good enough that you keep walking around for its own
sake (subjective, and the right gate — no later content fixes bad movement).
Grid constants frozen.

### M2 — Lighting bible + DLSS + perf harness (~3 wk)
Full §6 configuration: MegaLights, Lumen HWRT, VSM moon, palette material
system, all four tiers, volumetric fog, the designed-flicker component. **DLSS
4.5 integration: Super Resolution + DLAA + Reflex; Ray Reconstruction spike
on dark/flicker captures (including the undocumented RR × MegaLights
combination); FSR/XeSS/TSR behind the upscaler abstraction.**
Interior/exterior transition test level. Debug HUD (RT instances, lux
sampling). Perf gate scripts + capture routes + dark-scene noise regression
set. The research-flagged prototypes run first: (a) **MegaLights floor
cost in a 3-light dark interior** vs conventional lights, (b) Nanite-vs-not
on one city block incl. VRAM steady state with RT proxies, (c) flicker
fixtures against the denoiser in a black room, (d) the hidden player
shadow-proxy against MegaLights' screen-space pre-trace (invisible casters
are a documented artifact source).
**Exit:** test level holds 60 fps at High tier on the 3060 Ti with budgets
green; all tiers launch; upscaler switching works in-menu; RR adoption
decision recorded; MegaLights sparse-interior decision recorded; Nanite table
verified with data.

### M3 — Noise & audio (~2 wk)
Portal graph + Dijkstra propagation + noise events, MetaSound footsteps with
surface variation, occlusion/reverb from the graph, mix states, `show
SoundEvents`.
**Exit:** closing a door measurably and audibly drops propagated loudness per
the table; propagation unit tests pass; a blind-test listener can point at an
unseen sound source.

### M4 — The Watcher (~3 wk)
Light registry + lux sampling + delta/beam/silhouette semantics, C++ state
machine, navigation, audio signature, awareness broadcasting,
shadow-arrives-first staging, two-strike contact, death attribution.
**Exit:** you can be hunted, hide, break LKP, and escape — and at every moment
you know whether it knows. Lux overlay agrees with the rendered image at the
test positions. AI-01/02/03/07 pass.

### M5 — Building generator + validator (~4 wk)
Stages 5–9 for single buildings: massing, portal list, floor plans, room
graph, apertures, kit meshes (agent-generated, ~30), population, per-building
circuits. Validation suite + headless soak commandlet + regression corpus.
**Save v1**: seed + delta-log round-trip against the stable IDs the generator
now provides (the M6 gate needs working save/load, so it is built here, not
"hardened" into existence at M10). PCG scatter spike (time-boxed, decision
recorded).
**Exit:** 10 000 seeds of buildings validate clean; a generated building is
indistinguishable in play from the M1 hand-built one; generation < 5 ms per
building; generate → mutate → save → reload → compare passes.

### M6 — VERTICAL SLICE — GO/NO-GO (~3 wk)
One generated multi-floor building + street, one Watcher, one key condition
and one extraction commit (**minimal hand-wired versions** — the systemic
versions are M8's job and replace these), full loop, minimal shell to launch
it. Three external playtesters.
**Exit (all required):** complete run on a fresh seed; 60 fps on the 3060 Ti,
budgets green; strangers finish without direction and describe the experience
in terms of light and sound; the Watcher reads as intentional (§9.2); save/
load mid-run works. **If this gate fails, M7 does not start** — the city
multiplies whatever the slice is.

### M7 — City generator + streaming (~5 wk)
Stages 1–4 + 10 at district scale, three-tier streaming, shell merging,
exterior lighting + power grid topology, city-scale validation, RT instance
enforcement, navmesh strategy at scale.
**Exit:** a full district generates, streams hitch-free, and holds budgets
through a 10-minute route across dense/sparse/interior/rooftop; 10 000-seed
city soak clean.

### M8 — Escape loop + Director + power grid (~3 wk)
Key conditions, substation/block/building/circuit gameplay, extraction commit
windows, Pressure, rest guarantee, run stats, seed screens.
**Exit:** run lengths land 25–45 min across 20 runs; encounter cadence per
§8.1; Director deterministic given (seed, action log); powering a block
visibly and tactically transforms it.

### M9 — The Listener + weather + breadth (~4 wk)
Second archetype, rain (noise floor + wet materials + reflections), district
palette/kit variants, more objective types, ambient dread cues, environmental
tells.
**Exit:** ten consecutive seeds are distinguishable in memory afterward — the
replayability test. The Listener creates a *different* game from the Watcher,
and rain visibly changes player routing decisions.

### M10 — Alpha shell (~3 wk)
All screens, full settings, control overlay, remapping, accessibility set
(photosensitivity mode, visual sound indicators, sliders-to-zero), save/load
hardening, packaged-build pass on all tiers.
**Exit = ALPHA:** the §13.5 checklist passes end-to-end; a stranger can
install a packaged build, complete a run, share a seed, and nothing on the
tech-risk register (§16) remains unretired. Total: ~34 weeks nominal — call
it **8–10 months** with slippage honesty; M7 is the variance driver.

**Post-alpha backlog (explicitly deferred):** The Still, electrical-storm
weather, more extraction families (harbor ferry, subway gate), daily seed,
landmark hand-authored buildings injected at generator slots, **enemy
perception of the player's cast shadow** (a fair, readable version of "it
saw your shadow round the corner" — the rendering already supports it; the
perception model is the hard part), **borrowed-hardware validation** (Frame
Generation on 40/50-series; min-spec pass on RDNA2 / Arc / RTX 20-series —
the published min spec stays provisional until this runs), Steam
integration/store prep, localization, third-party playtest rounds, ship
hardening (M11/M12-class work).

---

## 16. Risk register

| # | Risk | L | I | Mitigation |
| --- | --- | --- | --- | --- |
| R1 | RT instance count explodes at city scale | High | Critical | Debug HUD from M2, validator check from M5, CI gate from M7, generation-time merging, culling radius |
| R2 | Determinism breaks silently → saves corrupt, bugs unreproducible | High | Critical | CI hook at M0; byte-comparison gate live with first generator code (M5); RNG/TMap lint; seed soak nightly |
| R3 | 8 GB VRAM ceiling breached | Med | High | 6.8 GB gate in perf harness from M2; palette-atlas art keeps texture memory trivial; RT culling |
| R4 | MegaLights noise in near-black scenes (our whole game) | Med | High | Dark-scene capture regression from M2; larger dimmer emitters; Ray Reconstruction evaluation targeted at exactly this |
| R5 | Procedural interiors feel samey by run ten | High | High | District kits/palettes, room-type population, objective variety; explicit M9 memory test; landmark injection post-alpha |
| R6 | Animation-minimal reads unfinished | Med | High | Full §9 commitment; external playtest question at M6 while it is still cheap |
| R7 | Gameplay light model diverges from rendered truth | Med | High | Lux overlay from M4, tuned-together budgets, automated agreement checks at fixed captures |
| R8 | AI-generated code erodes architecture | High | Med | AGENTS.md contract, one-system tasks, system docs, human architectural review at milestone boundaries |
| R9 | MCP instability stalls pipeline | Med | Low | Never critical-path; C++/CSV is the authoring surface |
| R10 | Scope creep from "open world" | High | High | §4.8 list + 800 m bound + pillars; every proposal checked against P-list |
| R11 | DLSS/vendor plugin churn | Med | Med | Pinned versions, upscaler abstraction, M2 capture set as upgrade regression |
| R12 | UE6 migration pressure mid-project | Med | Med | Ship on 5.8.x; keep deprecation-clean; port is a separate future project |
| R13 | Single-human QA blind spots | High | Med | Soak + named gates carry the load; 3–5 external playtesters from M6 onward |

---

## 17. Decision log

Append-only. Every substantive roadmap change adds a line.

| # | Date | Decision | Rationale |
| --- | --- | --- | --- |
| 1 | 2026-08-31 | UE 5.8.2 pinned; ship on 5.8.x | Installed, current, MegaLights production-ready; UE6 is a separate project |
| 2 | 2026-08-31 | RTX 3060 Ti 8 GB is the perf reference | It is the actual dev machine; a gate you cannot run is a wish. 12 GB 3060 numbers in proposals were aspirational |
| 3 | 2026-08-31 | 100 % real-time lighting, HWRT required, no baked path ever | ADR-0004; P4. Supersedes art-direction's baked-fallback caveat |
| 4 | 2026-08-31 | C++-first, text-source-of-truth authoring | ADR-0005; AI production model demands reviewable diffs |
| 5 | 2026-08-31 | Custom C++ deterministic generator; PCG scatter-only | ADR-0006; determinism, headless soak, reviewability |
| 6 | 2026-08-31 | DLSS 4.5 via official UE plugin (v8.7.2 pinned); abstraction + FSR/XeSS/TSR fallbacks; FG never gates perf | ADR-0007; latest-DLSS requirement + 30-series reality |
| 6b | 2026-08-31 | Stock Epic 5.8.2, **no NvRTX fork** | RR now ships in the stock DLSS plugin; NvRTX exclusives (Mega Geometry, RTXDI, path tracing) don't serve this game; fork-carrying cost is real for a team of one |
| 7 | 2026-08-31 | Three survival resources; no hunger/thirst | Clock-watching ≠ tension at 35-minute runs |
| 8 | 2026-08-31 | Two-strike death + attribution sentence | Stakes without cheap-grab restarts; death teaches the system |
| 9 | 2026-08-31 | Death ends run (retry same seed); saves at checkpoints + quit | Roguelike stakes; P7 makes retry-same-seed free |
| 10 | 2026-08-31 | Watcher (light) + Listener (sound) as the Alpha pair; The Still deferred | One teacher per channel; flashlight-economy enemy is a great third, not a first |
| 11 | 2026-08-31 | Power grid is a core system, not a stretch feature | Strongest single idea across proposals: makes light itself the district-scale risk economy |
| 12 | 2026-08-31 | Always night; weather as noise-floor modifier; rain at M9 | Lighting identity + systemic value per feature-dollar |
| 13 | 2026-08-31 | ~800 × 800 m district, dense and vertical | "Open world" resolved toward density; RT scene and R10 both demand it |
| 14 | 2026-08-31 | Fiction stays minimal and unexplained | Environment carries tone; no dialogue/lore pipeline; the event is never named |
| 15 | 2026-08-31 | Windows/Steam only for v1 | Console moves every budget; not now |
| 16 | 2026-08-31 | Working title FOOTCANDLE | Photometric unit; literally footsteps + candlelight — the two channels in one word |
| 17 | 2026-08-31 | Independent review pass applied before adoption | 14 findings fixed, notably: no MegaLights-off tier exists (a non-RT shadow floor would violate ADR-0004 *and* be slower); save v1 scheduled at M5 (the M6 gate depends on it); validation split into data-level (soak) vs engine-level (functional) checks; Director determinism scoped to the recorded event log, not physics replay; off-tier hunters simulate as graph-walks on `FCityData` |
| 18 | 2026-08-31 | Player casts a real shadow via a hidden shadow-proxy body (M1, validated M2); enemy detection of player cast shadows deferred post-alpha by name | Arms-only player = no mesh = no shadow without a proxy; the rendered world must treat the player as geometry. AI shadow-reading is a fairness problem, not a rendering one |
| 19 | 2026-08-31 | Bindings: **Interact F, Flashlight T, Lean Q/E** (was Interact E / Flashlight F) | The E-key conflict had to break one way in week one; decades of Q/E-lean muscle memory wins. All rebindable at M10 regardless |
| 20 | 2026-08-31 | M1 grid constants frozen as speced (§5.3); M1 exit passed via 11/11 scripted behavior smoke + 6-station visual tour | The "traversal feels good" subjective gate awaits the director's hands-on pass; mechanics are evidence-verified (FCM1Smoke) |
| 21 | 2026-08-31 | **DLSS 4.5 plugin v8.7.2 installed and verified live**: NGX init Success on D3D12, RR transformer model loads, Frame Gen self-disables on 30-series. Vendored plugins gitignored; `install-dlss.ps1` is the pinned source of truth | Measured on the 3060 Ti (853×480 internal): DLSS-Q GPU ≈ TSR at equal internal res (3.3–3.6 ms) — adopted as NVIDIA default |
| 22 | 2026-08-31 | **Ray Reconstruction: per-tier quality option, default OFF on the 3060-class High tier** — costs +2.8 ms GPU on the reference card, visibly cleaner denoising (wall grain resolved). Re-evaluated on city content at M7 | RR must displace ~3 ms elsewhere to fit the §6.1 budget; render thread relief (−1.4 ms) is real but not enough. Exactly the evidence-based per-tier adoption ADR-0007 called for |
| 23 | 2026-08-31 | **MegaLights sparse-interior floor cost measured ≈ 0 ms** (on/off delta within noise, 4–5-light interior, 3060 Ti) — MegaLights stays on project-wide, no per-room hedging | The research-flagged "riskiest unknown" resolves benignly; re-verify at city light counts (M7) |
| 24 | 2026-08-31 | Night exposure locked via clamped auto-exposure (min 0.02 / max 0.18 / bias −0.4) + moon at 0.08 lux, both capture-tuned | 0.4 lux moon read as dusk; darkness must stay dark and practicals must dominate (P4) |
| 25 | 2026-08-31 | Lux registry works in **real lux**, saturating at 30 (street-lit) — replaces an arbitrary 0.004 normalization | Physical units keep "looks dark"/"is dark" tunable against renders; found via M4 smoke |
| 26 | 2026-08-31 | **Engine gotcha, encoded in code comments**: spawned `ASpotLight`/light actors carry component-level default rotations — always `SetWorldRotation` on the *component* | The streetlight had aimed south-and-down since M1; every capture "worked" until the perception model disagreed with the image — exactly the AI-07 divergence the registry exists to catch |
| 27 | 2026-08-31 | Watcher detection: fully-lit-in-direct-view ≈ 3.3 s to Hunt (rate 3.0/s·vis); measured-in-smoke | 11 s (initial 0.9 rate) was too forgiving; tuned via FCM4Smoke timing |
| 28 | 2026-08-31 | **All ten milestone cores (M0–M10) implemented and evidence-verified in the founding session** — scripted behavior smokes (M1 14/14 · M4 6/6 · M5 7/7×3 seeds · M6 6/6 · M7 3/3 · M8 13/13 · M9 6/6 · M10 4/4), unit+soak suites 6/6 green (1000-seed building + 100-seed city), packaged HWRT evidence, per-station perf sampling, every visual milestone eyeballed from captures | Remaining to the full Alpha bar, tracked here: the human gates (traversal feel M1, three external playtesters M6); polish passes (palette master material + kit meshes into scenes, MetaSounds audible layer, CommonUI skin over the code-drawn shell, full settings menu incl. photosensitivity/visual-sound-indicator toggles — the hooks exist); scale-up (road-graph organics + districts, RT-instance CI gate at city scale, async interior amortization, 10 000-seed full-city soak, packaged city run); and the post-alpha backlog (§15) |
| 29 | 2026-08-31 | **Every gameplay light is a visible, destroyable fixture** — `AFCLightFixture` (styles: CeilingBulb / Streetlight / TV / EmergencyLED) replaces every raw point/spot spawn in the play scenes; fixtures are interactable (F toggles), circuit-linkable (switch/breaker unchanged — they drive the same `ULightComponent`), and breakable by prop impact (glass noise 95, permanent dark, registry delta). Generated rooms get deterministic variety (rooms `Id % 5 == 3` glow from a guttering floor TV instead of a ceiling bulb). The flashlight gains a visible torch body. Registry occlusion traces ignore the light's own fixture actor. Verified: M1 grew to 24/24 incl. 7 fixture checks; full regression green (M4–M10, Shell, units); address + city captures eyeballed | Director's ask: "every light source is a real thing in the world — a bulb, a headlight, a TV — that can be turned off/on/destroyed." Also P4 embodied: the light economy now has physical targets |

---

## Appendix A — proposal review (what was taken, what was left)

Five proposals were reviewed from `My PS/uegt5-sh/`. This plan is a synthesis
with its own calls; provenance below so nothing looks accidental.

**Claude ("LOW-LIGHT")** — the strongest technical architecture; adopted
heavily: text-source rule, pure-function generation + delta-log saves,
portal-graph propagation, CPU light registry, RT-instance budget discipline,
MegaLights lean-in, milestone gate structure incl. the vertical-slice
go/no-go, risk register shape. *Rejected/changed:* 12 GB VRAM assumptions
(dev card is 8 GB); The Still demoted to post-alpha; its M0–M12 span trimmed
to an Alpha-terminated plan; DLSS treated as first-class early integration
rather than a plugin line-item.

**Grok ("SODIUM")** — the strongest design texture; adopted: power grid as
core, weather-as-noise-floor, sensory-specialist hunter split, portal
contract framing, death-sentence attribution, city-as-puzzle navigation
(paper schematic, no minimap), "intended look" tier philosophy, prompt
contract shape. *Rejected/changed:* City Sample PCG as the generator base
(collides with determinism + text-authoring; ADR-0006), Crawler archetype
deferred, 3-archetype v1 roster trimmed to 2, StateTree in favor of C++ SM.

**GPT ("Survival Escape")** — the strongest process discipline; adopted:
named gate tests (GEN/LGT/AUD/AI), stable logical save identity,
generation-failure honesty (retry codes, no silent fallback), packaged-build
verification culture, evidence rules (never report upscaled as native).
*Rejected:* its premise of building inside the `uegt1` repo (this is a fresh
repo; its M0 "isolate the old mode" work does not exist here); TSR-first
upscaling (DLSS is an explicit project requirement).

**Gemini ("City Escape")** — corroborated the consensus stack (Lumen HWRT +
MetaSounds + Chaos noise + rare stalkers); its light-based AI vision cone and
diegetic HUD notes align with choices above. Nothing unique adopted beyond
corroboration; its PCG-first generator and 12-week timeline were rejected as
under-scoped.

**Meta ("PROJECT ECHO")** — rejected in most specifics (blanket Nanite,
DLSS Frame Gen "on by default" — impossible on the reference card, 11-week
ship plan, WFC-in-construction-script interiors), but its insistence on
volumetrics-always and its Listener echolocation flavor influenced §6.9 and
the Listener's telegraph design. The stop-motion enemy-animation idea was
considered and set aside as conflicting with the glide/freeze aesthetic.

## Appendix B — research notes (verified 2026-08-31)

Independent web research run before this roadmap was finalized. Facts below
are sourced; items marked ⚠ could not be verified against a primary source
and are treated as open questions, not assumptions.

### Engine (UE 5.8 / 5.8.2)

- 5.8.0 shipped 2026-06-17; **5.8.2 hotfix 2026-08-25** (installed and
  pinned). ⚠ "5.8 is the last major UE5 release, UE6 early access late 2027"
  is widely reported from State of Unreal 2026 but not Epic-quoted — R12
  stands regardless.
- **MegaLights: Production-Ready in 5.8** ([release notes](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-5-8-release-notes)).
  Constant-cost model; quality (not cost) degrades with per-pixel light
  complexity; per-light shadow choice with **ray-traced default** ("no extra
  cost per light, correct area shadows"); Epic's recommended split = VSM for
  the strong directional + MegaLights RT shadows for locals — adopted
  verbatim (§6.2). 5.8 adds first-person viewmodel support, IES on
  volumetrics, lighting channels. Known gotchas: lights inside geometry,
  dense overlaps (blur/ghosting), screen-trace artifacts
  ([MegaLights doc](https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine)).
  ⚠ No official base-cost ms figure exists; no shipped MegaLights title yet;
  sparse-dark-interior behavior unpublished → first M2 measurement.
- **Lumen:** HWRT is the default/recommended path in 5.8, including 60 fps
  console targets; official budget ~4 ms GPU at 60 fps at 1080p internal
  ([performance guide](https://dev.epicgames.com/documentation/unreal-engine/lumen-performance-guide-for-unreal-engine)).
  **Lumen Lite** confirmed (irradiance fields, ~2× faster than High, ⚠ Beta) —
  Medium tier only. ⚠ The "software tracing leaks through thin walls" claim
  is community consensus + inference from the merged-distance-field
  representation, not current doc text; the design conclusion (HWRT on all
  gameplay tiers) holds on accuracy grounds either way.
- **RT scene:** TLAS rebuilds per frame ∝ instances; official guidance
  ≤ 100 k instances after culling (30 fps console), ~20 k dynamic
  primitives/frame for 60 fps; culling defaults mode 3 / 10 000 uu / 1°
  (City Sample: 15 000 / 0.5°); Nanite meshes trace **streamed RT proxies**,
  and per-platform "Generate Nanite Fallback Meshes" can be disabled in
  favor of proxies, "usually saving a lot of memory"; WPO ignored in RT by
  default, and WPO on instanced components ⇒ per-instance BLAS ("extreme
  cost") ([RT performance guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/ray-tracing-performance-guide-in-unreal-engine)).
  5.8 defaults reference-based BLAS residency on (meshes without LODs never
  evicted). The 5.6/5.7 Nanite+RT VRAM leak is fixed in 5.8 — but watch
  `stat RHI` over long sessions anyway.
- **VSM:** designed around Nanite; non-Nanite geometry markedly more
  expensive; WPO invalidates pages per frame
  ([VSM doc](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine)) —
  a load-bearing input to the §6.5 Nanite-on-kit hypothesis.
- **Unreal MCP:** confirmed first-party Experimental plugin + toolset
  registry; loopback-only, no auth, serial game-thread execution
  ([doc](https://dev.epicgames.com/documentation/unreal-engine/unreal-mcp-in-unreal-editor)) —
  §10.5 policy matches.
- **City Sample PCG:** rebuilt with in-engine PCG + MCP workflows, free on
  Fab — **exteriors only, no interiors, collision off by default**
  ([doc](https://dev.epicgames.com/documentation/unreal-engine/city-sample-pcg-for-unreal-engine)).
  Useful as reference material; not a generator base (ADR-0006 stands).
- Tom Looman, *UE 5.8 Performance Highlights* (2026-08-27): MegaLights
  penumbra-only multi-ray (0.3–1 ms console savings), ~20 % light-sampling
  culling, VSM invalidation budgeting, shader-permutation reductions
  ([article](https://tomlooman.com/unreal-engine-5-8-performance-highlights/)).

### DLSS / upscalers

- Current branding **DLSS 4.5** (CES 2026). Support matrix: SR / RR / DLAA —
  all RTX (20–50); FG — 40/50; MFG + Dynamic MFG (6X) — 50 only
  ([NVIDIA DLSS](https://www.nvidia.com/en-us/geforce/technologies/dlss/)).
  2nd-gen transformer SR shipped Jan 2026; **2nd-gen transformer RR official
  release Sept 2026** (driver-override live since Gamescom Aug 2026).
  Reflex Low Latency: all RTX. ⚠ Reflex 2 Frame Warp still unreleased —
  planned around nothing.
- **Official DLSS plugin v8.7.2 (Jul 2026) supports UE 5.8**; Streamline
  2.11.1 + NGX 310.6.0; the stock plugin exposes SR, DLAA, **RR**, FG, MFG,
  Reflex, NIS ([developer.nvidia.com/rtx/dlss](https://developer.nvidia.com/rtx/dlss)).
  **NvRTX no longer needed for RR** — its 5.8 branch is "preview" and its
  exclusives (Mega Geometry, RTXDI, path tracing, SER) don't serve this
  game → decision 6b.
- RR integration: replaces engine RT denoisers (e.g.
  `r.Lumen.Reflections.BilateralFilter=0` at ini level per NVIDIA guidance).
  ⚠ RR × MegaLights combination officially undocumented → M2 spike.
- Transformer SR VRAM ≈ 86 MB @1080p (Tom's Hardware, Jun 2025); ⚠ RR VRAM
  unpublished — measured at M2.
- Fallbacks current: **AMD FSR plugin for UE 5.8** (Aug 2026; FSR 4.1.1
  upscaling, ML-based, + FG 4) ([GPUOpen](https://gpuopen.com/learn/amd-fsr-plugin-updated-for-unreal-engine-58/));
  **Intel XeSS plugin 3.1.0** for 5.8 (⚠ XeLL broken on 5.8.0/5.8.1 —
  verify on 5.8.2).

### Performance evidence (mid-range RTX, 8 GB)

- Strongest existence proof: **The Witcher 4 UE demo (Nanite + HW Lumen +
  MegaLights + runtime streaming) at DF-verified 60 fps on base PS5**
  (June 2025) — base PS5 RT throughput ≈ at/below a 3060 Ti, so §6.1's
  1080p60 + DLSS Quality target is grounded, not hopeful.
- 8 GB pressure symptoms (community): out-of-video-memory crashes, sudden
  texture blur, paging hitches; scalability presets barely move fixed
  buffers — the palette-atlas art direction (near-zero texture pool) is our
  structural advantage here.
- Dark-scene temporal denoising: fast per-frame flicker breaks accumulation
  (noisy ghosting patterns — multiple community reports through 5.7); Epic
  acknowledges MegaLights moving-object ghosting as under investigation.
  Mitigations adopted: multi-frame designed flicker (§6.2), RR evaluation,
  `r.Lumen.ScreenProbeGather.Temporal.*` tuning, black-room fixture tests.
- Deep reference for later tuning: Narkowicz & Costa, *MegaLights:
  Stochastic Direct Lighting in UE5*, SIGGRAPH 2025 Advances
  ([PDF](https://advances.realtimerendering.com/s2025/content/MegaLights_Stochastic_Direct_Lighting_2025.pdf)).
