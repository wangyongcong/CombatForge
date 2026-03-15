---
id: combat_input_system_v2
title: Combat Input Runtime - MUGEN Simplified
domain: combat
subsystems:
  - input
  - command-matching
  - buffering
  - parsing
  - debugging
engine_features:
  - Enhanced Input
  - Gameplay Ability System
  - State Tree
status: draft-v2
owner: gameplay-engineering
tags:
  - combat
  - deterministic
  - data-driven
  - command-input
  - mugen
  - v2
last_updated: 2026-03-14
summary: Cardinal-only V2 combat input runtime that normalizes U/D/F/B into state bits, tracks per-token signed ages for incremental command progression, uses exact cardinal press matching with explicit supersets, and preserves MUGEN-style authored command strings.
---

# Combat Input Runtime - MUGEN Simplified

## Summary
This document captures the latest simplified combat-input design and the current implementation direction in `CombatForge`.

Goals:
- align command semantics with MUGEN where intentionally supported
- keep runtime deterministic and fixed-step
- simplify buffering and matching
- emit command intents only when they become newly complete on the current tick

This design supersedes the earlier anchor-based backward matcher for ongoing development.

## Input Model
- The runtime uses a 16-bit input state.
- Runtime direction storage is cardinal-only:
  - lower direction bits store `Up`, `Down`, `Forward`, `Back`
  - diagonals (`UB`, `UF`, `DB`, `DF`) are inferred views derived from same-tick cardinal combinations
  - neutral is represented by no direction bits set
- Buttons remain in the upper bits:
  - `X`, `Y`, `Z`, `A`, `B`, `C`
- Command strings keep MUGEN-style literals:
  - directions are uppercase: `U, UB, B, DB, D, DF, F, UF`
  - face buttons are lowercase: `x, y, z, a, b, c`

### Direction Normalization Before State Tracking
- Facing-relative horizontal resolves to either `F` or `B`, never both.
- Vertical resolves to either `U` or `D`, never both.
- Opposite pairs are canceled before the state enters runtime tracking.
- Runtime state stores only the normalized cardinal state.

## Fixed-Step Runtime
- The input component samples current state on each fixed-step tick.
- The matcher source of truth is per-token signed age state, not backward history scans.
- The runtime may keep a compact snapshot ring for debug visibility, but matching does not depend on it.

Current matcher state owned by the runtime:
- `CurrentTick`
- `CurrentStateBits`
- `PreviousStateBits`
- `TokenStates[16]`
  - `Age > 0` means held for `N` ticks
  - `Age < 0` means released for `N` ticks
  - `Age == 1` means newly pressed this tick
  - `Age == -1` means newly released this tick
- per-command progress state
  - next step index
  - sequence start tick
  - last matched tick
  - held-completion latch, when needed

Why:
- press / hold / release / charge semantics can be queried directly from token ages
- commands can advance incrementally without backward scanning
- `~30a` and `/30a` are both natural under the same token-age model
- this is closer in spirit to Ikemen-GO's input age-state approach while preserving CombatForge's own directional and arbitration rules

## Matching Trigger
- The runtime ticks every fixed step.
- Commands may advance on any fixed tick if their next required step is satisfied.
- Press and release steps still require a changed tick.
- Held-only completions may occur on a quiet tick once the held condition becomes valid.
- A command intent is emitted only when the command becomes newly complete on the current tick.
- Held-final commands are latched until their final held condition becomes false so they do not re-fire every tick.

## Command Semantics
### Supported MUGEN Operators
- `,`
  - ordered sequence
  - does **not** imply release
- `/`
  - held requirement
  - example: `/a,b` means `a` must still be down when `b` is pressed
- `/N`
  - held charge requirement
  - example: `/30a` means `a` has been held for at least 30 ticks
- `~`
  - release requirement
  - example: `~a,b` means release `a`, then press `b`
- `~N`
  - charge-release
  - example: `~30a` means hold `a` at least 30 ticks, then release it
- `$`
  - directional superset
  - example: `$D` matches `D`, `DB`, `DF`
- `+`
    - combined input requirement on one changed sampled state
    - all tokens must be down on the matching tick
    - one token may already be held if the full combination becomes newly satisfied on that tick
    - example: `a+b`
