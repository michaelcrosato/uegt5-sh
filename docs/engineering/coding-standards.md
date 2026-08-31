# Coding standards

We follow [Epic's C++ coding standard](https://docs.unrealengine.com/5.0/en-US/epic-cplusplus-coding-standard-for-unreal-engine/).
This document only records where we differ or where the engine standard leaves a
choice open.

## Naming

| Kind | Convention | Example |
| --- | --- | --- |
| `UObject` subclass | `U` prefix | `UInteractionComponent` |
| `AActor` subclass | `A` prefix | `APlayerCharacter` |
| `SWidget` subclass | `S` prefix | `SDebugOverlay` |
| Struct / plain class | `F` prefix | `FWeaponStats` |
| Enum | `E` prefix, scoped | `enum class EWeaponState : uint8` |
| Interface | `I` prefix | `IInteractable` |
| Template | `T` prefix | `TRingBuffer` |
| Boolean | `b` prefix | `bIsAiming` |
| Everything else | `PascalCase` | `FireRate`, `CalculateSpread()` |

No Hungarian notation beyond the prefixes above. No `m_`, no trailing
underscores, no `snake_case`.

## Layout

- **Tabs, width 4.** Matches the engine and `.editorconfig`.
- Braces on their own line (Allman), including for single-statement blocks —
  always brace them.
- One class per header where practical.
- `#pragma once` at the top of every header; the engine's generated header
  (`*.generated.h`) is always the **last** include.

## Modules

```
Source/
  <Name>/                 # Runtime gameplay module
    Public/               # Headers other modules may include
    Private/              # Everything else
    <Name>.Build.cs
  <Name>Editor/           # Editor-only tooling (never shipped)
```

Keep the runtime module free of `UnrealEd`, `Slate` editor headers, and anything
`WITH_EDITOR`-only that could leak into a packaged build.

## Unreal-specific rules

- **`TObjectPtr<T>` for `UPROPERTY` object references.** Raw pointers for local
  variables and function parameters are fine.
- **`TEXT()` around every string literal** passed to engine APIs.
- Prefer `checkf()` / `ensureMsgf()` over silent failure. `check` for
  "this is a programming error", `ensure` for "recoverable but I want to know".
- `UE_LOG` with a project-specific category, never `LogTemp`, outside throwaway
  debugging.
- Prefer composition (`UActorComponent`) over deep actor inheritance.
- Mark functions `const` and pass by `const&` by default.
- `Forward declare` in headers; include in the `.cpp`. Header include bloat is
  the main driver of Unreal compile times.

## C++ vs Blueprint

The default split:

- **C++:** systems, data structures, anything performance-sensitive, anything
  that needs to be diffed or reviewed.
- **Blueprint:** composition, tuning values, designer-facing behaviour, quick
  iteration on a single actor.

Blueprints do not diff and do not merge. A Blueprint that has grown into a
system is a signal to move its logic into a C++ base class and leave the
Blueprint as a thin, data-only child.

Expose to Blueprint deliberately: `UPROPERTY(EditAnywhere, BlueprintReadWrite,
Category = "...")` with a real `Category`, not a dumping ground.

## Performance defaults

For a low-poly game the CPU is usually the constraint, not the GPU:

- No per-frame `Tick` unless the actor genuinely needs it. Disable it in the
  constructor (`PrimaryActorTick.bCanEverTick = false;`) and turn it on only
  where required — or use a timer at a lower rate.
- No `GetAllActorsOfClass` on a hot path. Ever.
- Cache component pointers; do not `FindComponentByClass` per frame.
- Prefer events and delegates over polling.

## Tests

Automation tests live beside the code they test, in `Private/Tests/`, using
`IMPLEMENT_SIMPLE_AUTOMATION_TEST`. Test the things that are cheap to test and
expensive to get wrong: math helpers, state machines, save/load, data validation.

Do not chase coverage on `UObject`-heavy gameplay code — the setup cost exceeds
the value. Prefer making that code simple enough to be obviously correct.
