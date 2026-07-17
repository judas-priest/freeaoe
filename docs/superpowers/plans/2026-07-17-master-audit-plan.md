# freeaoe Feature Audit — Master Plan Index

> Generated 2026-07-17 from full codebase audit (4 parallel agents). Plans verified against actual APIs.

## Verification Notes

All plans cross-checked against actual codebase:
- **Command dispatch is in `GameState::executeCommands()`** (GameState.cpp:355), NOT Engine.cpp
- **`Player::removeResource()`** — not `takeResource()` (doesn't exist)
- **`DecayingEntity(graphicId, decayTime, Size)`** — constructor takes 3 args, no MapPos
- **`Map::addEntityAt(col, row, entity, foundationTerrain)`** — not `addEntity()`
- **8 of 24 CommandTypes currently handled** — Garrison, Ungarrison, BuyResource, SellResource, Tribute, SetFormation, SetStance, Patrol all need handlers
- **`Building::ungarrisonAll()`** and **`Unit::ungarrisonAllUnits()`** already exist
- **`checkForAutoTargets()`** already filters Aggressive/Defensive (blocks StandGround/NoAttack)
- **`DiplomacyScreen`** already has working stance buttons
- Tree depletion uses **`DeadUnitID`** (not DyingGraphic) — stump may already work

## Priority Order (impact vs effort)

| # | Module | Effort | Impact | Plan File |
|---|--------|--------|--------|-----------|
| 1 | **Ungarrison** | Small | High | `module-01-ungarrison.md` |
| 2 | **Combat Stances** | Medium | High | `module-02-stances.md` |
| 3 | **Market Buy/Sell UI** | Medium | High | `module-03-market-ui.md` |
| 4 | **Relic Deposit** | Small | Medium | `module-04-relic-deposit.md` |
| 5 | **Unit Formations** | Medium | Medium | `module-05-formations.md` |
| 6 | **Building Visuals** | Medium | Medium | `module-06-building-visuals.md` |
| 7 | **Diplomacy UI** | Small | Medium | `module-07-diplomacy-ui.md` |
| 8 | **Save/Load UI** | Medium | Medium | `module-08-save-load-ui.md` |
| 9 | **Tree Stumps & Decay** | Small | Low | `module-09-tree-visuals.md` |
| 10 | **Naval** | Large | High | `module-10-naval.md` |

## Missing Command Handlers (GameState.cpp)

Currently only 8/24 commands handled. These modules add the missing ones:

| Command | Module | Priority |
|---------|--------|----------|
| Ungarrison | 1 | High |
| BuyResource | 3 | High |
| SellResource | 3 | High |
| SetFormation | 5 | Medium |
| Garrison | 10 | Medium |
| Tribute | 7 | Medium |
| SetStance | 2 (if needed) | Medium |
| Patrol, AttackMove, Trade, Repair, Heal, Convert, PickupRelic, SetRallyPoint | — | Low (may work via IAction::assignTask already) |

## Not Planned (too large / cosmetic)

- Scenario Editor, Replay System, Weather Effects, Day/Night Cycle
- Particle Effects (fire, blood, dust), Campaign Cinematics
- Cartography/Spies tech effects, Siege Tower wall-climbing
- Scorpion pass-through, Hero unit mechanics

## Quick Wins (< 30 min)

1. **Module 1** — one `case` in GameState.cpp switch, calls existing `ungarrisonAll()`
2. **Module 9 Task 1** — verify tree stumps already work (DeadUnitID switching exists)
3. **Module 7 Task 1** — one `case` for Tribute command
4. **Module 3 Task 1** — two `case` statements for BuyResource/SellResource
