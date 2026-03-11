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
last_updated: 2026-03-11
summary: Cardinal-only V2 combat input runtime that stores U/D/F/B in buffered state bits, infers diagonals during parsing and matching, and preserves MUGEN-style authored command strings.
---

# Combat Input Runtime - MUGEN Simplified

## Summary
This document captures the latest simplified combat-input design and the current implementation direction in `CombatForge`.

Goals:
- align command semantics with MUGEN where intentionally supported
- keep runtime deterministic and fixed-step
- simplify buffering and matching
- only evaluate commands when input state changes

This design supersedes the earlier token-transition matcher for ongoing development.

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

### Direction Normalization Before Buffering
- Facing-relative horizontal resolves to either `F` or `B`, never both.
- Vertical resolves to either `U` or `D`, never both.
- Opposite pairs are canceled before the state enters the buffer.
- Buffered states store only the normalized cardinal state.

## Fixed-Step Buffer
- The input component samples current state on each fixed-step tick.
- The buffer stores one `uint16` state per tick.
- No per-tick transition payload is stored.
- Buffer is a fixed-size ring of recent per-tick states.
- `InputWindowFrames` means: inspect only the last `N` ticks for a command.

Current runtime state owned by the buffer:
- `CurrentTick`
- `PreviousStateBits`
- `HeldTicksByBit[16]`

Why:
- command matching is only triggered when state changes
- charge/release semantics need persistent per-bit timing state
- old press states may expire, so hold timing cannot rely only on backward scanning

## Matching Trigger
- The buffer ticks every fixed step.
- Command solving only runs when `CurrentStateBits != PreviousStateBits`.
- When state does not change:
  - state is still buffered
  - commands are not evaluated

This means command intents are emitted only on input-change ticks.

## Command Semantics
### Supported MUGEN Operators
- `,`
  - ordered sequence
  - does **not** imply release
- `/`
  - held requirement
  - example: `/a,b` means `a` must still be down when `b` is pressed
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
  - same-step combined input requirement
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
- Matching is anchored on the latest changed tick.
- The latest command element must match the newest buffered changed state.
- Earlier elements are searched backward within the command window.
- Candidate ordering is deterministic:
  - higher priority first
  - longer element sequence second
- Default behavior is MUGEN-like leniency:
  - unrelated inputs between elements are allowed
  - unless the next element is marked with `>`

### Direction Literal Semantics
- Cardinal literals (`U`, `D`, `F`, `B`):
  - require their cardinal bit in the current state
  - reject the opposite direction on the same axis
  - do not rely on stored diagonal bits
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
- `Release`
  - token was active on previous tick and is inactive on current tick

Held and release checks operate on cardinal bits:
- inferred diagonal held states come from both component cardinals staying active
- inferred diagonal release transitions come from leaving that composite state
- example: `DF -> F` is treated as `D` released while `F` remains held

### Strict Adjacency
- If an element has `bStrictAfterPrevious = true`:
  - no intervening state changes are allowed between the earlier matched element and the later one
- Strict adjacency keys off actual buffered state changes, not inferred directional-token artifacts

### Charge
- Only charge-then-release is in scope
- `~30a` is supported
- Current simplification:
  - release-charge matching is reliable when the charged release is the latest triggering input

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
  - additional cardinal direction bits allowed for inferred directional supersets such as `$D`
- `MatchKind`
  - `Press`, `Held`, `Release`
- `MinHeldFrames`
  - used for charge-release
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
- press `D`
- then press `D+F` on the same tick
- then release `D` while `F` remains held or continue into pure `F`

## Current Implementation Shape
### Input Component
`UCombatForgeInputComponent` is responsible for:
- maintaining current button bits from `InputAction` start/completion
- converting 2D stick input into normalized facing-relative cardinal bits each fixed step
- feeding the current 16-bit state into the buffer
- broadcasting raw state bits + matched commands only when state changed

### Input Buffer
`FCombatForgetInputBuffer` is responsible for:
- fixed-size per-tick state ring buffer
- direction normalization safety for incoming states
- command string compilation
- per-bit hold timing
- inferred-direction MUGEN-style command matching
- deterministic candidate sorting by priority, then element count

## Debug Expectations
- The raw timeline shows stored cardinal state bits.
- Debug or match traces may also show an inferred 8-way direction label for readability.
- Candidate rejection reasons should continue to describe parser or matcher failures deterministically.

## Current Tests
The current automated coverage is aimed at:
- fixed ring history behavior
- parser compilation for diagonal literals and directional supersets
- `a,b`
- `/a,b`
- `a,>b`
- `~a,b`
- `~30a`
- `D,DF,F`
- derived diagonal hold/release behavior from cardinal states
- opposite-direction normalization and facing-relative quantization
- replay determinism for equivalent cardinal streams

## Known Simplifications
- Matching only happens on state changes.
- Only charge-then-release is considered for now.
- Full MUGEN deep-buffering behavior is not implemented.
- The current system does not try to auto-fire a command that becomes valid without a new input change.
- Stored history is intentionally compact: one normalized key-state per tick.

## Development Notes
Areas to refine next:
- verify full compile correctness in UE/MSBuild
- tighten parser validation and error reporting
- clarify exact behavior for `+` under fixed-step sampling
- extend tests for more MUGEN command forms
- decide whether later development should support deeper historical hold-state behavior beyond the current scope
