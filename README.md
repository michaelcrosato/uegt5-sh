# uegt5-sr

A first-person, low-poly game built in **Unreal Engine 5**.

> **Status:** pre-production. The repository is set up; the Unreal project itself
> has not been generated yet. See [Getting started](#getting-started).

---

## Pillars

These three constraints drive every technical and artistic decision in the repo.
When a choice is ambiguous, the one that better serves a pillar wins.

| Pillar | What it means in practice |
| --- | --- |
| **First person** | The camera is the player's eyes. No third-person cinematics, no full-body IK rig to maintain. Arms-only viewmodel; the world is authored to be read at eye height. |
| **Low poly** | Flat/vertex-lit shading, hard-edged silhouettes, small triangle budgets, palette-driven colour instead of high-res texture detail. Readability over fidelity. |
| **Minimal animation** | Few, short, hand-authored clips. Prefer code-driven motion (curves, procedural bob, transforms) over skeletal animation. No mocap, no complex state machines. |

A longer treatment lives in [`docs/design/art-direction.md`](docs/design/art-direction.md).

---

## Getting started

### Prerequisites

| Tool | Version | Notes |
| --- | --- | --- |
| Unreal Engine | 5.x | Install via the Epic Games Launcher |
| Visual Studio | 2022 | With the **Game development with C++** workload and the *Unreal Engine installer* component |
| Git | 2.40+ | |
| Git LFS | 3.x | **Required** — the repo will not check out correctly without it |

### Clone

Git LFS must be initialised *before* the first clone, or you will get pointer
text files instead of assets:

```bash
git lfs install
git clone https://github.com/michaelcrosato/uegt5-sr.git
cd uegt5-sr
```

Windows only, and worth doing once globally — Unreal's `Content/` paths blow
past the 260-character limit:

```bash
git config --global core.longpaths true
```

### First build

Once the `.uproject` exists:

1. Right-click the `.uproject` → **Generate Visual Studio project files**.
2. Open the generated `.sln`, set the configuration to **Development Editor**, build.
3. Launch the editor from Visual Studio (F5) or by double-clicking the `.uproject`.

---

## Repository layout

```
.
├── Config/              # UE ini files — committed, hand-edited sparingly
├── Content/             # UE assets (.uasset/.umap) — all Git LFS
├── Source/              # C++ game modules
├── Plugins/             # First-party plugins (Source/ + Content/ committed)
├── docs/
│   ├── design/          # Game design doc, art direction
│   ├── engineering/     # Coding standards, naming, source-control workflow
│   └── adr/             # Architecture decision records
├── tools/scripts/       # Repo maintenance and CI helper scripts
├── .github/             # Issue/PR templates, CI workflows
├── .gitattributes       # LFS routing + line endings  ← read before adding a new file type
└── .gitignore           # Everything the engine regenerates
```

Directories the engine generates (`Binaries/`, `Intermediate/`, `Saved/`,
`DerivedDataCache/`, `Build/`) are ignored and must never be committed.

---

## Conventions

| Topic | Document |
| --- | --- |
| Asset & C++ naming | [`docs/engineering/asset-naming-conventions.md`](docs/engineering/asset-naming-conventions.md) |
| C++ style, module layout | [`docs/engineering/coding-standards.md`](docs/engineering/coding-standards.md) |
| Branching, LFS, binary merge conflicts | [`docs/engineering/source-control.md`](docs/engineering/source-control.md) |
| Why things are the way they are | [`docs/adr/`](docs/adr/) |

Short version:

- **Branches:** `main` is always buildable. Work on `feat/`, `fix/`, `art/`, `docs/`, `chore/` branches; merge via PR.
- **Commits:** [Conventional Commits](https://www.conventionalcommits.org/) — `feat(weapons): add hitscan trace`.
- **Binary assets cannot be merged.** Coordinate before two people touch the same `.uasset`. See the locking section in the source-control doc.

---

## Adding a new binary file type

If you introduce a file type that is not already in `.gitattributes`, add it
there **before** committing the file, otherwise it lands in git history as a raw
blob and bloats every future clone:

```bash
git lfs track "*.newext"      # appends to .gitattributes
git add .gitattributes
```

Already committed something the wrong way? `git lfs migrate import --include="*.newext" --everything` rewrites history — coordinate with everyone first.

---

## Licence

Copyright © 2026 Michael Crosato. All rights reserved. See [LICENSE](LICENSE).
