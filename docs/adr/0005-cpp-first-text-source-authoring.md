# 5. C++-first authoring; text files are the source of truth

- **Status:** Accepted
- **Date:** 2026-08-31

## Context

This project's production model is AI-authored, human-directed: agents write
most of the code and content, and a single director reviews it. Review
happens in diffs.

Blueprint graphs, Behavior Trees, PCG graphs, and MetaSound graphs are binary
`.uasset` files: they cannot be meaningfully diffed, code-reviewed in a PR,
merged, or edited by a text-based agent. On a conventional team Blueprint is
a productivity tool; on this project it is a hole in reviewability exactly
where most of the work happens. The same logic applies to tunable values
buried in asset defaults.

## Decision

| Layer | Authored as |
| --- | --- |
| Gameplay, systems, AI, generation, perception | C++ |
| Tunables | CSV / JSON committed as source of truth, imported to DataTables / DataAssets |
| Enemy behavior | Hand-rolled C++ state machines (not Behavior Trees / StateTree assets) |
| Materials | One hand-built master; all variants parameter-driven instances |
| UI | CommonUI in C++; Blueprint widget shells for layout only |
| Blueprint | Thin config wrappers and spawn presets — nothing with logic |

Corollaries:

- No magic numbers in C++; no tunables in Blueprint defaults.
- Assets a script can regenerate are not committed — the script is.
- MetaSound graphs are built via the Builder API from C++ where practical.
- An agent should almost never need the editor open to change gameplay.

## Consequences

- Every meaningful change is reviewable as text. This is the property the
  whole production model rests on.
- Compile times matter more than on a Blueprint-heavy project; mitigated by
  module separation (`docs/ROADMAP.md` §10.2).
- Designer-style in-editor tweaking is deliberately constrained; tuning moves
  to CSV edits + hot-reloadable data assets instead.
- Some engine workflows (e.g. animation blueprints, PCG node graphs) are
  effectively off the roster; acceptable because ADR-0003 and ADR-0006
  independently minimize both.
