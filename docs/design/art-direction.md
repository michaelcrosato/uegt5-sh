# Art direction

> Living document. Numbers here are **starting budgets**, not laws — but change
> them deliberately and update this file when you do.

## The look in one sentence

Hard-edged low-poly geometry, flat or lightly-shaded surfaces, colour carried by
palette and light rather than texture detail, read from a first-person camera at
eye height.

## Why low poly (beyond taste)

It is a production strategy as much as a style. Every constraint below exists to
keep a small team shipping:

- No high-poly sculpt → no bake → no PBR texture set per asset.
- Flat shading hides topology sins; you can model an asset in minutes.
- Small textures and few materials mean the game runs on anything.
- Silhouette does the storytelling, so assets stay readable at any distance.

The corollary: **when the art looks wrong, the fix is almost always silhouette,
proportion or colour — not more triangles.**

## Geometry

| Asset class | Triangle budget | Notes |
| --- | --- | --- |
| Small prop | 100 – 500 | Crates, bottles, debris |
| Large prop / set dressing | 500 – 2,000 | Furniture, machinery |
| Architectural module | 200 – 1,500 | Built to a grid, see below |
| Character | 1,500 – 4,000 | Silhouette over detail |
| First-person viewmodel (arms + held object) | 3,000 – 8,000 | The only thing the player ever sees up close — it gets the budget |

Rules:

- **Hard edges by default.** Split normals at every edge unless a surface is
  genuinely meant to read as curved. No smoothing groups agonising.
- **Grid-snap architecture.** Author to a modular grid (start at 100 uu, i.e. 1 m)
  so level pieces tile without seams or Z-fighting.
- **No bevels for their own sake.** A bevel that survives at 1080p on a distant
  wall is wasted; a bevel on a viewmodel is worth it.
- **Delete what the player cannot see.** Backfaces, interiors of sealed props.

## Materials & texture

- Aim for a **small number of shared master materials** with instance parameters,
  not a bespoke material per asset. Draw calls are the budget that actually hurts.
- Prefer **vertex colour** and **palette/gradient textures** (a 256×256 atlas
  shared across dozens of meshes) over per-asset texture sets.
- Roughness/metallic as flat constants where possible. If an asset needs a
  normal map, question whether it needs the geometry instead.
- Texture sizes: 512 max for world props, 1024 for viewmodels, 256 for UI atlases.

## Colour & lighting

- The palette is a **fixed, named set**. Add to it deliberately; do not eyedrop a
  new blue because it looked nice in one scene.
- Lighting does the mood work: strong key direction, coloured fill, and generous
  fog. Low-poly reads badly under flat neutral light.
- Lumen is fine for a low-poly game and saves lightmap authoring — but validate
  performance early, and be ready to fall back to baked lighting if the target
  hardware demands it. Record the decision as an ADR if you switch.

## First-person specifics

- **Eye height:** 160–180 uu. Pick one and never change it — it silently
  recalibrates every space you have built.
- **FOV:** 90–100° horizontal. Exposed as a player setting; author for the
  default, sanity-check the extremes.
- **Viewmodel FOV is separate** from world FOV, otherwise held objects distort
  grotesquely at high FOV.
- **Near clip plane** is tight (~1 uu) so the viewmodel does not clip into walls.
  Alternatively render the viewmodel in a separate pass — decide this early, it
  affects every held asset.
- The player has no visible body below the neck unless a specific design need
  emerges. That is a deliberate scope decision, not an oversight.

## Animation

The pillar is **minimal**, and the ordering below is the decision procedure:

1. **No motion.** Does this need to move at all?
2. **Transform / curve driven.** Timelines, `FInterpTo`, rotators, material
   parameter animation. Doors, lifts, pickups, hover, spin, weapon bob and sway
   — all of this is code, not clips.
3. **Short hand-authored clips.** Only when 1 and 2 fail. Keep them under a
   second where possible, few keyframes, snappy easing. Stylised low-poly
   tolerates — and often benefits from — deliberately non-smooth motion.
4. **Full skeletal state machine.** Requires a real justification and an ADR.

Never: mocap, retargeted marketplace animation packs, or a locomotion blendspace
for a first-person game that shows no legs.

## Audio (placeholder)

Noted here only so it is not forgotten: a stylised look needs stylised audio.
Sparse, punchy, synthetic. To be expanded when audio work starts.

## Reference

_Add reference images and links here as the style firms up. Keep the list short
and opinionated — twenty references is not a direction, it is indecision._
