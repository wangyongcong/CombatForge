# CombatSystemFramework.md Design Doc

## Summary

Create Doc/CombatSystemFramework.md as the high-level architecture contract for the reusable
CombatForge gameplay framework. The document should define system ownership and runtime data
flow across input, StateTree, GAS, animation, character/controller, and AI. It should be
written as a framework-level design doc, not an implementation tutorial.

Use the existing repo document style: short metadata header, concise summary, explicit
ownership boundaries, and clear terminology for commands, states, and system responsibilities.

## Key Content

- Introduce the framework purpose and scope:
    - reusable action-game foundation for player and AI characters
    - StateTree as the only state machine system for both player and AI
    - GAS as the ability/attribute/effect system
    - UCombatForgeInputComponent as the gameplay-facing input abstraction
- Define core public classes with CombatForge prefix:
    - ACombatForgeCharacter
    - ACombatForgeController
    - ACombatForgeAIController
    - UCombatForgeInputComponent
- Document UCombatForgeInputComponent as the common interface with two implementations:
    - MUGEN-style command input implementation
    - simple buffered-command implementation
- Define the StateTree contract:
    - reads pending commands and gameplay facts from shared context
    - decides legality, priority, and transitions
    - executes side effects via tasks
    - never owns input storage directly
- Define the GAS contract:
    - attributes, costs, cooldowns, gameplay effects, ability execution
    - state machine may trigger abilities, but does not replace ability logic
- Define animation contract:
    - gameplay writes locomotion/action state snapshot
    - ABP observes gameplay-approved state and movement facts
    - ABP does not own authoritative gameplay decisions
- Define AI contract:
    - AI uses StateTree too, not behavior tree
    - AI writes requests/intent through the same gameplay context model when possible

## Recommended Sections

- Title and metadata
- Summary
- Goals and non-goals
- Input pipeline
- GAS ownership
- Animation data flow
- AI data flow
- Gameplay tag taxonomy
- Example runtime flows
- Debug expectations
- Extension points / deferred systems

## Concrete Guidance To Capture

- Request tags and state tags must be separate namespaces:
    - Command.*
    - State.*
    - Status.*
- Preferred runtime flow:
    - input action -> input component emits command -> shared context stores pending command ->
    StateTree accepts/rejects -> task performs gameplay action -> movement/GAS update facts
    -> ABP observes final state
- Character movement and movement mode remain authoritative for physical locomotion facts such
as grounded/falling
- No custom movement component or custom anim instance is required in v1; leave them as future
extension points
- StateTree should read shared state/context rather than being called like an event dispatcher
- Avoid duplicate state machines between gameplay and animation

## Example Scenarios

- Jump:
    - input emits Command.Jump
    - StateTree validates grounded/not blocked
    - enter-state task calls Jump()
    - movement mode changes to falling
    - ABP observes airborne state
- Roll:
    - input emits Command.Roll
    - StateTree validates state and resource conditions
    - task activates roll logic or roll GAS ability
    - ABP observes roll/action state
- AI attack:
    - AI StateTree chooses attack state
    - task activates attack ability or action
    - completion delegate/event advances state

## Test/Acceptance Criteria For The Doc

- The document clearly assigns ownership for input, state transitions, abilities, animation,
and AI
- The document defines all main public framework classes named above
- The document explains the shared command-buffer concept independently of the two input
implementations
- The document includes at least one player flow and one AI flow
- The document explicitly states the deferred decision on custom movement component and custom
anim instance

## Assumptions

- File path: Doc/CombatSystemFramework.md
- Document should be framework-oriented and reusable across multiple games built on CombatForge
- GAS is the required system for abilities/attributes/effects; StateTree remains the required
state-machine system