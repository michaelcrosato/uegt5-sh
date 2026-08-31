# Source control workflow

Game repos break in ways code repos do not: assets are large, opaque, and
unmergeable. Everything here follows from that.

## One-time machine setup

```bash
git lfs install
git config --global core.longpaths true     # Windows: UE content paths exceed 260 chars
git config --global core.autocrlf false     # .gitattributes handles line endings
```

## How LFS is wired here

[`.gitattributes`](../../.gitattributes) routes binary extensions through Git
LFS. Git stores a ~130-byte pointer; the real bytes live on GitHub's LFS store.

Verify a file is going to LFS **before** committing it:

```bash
git check-attr filter -- Content/Environment/Props/SM_Crate_01.uasset
# → Content/...uasset: filter: lfs
```

List what LFS is actually tracking:

```bash
git lfs ls-files | head
git lfs status
```

### Adding a new binary type

```bash
git lfs track "*.newext"    # appends a rule to .gitattributes
git add .gitattributes
```

Do this **before** the first file of that type is committed. `.gitattributes` is
not retroactive: a binary committed without it is in history forever, and every
future clone pays for it.

### Fixing it after the fact

```bash
git lfs migrate import --include="*.newext" --everything
```

This **rewrites history**. Every other clone must be re-cloned. Do not run it on
a shared branch without telling everyone first.

### Symptom: assets are 130-byte text files

You cloned without `git lfs install`. Fix:

```bash
git lfs install
git lfs pull
```

### LFS storage quota

GitHub's free tier includes 1 GB of LFS storage and 1 GB/month of bandwidth.
A UE project passes that quickly. Watch it under
**Settings → Billing → Git LFS Data**, and buy a data pack before it hits zero —
when the quota is exhausted, pushes *and* clones fail.

Keep it down by: not committing source-art `.blend`/`.psd` files that nothing
imports, keeping textures small, and never committing packaged builds.

## Branching

`main` is protected and always buildable. Work on a branch, merge via PR.

```
feat/  fix/  art/  perf/  docs/  chore/
```

Squash-merge into `main`, so `main`'s history is one commit per change.

## Branch protection

`main` is guarded by a repository **ruleset** (`main-protection`), not classic
branch protection. It enforces:

| Rule | Effect |
| --- | --- |
| Block deletion | `main` cannot be deleted |
| Block non-fast-forward | no `push --force` to `main` |
| Require a pull request | **0** approvals required, so a solo developer can self-merge |
| Squash merge only | one commit on `main` per change |
| Require conversation resolution | no merging over an unresolved review comment |
| Require status check `hygiene` | the repo-hygiene CI job must pass |

The **repository admin role bypasses all of it**. That is deliberate: it keeps
the documented workflow as the default path without locking the owner out of an
emergency fix. Bypassing is an escape hatch, not a shortcut — if you are using it
routinely, the ruleset is wrong and should be changed instead.

### Renaming a CI job breaks merging

The required status check is pinned to the **job name** `hygiene` in
[`repo-hygiene.yml`](../../.github/workflows/repo-hygiene.yml). If that job is
renamed, the check named `hygiene` never reports, and GitHub waits for it
forever — every PR shows as blocked with no failing check to point at.

Rename the job and update the ruleset in the same change, or do not rename it.

The same trap catches any PR branched from before a job rename: it reports the
*old* check name and is blocked until rebased onto `main`.

## The binary merge problem

**`.uasset` and `.umap` files cannot be merged.** If two branches change the same
asset, `git merge` reports a conflict with no way to resolve it except picking
one side and losing the other's work.

Mitigations, in order of how much they help:

1. **Talk before touching shared assets.** Levels are the usual collision point.
2. **Split large assets.** World Partition / Level Instances / sublevels turn one
   contested `.umap` into many uncontested ones.
3. **Short-lived branches.** An asset branch that lives a week will conflict.
4. **File locking** (below), once more than one person works in the editor.

When a conflict does happen, there is no merge — pick a side:

```bash
git checkout --ours   Content/Maps/L_Atrium.umap    # keep the version on your branch
git checkout --theirs Content/Maps/L_Atrium.umap    # take the incoming version
git add Content/Maps/L_Atrium.umap
```

Then manually redo whatever the discarded side contained.

## File locking (enable when the team grows past one)

LFS locking makes an asset read-only until someone explicitly checks it out,
which prevents the conflict rather than resolving it.

1. Uncomment the `lockable` lines in [`.gitattributes`](../../.gitattributes).
2. In the editor: **Edit → Project Settings → Source Control**, select the **Git**
   provider, tick **Use Git LFS File Locking**.
3. From then on, the editor takes and releases locks automatically as you check
   assets out.

Manually:

```bash
git lfs lock   Content/Maps/L_Atrium.umap
git lfs locks
git lfs unlock Content/Maps/L_Atrium.umap
```

Do not enable this while working solo — read-only `.uasset` files will simply
prevent the editor from saving.

## Never commit

`Binaries/` · `Intermediate/` · `Saved/` · `DerivedDataCache/` · `Build/` (except
the whitelisted files) · packaged builds · `.sln` and other generated project
files.

They are in [`.gitignore`](../../.gitignore). If you find yourself reaching for
`git add -f` on one of them, stop and ask why.

## Never rewrite pushed history

`push --force`, rebasing pushed commits, and `git lfs migrate` all invalidate
every other clone and can orphan LFS objects. On a solo repo it is merely
annoying; with collaborators it is destructive. Coordinate first, always.
