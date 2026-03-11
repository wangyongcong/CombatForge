# CombatSystem

`CombatSystem` is an Unreal Engine 5.7 project focused on deterministic, data-driven combat for third-person action games, with reference points including Sekiro, Stellar Blade, and Dynasty Warriors.

The current gameplay architecture is built around three required UE systems:
- Gameplay Ability System for attributes, effects, costs, and ability activation
- Enhanced Input for physical input ingestion and logical command input
- State Tree for gameplay state flow and AI behavior

## Source of Truth

Design documents in [`Doc/`](C:/projects/CombatSystem/Doc) are the authoritative gameplay contracts for this project.

The primary combat-input spec is:
- [`Doc/Combat Input System.md`](C:/projects/CombatSystem/Doc/Combat%20Input%20System.md)

Current doc direction:
- deterministic, fixed-step input processing
- data-driven command definitions
- buffered command matching with stable arbitration
- command intent emission owned by the input runtime
- legality checks and move execution owned by GAS and State Tree integration

If code and docs disagree, the intended workflow is to either align code to docs or update docs and changelog together with explicit rationale.

## Project Structure

Key top-level directories:
- [`Source/`](C:/projects/CombatSystem/Source): C++ game module and gameplay variants
- [`Content/`](C:/projects/CombatSystem/Content): Unreal assets
- [`Config/`](C:/projects/CombatSystem/Config): project and engine configuration
- [`Doc/`](C:/projects/CombatSystem/Doc): gameplay and system design documents
- [`Plugins/`](C:/projects/CombatSystem/Plugins): local project plugins, including `CombatForge`

Main code organization under [`Source/CombatSystem/`](C:/projects/CombatSystem/Source/CombatSystem):
- `Variant_Combat`: combat-focused character, AI, animation notify, gameplay, interface, and UI code
- `Variant_Platforming`: platforming-focused gameplay variant
- `Variant_SideScrolling`: side-scrolling gameplay variant

## Key Dependencies

The project module depends on:
- `EnhancedInput`
- `CombatForge`
- `AIModule`
- `StateTreeModule`
- `GameplayStateTreeModule`
- `UMG`
- `Slate`

Enabled project plugins include:
- `StateTree`
- `GameplayStateTree`
- `GameplayAbilities`
- `CombatForge`
- `TestFramework`
- `FunctionalTestingEditor`

## Combat Input Runtime Notes

The current combat-input design in the doc describes a MUGEN-inspired runtime with these constraints:
- frame-based timing only
- fixed-size recent input buffering
- normalized cardinal direction storage with inferred diagonals
- command solving triggered on input-state changes
- deterministic candidate ordering and arbitration
- debug visibility for timeline and rejection reasons in development builds

Default tuning values currently called out in project guidance:
- `MaxBufferFrames = 30`
- `SimultaneousMergeFrames = 2`
- `DefaultInputWindowFrames = 20`

## Working In This Repo

When making combat-input changes:
- inspect `Doc/` first and read metadata before diving into full documents
- keep behavior deterministic and data-driven
- do not move combo/state-machine logic into the raw input solver
- add or update tests for buffer behavior, matching, charge logic, arbitration, and replay determinism
- preserve clear ownership boundaries between input runtime, GAS, and State Tree

## Getting Started

1. Open [`CombatSystem.uproject`](C:/projects/CombatSystem/CombatSystem.uproject) with Unreal Engine 5.7.
2. Allow the editor to generate project files if needed.
3. Build the `CombatSystemEditor` target from Visual Studio or Rider.
4. Review [`Doc/Combat Input System.md`](C:/projects/CombatSystem/Doc/Combat%20Input%20System.md) before changing combat-input behavior.

## Repository Hygiene

Local build outputs, caches, and IDE settings are ignored via [`/.gitignore`](C:/projects/CombatSystem/.gitignore).
