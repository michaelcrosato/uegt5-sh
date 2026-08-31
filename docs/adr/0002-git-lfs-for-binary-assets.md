# 2. Git + Git LFS for source control

- **Status:** Accepted
- **Date:** 2026-08-31

## Context

Unreal projects are mostly binary. `.uasset` and `.umap` files are opaque,
frequently large, and change wholesale on every save. Plain Git stores every
version of every binary in every clone, so a project's history compounds until
cloning takes hours.

The realistic options for a small UE5 project:

1. **Perforce (Helix Core).** The industry standard for game development.
   Handles binaries and exclusive checkout properly; UE has first-class
   integration. Requires running or paying for a server, and the workflow is
   unfamiliar to anyone from a general software background.
2. **Plastic SCM / Unity VCS.** Good binary handling, good UE integration,
   hosted. Costs money per seat and ties the project to a vendor.
3. **Git + Git LFS.** Familiar tooling, free hosting on GitHub, everything else
   in the project's ecosystem (CI, issues, PRs, this documentation) already lives
   there. Binaries still cannot be merged, and LFS storage on GitHub is metered.

## Decision

Git with Git LFS, hosted on GitHub.

Binary extensions are routed to LFS in `.gitattributes`. LFS **file locking** is
configured but left commented out — it makes assets read-only on checkout, which
blocks editor saves for a solo developer with no benefit. It gets enabled the
moment a second person works in the editor.

## Consequences

**Good:**

- Zero infrastructure to run. Free at current scale.
- Code review, CI, issues and docs in one place.
- Familiar workflow; no new tool to learn.

**Bad, and accepted:**

- **`.uasset`/`.umap` conflicts are unresolvable.** Mitigated by coordination,
  small assets, short branches, and eventually file locking. This is the real
  cost of this decision.
- **GitHub LFS quota is 1 GB free**, and both pushes *and* clones fail when it is
  exhausted. Needs active monitoring and probably a paid data pack later.
- Git does not do exclusive checkout as naturally as Perforce does.

**Reversal path:** if the project grows past what LFS can carry — more than two
or three people in the editor daily, or LFS costs exceeding a Perforce seat —
migrate to Perforce. Doing so means abandoning the binary history, which is
acceptable; it is the current state of assets that matters, not their past
versions. Revisit at the first sign of daily lock contention.
