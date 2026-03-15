# Combat Input Matcher Redesign

## Summary

Replace the current anchor-based backward-scanning matcher with an incremental token-state
matcher.

New core rule:
- emit command intents only when a command becomes newly complete on the current tick

This removes the need for backward history scans during matching and makes hold / release / charge
semantics much cleaner.

Ikemen-GO is the reference for the token age-counter model:
- `C:/projects/Ikemen-GO/src/input.go`

It is a reference for runtime input-state tracking and incremental command progression, not a full
behavior spec to copy exactly.

## Goals

- remove backward scanning from command matching
- keep deterministic command arbitration
- support press, hold, release, and charge semantics cleanly
- support both `~30a` and `/30a`
- keep compact debug snapshots for timeline visibility
- preserve data-driven command definitions

## Core Design

### 1. Token-State Runtime Model

Use per-token signed age state as the matcher source of truth.

```cpp
struct FCombatInputTokenState
{
    int16 Age = 0;
    int16 PrevAge = 0;
};

Semantics:

- Age == 1: token changed into held this tick
- Age > 1: token has been held for Age ticks
- Age == -1: token changed into released this tick
- Age < -1: token has been released for abs(Age) ticks

Runtime owns:

- TokenStates[16]
- CurrentStateBits
- PreviousStateBits
- per-command progress state
- optional compact debug snapshot ring

### 2. Input Update Model

Each fixed tick:

1. sample raw input
2. normalize directional state
3. compute CurrentStateBits
4. update all token ages from previous vs current state
5. advance commands incrementally
6. emit only commands newly completed this tick

### 3. Command Progress Model

Each compiled command owns small runtime progress state.

struct FCombatCommandProgress
{
    int16 NextStepIndex = 0;
    int16 SequenceStartTick = -1;
    int16 LastMatchedTick = -1;
    bool bCompletedThisTick = false;
};

Meaning:

- NextStepIndex: next step to satisfy
- SequenceStartTick: when the current attempt started
- LastMatchedTick: tick of the most recent matched step
- bCompletedThisTick: transient completion flag for arbitration / emission

### 4. Intent Emission Rule

Commands do not emit because they are currently valid.

Commands emit only when:

- they were incomplete before this tick
- they become complete on this tick

This replaces the old “newest changed tick is the anchor” rule.

## Matching Semantics

### Press

A press step matches when:

- required token(s) were newly pressed this tick
- current state satisfies the step mask
- previous state did not already satisfy it

### Held

A held step matches when:

- required token(s) are currently held
- accepted mask rules are satisfied
- optional minimum hold duration is satisfied

This naturally supports /a and /30a.

### Release

A release step matches when:

- required token(s) were newly released this tick
- previous state satisfied the held form
- current state no longer satisfies it
- optional minimum prior hold duration is satisfied

This naturally supports ~a and ~30a.

### Charge

Charge checks are derived directly from token ages:

- held charge: current Age >= N
- release charge: previous held age before release >= N

No backward scan is required.

## Direction Handling

Use the token-state model for runtime representation, but keep Combat Input System.md as the
behavioral source of truth for direction semantics.

Adopt:

- normalized cardinal state bits before token-age update
- exact plain cardinal matching
- explicit author-driven directional supersets
- deterministic directional matching rules

Do not copy Ikemen-GO’s exact directional permissiveness wholesale.

Ikemen-GO is only the reference for:

- age-counter input tracking
- incremental command advancement
- clean press / hold / release / charge queries

## Algorithm Flow

Each fixed tick:

1. normalize input into CurrentStateBits
2. update TokenStates
3. derive changed tokens
4. clear all bCompletedThisTick
5. for each command:
    - expire old progress if command window elapsed
    - try to start, restart, or advance progress
    - if final step is reached, mark completed this tick
6. arbitrate among commands completed this tick
7. emit intents in deterministic order

## Restart / Overlap Policy

Partial command progress should not be discarded immediately when a tick fails to advance the next
step.

Policy:

- keep progress alive until timeout or explicit invalidation
- allow step 0 to restart the command with a fresher prefix

Example:

- command a,b
- input a,a,b

Behavior:

- first a starts progress
- second a refreshes the prefix
- b completes the command

This matches the useful incremental behavior seen in Ikemen-GO.

## Strict > Policy

Without backward scanning, strict adjacency is implemented through incremental progression rules.

Recommended rule:

- if a step is marked strict, it must match on the immediately next eligible changed tick after
the previous matched step
- otherwise the current attempt fails

This preserves strict sequencing without requiring history scans.

## Arbitration

Only commands newly completed on the current tick enter arbitration.

Deterministic sort order:

1. higher priority
2. longer matched sequence
3. smaller timing slack
4. lower command id

Suggested slack metric:

- InputWindowFrames - (CurrentTick - SequenceStartTick)

## Debug Model

Do not use a ring buffer for matching.

Keep only a compact optional debug snapshot ring:

struct FInputSnapshot
{
    uint16 StateBits;
    uint16 ChangedBits;
};

This is sufficient for:

- input timeline display
- press / release reconstruction
- deterministic debug logs

The matcher must not depend on this snapshot ring.

## Ikemen-GO Reference

Reference file:

- C:/projects/Ikemen-GO/src/input.go

Relevant ideas borrowed:

- signed age counters per token
- press / hold / release derived from token age
- incremental per-command progression
- charge determined from held age rather than history scans

Relevant behaviors not adopted as-is:

- full directional compatibility rules
- name-based command clearing behavior
- lack of explicit global deterministic arbitration

## Resulting Architecture

### Matcher source of truth

- token age states

### Command progression source of truth

- per-command incremental progress state

### Debug source of truth

- compact snapshot ring

### Emission rule

- newly completed this tick only

## Expected Benefits

- cleaner matcher implementation
- no backward scanning
- direct support for /30a and ~30a
- simpler step semantics
- better separation between runtime matching and debug visibility
- easier profiling and reasoning about command progression