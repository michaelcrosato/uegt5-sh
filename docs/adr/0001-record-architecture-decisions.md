# 1. Record architecture decisions

- **Status:** Accepted
- **Date:** 2026-08-31

## Context

Decisions on a game project get made in the moment — often inside the editor,
often under time pressure — and then become load-bearing. Six months later the
reasoning is gone and the only record is the code, which shows *what* was chosen
but never *why*, or what the alternatives were.

The expensive case is not forgetting a good decision. It is re-opening a settled
question, or worse, silently violating a constraint that existed for a reason
nobody can now articulate.

## Decision

Record decisions that are expensive to reverse as ADRs in `docs/adr/`, following
Michael Nygard's format: Context, Decision, Consequences.

ADRs are immutable. Superseding one means writing a new ADR and marking the old
one `Superseded by 00NN`.

## Consequences

- A written trail for anyone joining the project, including future me.
- A natural forcing function: writing down the alternatives usually improves the
  decision.
- Slight overhead per decision. Mitigated by keeping ADRs short — half a page is
  normal — and by only writing them for genuinely irreversible choices.