- `>`
  - strict adjacency
  - no intervening input-state changes between adjacent command elements

### Important Differences From Earlier Design
- `a,b`
  - now means MUGEN ordered sequence
  - does not require releasing `a`
- `/a,b`
  - now has distinct held behavior
- `~a,b`
  - now means explicit release then next input
- `>`
  - no longer means charge
- charge syntax is `~Ntoken`, not `>Ntoken`

## Matching Rules
### General
- Matching is incremental, forward-only progression.
- Each command owns explicit partial progress state.
- Commands do not search backward through buffered history.
- A command advances only when its next legal step matches on the current tick.
- Earlier partial progress remains alive until timeout or invalidation; there is no generic restart from step 0.
- Candidate ordering is deterministic:
  - higher priority first
  - longer element sequence second
  - smaller timing slack third
  - lower command id fourth
- Default behavior is MUGEN-like leniency:
  - unrelated inputs between elements are allowed
  - unless the next element is marked with `>`

### Problems This Revision Solves
- Before the exact-cardinal change, plain `D` and `F` also accepted `DF`.
- That made motion paths ambiguous:
  - `Hadoken`-style `D,DF,F+a`
  - `Shoryuken`-style `F,D,DF+a`
  - could both match the same diagonal-heavy history because `DF` could satisfy both `D` and `F`.
- The result was that command identity was often determined by arbitration order rather than by the authored motion path.
- Making plain cardinal presses exact fixes that ambiguity:
  - `D` means exact `D`
  - `F` means exact `F`
  - diagonal leniency must be authored explicitly with `$`
- This change exposed two practical motion-input issues that the matcher now handles explicitly:
  - quarter/circle motions often need a held cardinal to remain valid when the stick rolls into a neighboring diagonal, so held plain cardinals accept neighboring diagonals that preserve the held component
- These exceptions are limited to directional motion ergonomics:
  - press elements remain exact
  - non-directional press elements keep their original behavior
  - explicit directional supersets still use `$`

### Direction Literal Semantics
- Cardinal literals (`U`, `D`, `F`, `B`):
  - require their cardinal bit in the current state
  - reject any extra direction bits unless explicitly allowed with `$`
  - do not rely on stored diagonal bits
  - exception: held plain cardinals accept neighboring diagonals that preserve the held component
- Diagonal literals (`UB`, `UF`, `DB`, `DF`):
  - require both component cardinals on the same tick
  - are inferred from the stored cardinal state during parsing and matching
- Directional superset (`$`):
  - expands over inferred directional classes, not stored diagonal bits
  - example: `$D` accepts `D`, `DB`, `DF`

### Per Element Types
- `Press`
  - token becomes active on that tick
- `Held`
  - token is active on that tick
  - for plain cardinal directions, held checks accept neighboring diagonals on the orthogonal axis
  - may also include a minimum hold duration such as `/30a`
- `Release`
  - token was active on previous tick and is inactive on current tick
  - may also include a minimum pre-release hold duration such as `~30a`

Implications for common motions:
- `F,D,DF+a`
  - `F`, `D`, and `DF+a` all remain exact event steps unless `$` is authored
  - `DF+a` remains exact unless `$` is authored
- `/D,DF,F+a`
  - `/D` remains valid while the stick rolls through `DF`
  - later `DF` and `F+a` advance the sequence without resetting earlier partial progress
- `D,DF,F+a`
  - remains exact on its directional press steps
  - if diagonal leniency is desired for a specific element, author it explicitly with `$`

Held and release checks operate on cardinal bits:
- inferred diagonal held states come from both component cardinals staying active
- inferred diagonal release transitions come from leaving that composite state
- example: `DF -> F` is treated as `D` released while `F` remains held

### Strict Adjacency
- If an element has `bStrictAfterPrevious = true`:
  - the next strict event step must match on the immediately next eligible changed tick after the previous matched step
- Strict adjacency keys off actual normalized state changes, not inferred directional-token artifacts

### Charge
- Held-charge and charge-release are both in scope
- `/30a` is supported
- `~30a` is supported

## Current Parser Model
Each command string compiles into:
- `FCombatForgeCommand`
- `TArray<FCombatForgeCommandElement>`

