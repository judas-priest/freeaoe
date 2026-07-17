# freeaoe Full Audit v3 — Master Plan Index

> Generated 2026-07-17 from deep 6-agent codebase audit with code verification. Continues from modules 01-20 (implemented).

## Audit Corrections

Deep code research revealed several "missing" features from v2 audit **already work**:

| Initially "Missing" | Actual Status | Evidence |
|---|---|---|
| Building prerequisites (tech/age gating) | WORKS | `canAffordUnit()` checks `unit.Enabled`; `enableUnit()`/`disableUnit()` driven by .dat tech cascade |
| Age building requirements | WORKS | `Player::setAge()` triggers full implicit tech cascade loop (`Player.cpp:223-260`) |
| Gathering rate techs | WORKS | `applyUnitAttributeModifier()` modifies `Action.WorkRate` (attr 13); `ActionGather.cpp:90` reads it |
| Building speed from techs | N/A | Original AoE2 doesn't have build speed techs; `3/(n+2)` formula is correct |
| Gates (open/close) | WORKS | `Gate.h/cpp` — auto-open for allies, lock/unlock UI, sprite swap |
| Naval combat | WORKS | Ships use standard `ActionAttack` pipeline |
| Transport load/unload | WORKS | `ActionGarrison.cpp:32-62` + Disembark command |

## Modules 21-29: Genuine Remaining Gaps

### Tier 1 — Gameplay Impact

| # | Module | Effort | Impact | Plan File |
|---|--------|--------|--------|-----------|
| 21 | **Team Bonuses** | Small | High | `module-21-team-bonuses.md` |
| 22 | **Shared Vision** | Medium | High | `module-22-shared-vision.md` |
| 23 | **Game Mode Mechanics** | Medium | High | `module-23-game-modes.md` |

### Tier 2 — Noticeable Absence

| # | Module | Effort | Impact | Plan File |
|---|--------|--------|--------|-----------|
| 24 | **Double-Click Select Same Type** | Small | Medium | `module-24-double-click-select.md` |
| 25 | **Voice Notifications** | Small | Medium | `module-25-voice-notifications.md` |
| 26 | **Chat & Taunts** | Medium | Medium | `module-26-chat-taunts.md` |
| 27 | ~~AI Script (.per) Loading~~ | ~~Large~~ | ~~Medium~~ | ALREADY IMPLEMENTED (ScriptLoader + bison parser + GameState.cpp:903) |

### Tier 3 — Polish / Content

| # | Module | Effort | Impact | Plan File |
|---|--------|--------|--------|-----------|
| 28 | **Additional Map Types** | Large | Low | `module-28-map-types.md` |
| 29 | **Tech Tree Viewer** | Large | Low | `module-29-tech-tree-viewer.md` |

## Not Planned (separate project scope)

- Scenario Editor (full rewrite needed)
- Replay system
- NAT traversal / matchmaking
- Post-game statistics timeline/graphs
- BIK video playback
- Group pathfinding / flow-field
- Campaign save state / unlock tracking

## Implementation Order (Recommended)

1. Module 21 (Team Bonuses) — ~30 lines, data-driven, self-contained
2. Module 22 (Shared Vision) — complements team bonuses, ~50 lines
3. Module 24 (Double-Click Select) — small QoL, ~20 lines
4. Module 25 (Voice Notifications) — atmosphere, ~40 lines
5. Module 23 (Game Modes) — KotH + Deathmatch + victory UI
6. Module 26 (Chat & Taunts) — multiplayer essential
7. ~~Module 27 (AI Scripts) — ALREADY IMPLEMENTED~~
8. Module 28 (Map Types) — content expansion
9. Module 29 (Tech Tree Viewer) — UI polish

## What Was Already Done

- Modules 01-10: See `master-audit-plan.md` (ungarrison, stances, market, relics, formations, building visuals, diplomacy, save/load UI, tree visuals, naval)
- Modules 11-20: See `master-audit-plan-v2.md` (conquest victory, AI farms, map resources, patrol/guard, age advance, save/load v3, game setup, terrain elevation, AI buildings, tribute tax)
