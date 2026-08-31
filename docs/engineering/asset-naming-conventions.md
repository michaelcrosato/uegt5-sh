# Asset naming & content structure

Based on the community-standard [Unreal Engine style guide](https://github.com/Allar/ue5-style-guide),
trimmed to the asset types this project actually uses.

## The pattern

```
Prefix_AssetName_Variant
```

- `SM_Crate_01`
- `M_Rock_Base`, `MI_Rock_Mossy`
- `T_Palette_Main_D`
- `BP_PlayerCharacter`

Rules:

- `PascalCase`, no spaces, no hyphens, no double underscores.
- Numeric variants are **two digits**: `_01`, not `_1`.
- The name describes the thing, not where it is used. `SM_Crate_01`, not
  `SM_Level2Crate`.
- Never rename an asset outside the editor — Unreal's redirectors exist for a
  reason, and a file-system rename breaks every reference.

## Prefixes

| Type | Prefix | Example |
| --- | --- | --- |
| Static Mesh | `SM_` | `SM_Barrel_01` |
| Skeletal Mesh | `SK_` | `SK_Hands` |
| Skeleton | `SKEL_` | `SKEL_Hands` |
| Physics Asset | `PHYS_` | `PHYS_Hands` |
| Material | `M_` | `M_LowPoly_Base` |
| Material Instance | `MI_` | `MI_LowPoly_Rock` |
| Material Function | `MF_` | `MF_Triplanar` |
| Material Parameter Collection | `MPC_` | `MPC_Global` |
| Texture | `T_` | `T_Palette_Main_D` |
| Blueprint (Actor) | `BP_` | `BP_Door` |
| Blueprint Interface | `BPI_` | `BPI_Interactable` |
| Blueprint Function Library | `BPFL_` | `BPFL_MathHelpers` |
| Actor Component | `AC_` | `AC_Interaction` |
| Widget Blueprint | `WBP_` | `WBP_HUD` |
| Enum | `E_` | `E_WeaponState` |
| Struct | `F_` | `F_WeaponStats` |
| Data Asset | `DA_` | `DA_WeaponConfig` |
| Data Table | `DT_` | `DT_Items` |
| Curve | `Curve_` | `Curve_RecoilPitch` |
| Level / Map | `L_` | `L_Atrium` |
| Level (persistent) | `L_` + `_P` | `L_Facility_P` |
| Niagara System | `NS_` | `NS_Dust` |
| Niagara Emitter | `NE_` | `NE_Sparks` |
| Sound Wave | `S_` | `S_Footstep_01` |
| Sound Cue | `SC_` | `SC_Footstep` |
| Sound Attenuation | `ATT_` | `ATT_Small` |
| Animation Sequence | `A_` | `A_Hands_Fire` |
| Animation Montage | `AM_` | `AM_Hands_Reload` |
| Animation Blueprint | `ABP_` | `ABP_Hands` |
| Input Action | `IA_` | `IA_Move` |
| Input Mapping Context | `IMC_` | `IMC_Default` |
| Game Mode / State | `GM_` / `GS_` | `GM_Default` |

## Texture suffixes

| Suffix | Map |
| --- | --- |
| `_D` | Base colour / diffuse |
| `_N` | Normal |
| `_M` | Packed mask (R/G/B = separate channels) |
| `_E` | Emissive |
| `_A` | Alpha / opacity |

Given the low-poly direction, most assets need only `_D` — often just a shared
palette texture. A texture set with four maps is a smell; ask whether it is
earning its place.

## Content folder structure

Organise by **domain**, not by asset type. `Content/Weapons/Pistol/` beats
`Content/Meshes/Weapons/Pistol/` — related assets stay together, so a whole
feature can be moved or cut in one operation.

```
Content/
  _Dev/                 # Scratch, greybox, test maps. Never referenced by shipping content.
  Core/                 # Game mode, player, input, shared systems
  Characters/
  Weapons/
  Environment/
    Modules/            # Modular architecture kit
    Props/
    Materials/
  UI/
  VFX/
  Audio/
  Maps/
  Shared/               # Master materials, palettes, curves used across domains
```

The leading underscore on `_Dev` sorts it to the top of the content browser and
marks it as excluded from packaged builds.

## Cross-references

- Migrating assets between projects drags dependencies. Check
  **Reference Viewer** before moving anything out of `_Dev`.
- Nothing in a shipping folder may reference `_Dev`. Validate with
  **Asset Audit** before packaging.
