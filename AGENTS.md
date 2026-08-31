# AGENTS.md — working contract for AI agents

Read this first, every session. `CLAUDE.md` covers repo mechanics; this file
covers how implementation work is done. `docs/ROADMAP.md` is the plan of
record; the ADRs in `docs/adr/` are settled and not up for re-litigation.

## The ten rules

1. **One system per task.** If a task needs changes in more than three files
   outside its module, stop and ask.
2. **Read before writing.** Read the relevant roadmap section and any
   `docs/systems/<system>.md` before implementing. No spec? Write it first.
3. **Determinism is law** (ROADMAP §5.1). All generation randomness via
   `FC::Gen::MakeStream`. Never `FMath::Rand`. Never iterate `TMap`/`TSet`
   where order affects generated output. Never read clocks, frame timing, or
   GPU state in generation or perception code.
4. **No new plugins or dependencies** without an explicit decision-log entry.
5. **No magic numbers.** Tunables go to CSV/JSON data (ROADMAP §10.1).
6. **Every system ships with an automation test.** No test, no merge.
   Tests live in `Source/FootcandleTests/` or `Private/Tests/` beside the code.
7. **No Blueprint logic** (ADR-0005). Blueprint is for thin config wrappers
   only. Behavior Trees, PCG graphs for gameplay, and MetaSound graphs
   authored by hand in the editor are all the same violation.
8. **Performance note required** on any change touching rendering,
   generation, or per-frame code: state expected cost against ROADMAP §6.1.
9. **Docs are part of the change.** Update the system doc and the roadmap
   decision log in the same commit, not a follow-up.
10. **Never commit an asset a script can regenerate. Commit the script.**
    New binary file types go in `.gitattributes` BEFORE the first commit.

## Verification loop

```powershell
./tools/scripts/build.ps1              # compile the editor target
./tools/scripts/run-tests.ps1          # headless automation tests (nullrhi)
./tools/scripts/visual-check.ps1       # launch game, tour stations, screenshots
```

A compile is not a pass. Logic changes run the tests; anything visible runs
the visual check and the screenshots get *looked at*. Renderer claims are
verified in the `[FCBOOT]` log lines, not assumed from ini files.

## Visual testing (use it constantly)

- `-fcdevscene` on the command line spawns the code-defined lighting scene.
- `fc.Station <name>` pops the camera to a named station; `fc.Stations` lists.
- `fc.Tour [dir] [quit]` screenshots every station after a settle delay.
- `-fctour=<dir>` runs the tour automatically and exits (what
  `visual-check.ps1` uses).
- Register a station for every new visual feature — stations are the visual
  regression corpus.

## Unreal MCP sessions (when enabled)

Checkpoint-commit on the work branch before every MCP editor session
(pre-authorized, CLAUDE.md rule 6). Read-only tools freely; write tools
deliberately; destructive tools only with per-session approval. One agent per
editor process. localhost only.
