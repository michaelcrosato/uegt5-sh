# Architecture decision records

Short, numbered, immutable notes recording decisions that are **expensive to
reverse**. They exist so that in six months nobody has to reconstruct the
reasoning from the code.

## When to write one

- Choosing between two viable technical approaches (input system, save format,
  networking model, animation strategy).
- Adopting a large dependency or plugin.
- Anything you would otherwise explain twice.

Not for: routine implementation choices, anything a comment covers.

## How

1. Copy `0001-record-architecture-decisions.md` as a template.
2. Number it sequentially, name it in kebab-case.
3. Status is `Proposed` → `Accepted` → later possibly `Superseded by 00NN`.

**Never edit an accepted ADR's decision.** Changing your mind means writing a new
ADR that supersedes it — the point is the audit trail, not the current state.

## Index

| # | Title | Status |
| --- | --- | --- |
| [0001](0001-record-architecture-decisions.md) | Record architecture decisions | Accepted |
| [0002](0002-git-lfs-for-binary-assets.md) | Git + Git LFS for source control | Accepted |
| [0003](0003-minimal-animation-strategy.md) | Code-driven motion over skeletal animation | Accepted |