Each element currently stores:
- `RequiredMask`
- `AcceptedMask`
- `MatchKind`
- `MinHeldFrames`
- `bStrictAfterPrevious`

Interpretation:
- `RequiredMask`
  - required cardinal direction bits plus button bits
  - diagonals compile to multi-bit cardinal requirements
- `AcceptedMask`
  - additional cardinal direction bits allowed only for authored directional supersets such as `$D`
- `MatchKind`
  - `Press`, `Held`, `Release`
- `MinHeldFrames`
  - used for held-charge and charge-release
- `bStrictAfterPrevious`
  - used for `>`

Helpers that need direction readability should derive one inferred direction class from the current cardinal state:
- `Neutral`
- `U`, `D`, `F`, `B`
- `UB`, `UF`, `DB`, `DF`

## Examples
### Plain Sequence
Command:
```text
a,b
```
Meaning:
- press `a`
- later press `b`
- holding `a` while pressing `b` is allowed

### Held Sequence
Command:
```text
/a,b
```
Meaning:
- `a` must be down when `b` is pressed

### Release Sequence
Command:
```text
~a,b
```
Meaning:
- release `a`
- later press `b`

### Strict Sequence
Command:
```text
a,>b
```
Meaning:
- press `a`
- then press `b`
- no intervening input-state changes are allowed

### Charge Release
Command:
```text
~30a
```
Meaning:
- hold `a` for at least 30 ticks
- release `a`

### Directional Superset
Command:
```text
$D,x
```
Meaning:
- enter any down direction (`D`, `DB`, `DF`)
- then press `x`

### Quarter Circle
Command:
```text
D,DF,F
```
Meaning:
- enter the `D` direction sector
- then enter the `DF` direction sector
- then enter the `F` direction sector

## Current Implementation Shape
### Input Component
`UCombatForgeInputComponent` is responsible for:
- maintaining current button bits from `InputAction` start/completion
- converting 2D stick input into normalized facing-relative cardinal bits each fixed step
- feeding the current 16-bit state into the runtime matcher
- broadcasting raw state bits + matched commands only when state changed

### Input Runtime
`FCombatForgetInputBuffer` is responsible for:
- direction normalization safety for incoming states
- command string compilation
- per-token signed age tracking
- per-command incremental progression
- compact debug snapshot storage
- deterministic candidate sorting by priority, then element count, then slack, then id

## Debug Expectations
- The raw timeline shows normalized cardinal state bits.
- A compact snapshot may store:
  - `StateBits`
  - `ChangedBits`
- Debug or match traces may also show an inferred 8-way direction label for readability.
- Command progression traces should be able to show:
  - current step index
  - matched / waiting status
  - completion this tick
  - invalidation or timeout reasons

## Current Tests
The current automated coverage is aimed at:
- parser compilation for diagonal literals and directional supersets
- `a,b`
- `/a,b`
- `/30a`
- `a,>b`
- `~a,b`
- `~30a`
- `Hadoken`-style `/D,DF,F+a`
- `Shoryuken`-style `F,D,DF+a`
- derived diagonal hold/release behavior from cardinal states
- opposite-direction normalization and facing-relative quantization
- realistic motion-input noise around authored motions
- replay determinism for equivalent cardinal streams

## Known Simplifications
- Only one partial progress path is currently tracked per command.
- Full MUGEN deep-buffering behavior is not implemented.
- The runtime may keep compact snapshots for debug, but matching does not use a full backward history scan.
- Motion ergonomics currently rely on held-cardinal diagonal tolerance rather than broader directional restart heuristics.
- Raw state display shown only on changed ticks can hide the exact per-tick sampled path; full per-tick traces are still useful when diagnosing live input issues.

## Development Notes
Areas to refine next:
- verify full compile correctness in UE/MSBuild
- tighten parser validation and error reporting
- extend tests for more MUGEN command forms
- decide whether future versions should support multiple concurrent partial paths per command
- 2026-03-11: cardinal literals are exact by default; use `$` when a command should also accept diagonal variants
- 2026-03-14: replaced the anchor-based backward matcher with token-age incremental progression inspired by Ikemen-GO's input age-state model
- 2026-03-14: removed generic restart-from-first-step behavior because it clobbered deeper motion progress for commands such as Shoryuken
