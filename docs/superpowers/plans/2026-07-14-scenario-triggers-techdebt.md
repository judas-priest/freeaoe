# Scenario Editor Triggers — Tech Debt

## Current State

ScenarioController already handles triggers from .scn/.scx campaign files. Partial implementation exists.

### Implemented Conditions (10 of 19)
| # | Condition | Status |
|---|-----------|--------|
| 1 | BringObjectToArea | ✅ |
| 3 | OwnObjects | ✅ |
| 4 | OwnFewerObjects | ✅ |
| 5 | ObjectsInArea | ✅ |
| 6 | DestroyObject | ✅ |
| 8 | AccumulateAttribute | ✅ |
| 10 | Timer | ✅ |
| 11 | ObjectSelected | ✅ |
| 13 | PlayerDefeated | ✅ |
| 19 | DifficultyLevel | ✅ |

### Missing Conditions (9 of 19)
| # | Condition | Difficulty | Notes |
|---|-----------|------------|-------|
| 2 | BringObjectToObject | Medium | Like BringObjectToArea but target is a unit, need distance check |
| 7 | CaptureObject | Medium | Check if unit changed owner (conversion) |
| 9 | ResearchTechnology | Easy | Check `player->hasResearched(techId)` — need to track researched techs |
| 12 | AISignal | Hard | AI scripting system integration |
| 14 | ObjectHasTarget | Medium | Check `unit->actions.currentAction()->target` |
| 15 | ObjectVisible | Easy | Check `visibility->visibilityAt(unit->position()) == Visible` |
| 16 | ObjectNotVisible | Easy | Inverse of above |
| 17 | ResearchingTechnology | Medium | Check if building is currently researching specific tech |
| 18 | UnitsGarrisoned | Easy | Check `building->garrisonedUnits.size() >= amount` |

### Implemented Effects (16 of ~30)
| # | Effect | Status |
|---|--------|--------|
| 1 | ChangeDiplomacy | ✅ |
| 2 | ResearchTechnology | ✅ |
| 3 | SendChat | ✅ |
| 4 | Sound | ✅ |
| 5 | SendTribute | ✅ |
| 8 | ActivateTrigger | ✅ |
| 9 | DeactivateTrigger | ✅ |
| 11 | CreateObject | ✅ |
| 12 | TaskObject | ✅ |
| 13 | DeclareVictory | ✅ |
| 15 | RemoveObject | ✅ |
| 16 | ChangeView | ✅ |
| 20 | DisplayInstructions | ✅ |
| 22 | SetUnitStance | ✅ |
| 24 | DamageObject | ✅ |
| 26 | ChangeObjectName | ✅ (logged only) |
| 27 | ChangeObjectHP | ✅ |
| HD | HealObject | ✅ |

### Missing Effects (14 of ~30)
| # | Effect | Difficulty | Notes |
|---|--------|------------|-------|
| 6 | UnlockGate | Medium | Need gate open/close mechanic |
| 7 | LockGate | Medium | Same as above |
| 10 | AIScriptGoal | Hard | AI scripting integration |
| 14 | KillObject | Easy | Just call `unit->kill()` |
| 17 | Unload | Medium | Ungarrison units from transport/building at position |
| 18 | ChangeOwnership | Easy | `unit->setPlayer(newPlayer)` |
| 19 | Patrol | Easy | Set patrol action on matching units |
| 21 | ClearInstructions | Easy | Clear message display |
| 23 | UseAdvancedButtons | Low priority | UI toggle |
| 25 | PlaceFoundation | Medium | Create building at location with 0 progress |
| 28 | ChangeObjectAttack | Medium | Modify unit attack stats at runtime |
| 29 | StopUnit | Easy | `unit->actions.clearActionQueue()` |
| 30+ | ChangeSpeed/Range/Armor | Medium | Modify unit stats, HD Edition extensions |

## Priority

### P0 — Easy wins (enables most campaigns)
1. **KillObject** effect — `unit->kill()` on matching units
2. **ChangeOwnership** effect — `unit->setPlayer(newPlayer)`
3. **StopUnit** effect — clear action queue
4. **Patrol** effect — set patrol action
5. **ClearInstructions** effect — clear messages
6. **ResearchTechnology** condition — track researched techs per player
7. **UnitsGarrisoned** condition — check garrison count
8. **ObjectVisible/NotVisible** conditions — check visibility map

### P1 — Medium effort
9. **BringObjectToObject** condition — distance check between two units
10. **CaptureObject** condition — detect ownership change
11. **ObjectHasTarget** condition — check current action target
12. **Unload** effect — ungarrison at position
13. **PlaceFoundation** effect — create building with 0 progress
14. **ChangeObjectAttack** effect — modify combat stats

### P2 — Hard / low priority
15. **Gate mechanics** (UnlockGate/LockGate) — needs gate state system
16. **AIScriptGoal/AISignal** — needs AI script system integration
17. **HD extensions** (ChangeSpeed/Range/Armor/AttackMove) — stat modifiers

## Files
- `src/mechanics/ScenarioController.cpp` — main trigger logic (lines 485-720)
- `src/mechanics/ScenarioController.h` — Condition/Trigger structs
- `src/extern/genieutils/include/genie/script/scn/Trigger.h` — TriggerCondition/TriggerEffect enums

## Estimated Effort
- P0 (8 items): ~2-3 hours — would unlock most campaign scenarios
- P1 (6 items): ~4-5 hours
- P2 (3 items): ~8+ hours (gate system alone is complex)
- **Total: ~15 hours for full trigger parity**
