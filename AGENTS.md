# AGENTS.md

## Project Overview
- Project: `CombatSystem` (UE5)
- Focus: deterministic, data-driven combat for 3rd-person action games inspired by Sekiro, Stellar Blade, and Dynasty Warriors.
- Primary design source: [`Doc/Combat Input System.md`](Doc/Combat%20Input%20System.md)

## Foundation Stack (Required)
- Gameplay Ability System (GAS): all gameplay attributes, costs, effects, and skills/abilities.
- Enhanced Input: player input mapping, action definitions, and input event ingestion.
- State Tree: gameplay state machine and AI decision flow.

Do not introduce parallel frameworks for these responsibilities unless explicitly requested.

## Source of Truth
- Treat documents in `Doc/*.md` as gameplay/system design contracts.
- If code and docs disagree, do not silently change behavior. Either:
  - implement code to match docs, or
  - update docs and changelog together with clear rationale.

## Design Doc Discovery (Mandatory)
- For any development task, search `Doc/` first.
- Read document metadata before full content to decide relevance.
- Treat `Doc` metadata as routing hints (similar to skill selection).
- If multiple docs are relevant, use the minimal set and note which docs were used.
- If no relevant doc exists, proceed with a minimal deterministic implementation and add `TODO(doc)` in code/notes.

## Current System Boundary (v1)
The combat input system owns:
- physical input -> logical token mapping
- event ring buffer + expiration
- preprocessing (direction normalization, simultaneous merge, charge pseudo-events)
- command solving (sequence/special matching)
- candidate arbitration
- command intent emission

The combat/state machine owns:
- combo branch progression
- legality checks (state/cancel/resource/hitstun/hitstop)
- move execution and animation state

System integration boundary:
- Input pipeline reads from Enhanced Input and outputs deterministic tokens/intents.
- Command intents are consumed by GAS-driven ability activation flow.
- State Tree owns high-level state transitions and AI behavior selection using those intents/results.

## Core Engineering Rules
1. Determinism first.
- Timing must be frame-based, never wall-clock based.
- Matching/arbitration outcomes must be stable across runs for the same input stream.

2. Data-driven behavior.
- Prefer command data/assets over hardcoded per-command logic.
- Keep alternatives, windows, priorities, and consume rules authorable in data.

3. Explicit conflict resolution.
- Use deterministic arbitration order:
  1) higher priority
  2) longer matched sequence
  3) smaller timing slack
  4) lower command id

4. Clear ownership boundaries.
- Do not move combo FSM logic into input solver.
- Input system emits command intents and raw mapped input events only.

5. Debuggability is mandatory.
- Keep input timeline visibility and candidate rejection reasons available in dev builds.

## Implementation Guidance (UE5)
- Use fixed-size structures where possible for predictable performance (ring buffer capacity target: 64-128 events).
- Store events with integer token IDs and frame index.
- Keep per-token hold state (`isDown`, `pressFrame`, `lastReleaseFrame`) for charge logic.
- Backward matching from newest event for responsiveness.
- Prefer precompiled command token arrays for v1; only introduce trie/NFA if profiling shows need.
- Keep ability/resource checks in GAS (attributes, tags, costs, cooldowns), not in the raw input solver.
- Keep transition logic in State Tree; avoid hidden transition rules in input or ability glue code.

## Defaults (until tuned)
- `MaxBufferFrames = 30`
- `SimultaneousMergeFrames = 2`
- `DefaultInputWindowFrames = 20`

## Testing Requirements
Every meaningful change to combat input must include (or update) tests:
- unit: buffer expiry/wraparound
- unit: sequence matcher alternatives
- unit: simultaneous merge edge cases
- unit: charge hold/release boundaries
- unit: arbitration determinism
- integration: token stream correctness + same-frame overlap of token and command intent
- replay: identical input stream => identical command stream

## Performance and Safety
- Avoid per-frame heap allocations in hot paths (`Tick`, match solve, preprocess).
- Avoid non-deterministic containers/iteration order in arbitration-critical code paths.
- Document any behavior that intentionally trades strictness for leniency.

## Change Workflow for Agents
When implementing combat-input-related tasks:
1. Discover candidate docs in `Doc/` and read metadata first.
2. Read only relevant `Doc/*.md` sections.
3. State assumptions in PR/commit notes.
4. Implement minimal, data-driven change.
5. Add/update tests for determinism and edge cases.
6. Add or update debug trace output if matching behavior changed.
7. Summarize tuning knobs affected (windows, priorities, consume behavior).

## Out of Scope for v1 (do not add unless requested)
- rollback netcode sync
- full editor GUI authoring toolchain
- AI planner integration beyond scripted test input

## If Docs Are Missing or Ambiguous
- Do not invent large mechanics silently.
- Implement the smallest deterministic default and add a `TODO(doc)` note pointing to `Doc/`.
