# CLAUDE.md

Project guide for Claude Code. Read `README.md` for the human-facing overview.

## What this is

A first-person, low-poly game in **Unreal Engine 5**, with **minimal animation**.
Those three constraints are design pillars, not incidental details — check
proposed work against them before writing code or authoring assets:

- **First person** — arms-only viewmodel, no third-person rig, no full-body IK.
- **Low poly** — flat/vertex-lit shading, small tri budgets, palette-driven
  colour rather than texture detail. Readability beats fidelity.
- **Minimal animation** — prefer code-driven motion (curves, timelines,
  procedural bob, transform lerps) over skeletal animation. Reach for an
  AnimSequence only when procedural motion genuinely cannot do the job.

Details: `docs/design/art-direction.md`, `docs/design/game-design-document.md`.

## Current state

Pre-production. The repo is scaffolded; **the `.uproject` does not exist yet.**
Do not assume `Source/`, `Content/` or `Config/` are present — check first.

## Hard rules

1. **Never commit engine-generated directories.** `Binaries/`, `Intermediate/`,
   `Saved/`, `DerivedDataCache/`, `Build/`. They are gitignored; do not force-add.
2. **Every new binary file type goes in `.gitattributes` before its first
   commit.** A binary committed outside LFS is permanently in history and bloats
   every clone. Verify with `git check-attr filter -- <path>`.
3. **Never rewrite pushed history** (`git rebase` onto pushed commits,
   `push --force`, `git lfs migrate`) without explicit approval — it breaks
   every other clone and can orphan LFS objects.
4. **Do not edit `.uasset` / `.umap` as text.** They are opaque binaries. Changes
   to them happen in the Unreal Editor, not in an editor tool.
5. **Do not hand-edit `Config/*.ini` casually.** The editor rewrites these files;
   prefer changing settings through Project Settings so the serialisation matches.
   Exception: engine cvars with no Project Settings surface (e.g. renderer
   tuning like `r.Lumen.*` overrides required by the DLSS plugin) go in ini
   deliberately, each with a comment naming why (see `docs/ROADMAP.md` §6).
6. Branch off `main` — never commit to `main` directly. Mid-task, commit or
   push only when asked. **Goal-completion protocol (standing order from the
   director, 2026-08-31): every time a `/goal` finishes — after tests pass
   and the work is verified — ship it with
   `./tools/scripts/ship.ps1 -Message "<full commit message>"`, which
   compiles + repackages `Dist/` when compiled inputs changed, runs the
   hygiene gate, commits everything, and pushes to GitHub.** Exception: a
   safety checkpoint commit on the current work branch immediately
   **before** any agent-driven editor session (Unreal MCP) is
   pre-authorised — an agent with editor write access and no checkpoint is a
   bad afternoon.

## Conventions

- **Commits:** Conventional Commits — `feat(player): add crouch`. Types: `feat`,
  `fix`, `perf`, `refactor`, `art`, `docs`, `test`, `build`, `ci`, `chore`.
- **Branches:** `feat/`, `fix/`, `art/`, `perf/`, `docs/`, `chore/`.
- **C++ style:** Unreal's own conventions — `U`/`A`/`F`/`E`/`I` type prefixes,
  `PascalCase` everything, tabs, `TEXT()` around literals, `TObjectPtr<>` for
  UPROPERTY object references. See `docs/engineering/coding-standards.md`.
- **Assets:** `Prefix_AssetName_Variant` — `SM_Crate_01`, `M_Rock_Base`,
  `BP_PlayerCharacter`. Full table in
  `docs/engineering/asset-naming-conventions.md`.

## Commands

The engine is not on `PATH` by default. `UE_ROOT` below means your engine install
(typically `C:\Program Files\Epic Games\UE_5.x`).

```powershell
# Regenerate Visual Studio project files after adding/removing source files
& "$env:UE_ROOT\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="$PWD\<Name>.uproject" -game -rocket -progress

# Build the editor target
& "$env:UE_ROOT\Engine\Build\BatchFiles\Build.bat" <Name>Editor Win64 Development -project="$PWD\<Name>.uproject" -waitmutex

# Package a build
& "$env:UE_ROOT\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project="$PWD\<Name>.uproject" -noP4 -platform=Win64 -clientconfig=Development -cook -build -stage -pak -archive -archivedirectory="$PWD\Dist"

# Run automation tests headless
& "$env:UE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\<Name>.uproject" -ExecCmds="Automation RunTests <Name>;Quit" -unattended -nopause -nullrhi -testexit="Automation Test Queue Empty"
```

Repo hygiene (no engine needed, same checks CI runs):

```bash
bash tools/scripts/check-repo-hygiene.sh
```

## Where things go

| Kind of work | Location |
| --- | --- |
| Gameplay C++ | `Source/<Name>/` |
| Editor-only C++ | `Source/<Name>Editor/` |
| Reusable systems | `Plugins/<PluginName>/Source/` |
| Assets | `Content/<Domain>/` — see naming doc for folder structure |
| Design docs | `docs/design/` |
| Irreversible technical decisions | `docs/adr/` (copy `0001` as template) |

## Gotchas specific to this project

- Windows long paths: `core.longpaths` is set locally. UE content paths routinely
  exceed 260 chars.
- LFS pointer files look like ~130 bytes of text starting with
  `version https://git-lfs.github.com/spec/v1`. If you see that where an asset
  should be, the fix is `git lfs pull`, not re-adding the file.
- `.uproject` and `.uplugin` are JSON and *are* tracked as text — they must stay
  out of LFS so they remain diffable.
