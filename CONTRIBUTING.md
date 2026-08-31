# Contributing

> This is a personal project that happens to be public. The guidance below is written for people with commit access. Outside pull requests are not expected — but issues and design discussion are genuinely welcome, and the licence ([LICENSE](LICENSE)) does not grant rights to reuse the code or assets.

## Before your first commit

```bash
git lfs install                          # once per machine
git config --global core.longpaths true  # Windows only, once per machine
```

If `git lfs install` was not run before you cloned, your `Content/` folder holds
~130-byte text pointer files instead of assets. Fix it with `git lfs pull`.

## Branches

`main` is protected and must always build and launch. Everything else happens on
a branch and merges through a pull request.

| Prefix | For |
| --- | --- |
| `feat/` | New gameplay, systems, features |
| `fix/` | Bug fixes |
| `art/` | Assets, materials, levels, VFX |
| `perf/` | Optimisation with no behaviour change |
| `docs/` | Documentation only |
| `chore/` | Tooling, CI, config, dependencies |

Example: `feat/weapon-hitscan`, `art/greybox-atrium`.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/):

```
<type>(<scope>): <short imperative summary>

<optional body explaining *why*, wrapped at 72 chars>
```

Types: `feat`, `fix`, `perf`, `refactor`, `art`, `docs`, `test`, `build`, `ci`, `chore`.

Good: `feat(player): add crouch with capsule half-height lerp`
Bad: `updates`, `wip`, `fixed stuff`

Keep asset commits separate from code commits where you can. A commit that
touches thirty `.uasset` files and one `.cpp` is impossible to review or revert.

## Pull requests

- Fill in the PR template. The "how I tested this" section is not optional.
- Attach a screenshot or short clip for anything visual.
- Keep PRs small. A 40-file art drop and a gameplay change belong in two PRs.
- CI (`repo-hygiene`) must be green.

## Working with binary assets

**`.uasset` and `.umap` files cannot be merged.** If two branches modify the same
asset, one side's work is lost. To avoid it:

1. Say what you are working on before you start on shared assets (levels are the
   usual flashpoint).
2. Prefer many small assets over one monolithic one. Use Level Instances / World
   Partition rather than one giant `.umap`.
3. Rebase early and often; do not let an asset branch sit for a week.
4. When the team grows past one person, enable LFS file locking — see
   [`docs/engineering/source-control.md`](docs/engineering/source-control.md).

## Adding a new file type

Check [`.gitattributes`](.gitattributes) first. Any new binary extension must be
LFS-tracked *before* the first file of that type is committed:

```bash
git lfs track "*.newext"
git add .gitattributes
```

## Code

Follow [`docs/engineering/coding-standards.md`](docs/engineering/coding-standards.md)
and [`docs/engineering/asset-naming-conventions.md`](docs/engineering/asset-naming-conventions.md).
Both are short; read them once.

## Making a decision that is hard to reverse

Write an ADR in [`docs/adr/`](docs/adr/). Copy `0001` as the template. This
applies to things like: choosing an input system, committing to a networking
model, picking an animation approach, adopting a large plugin.
