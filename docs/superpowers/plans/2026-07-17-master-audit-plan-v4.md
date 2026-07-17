# freeaoe Full Audit v4 — Corrected Master Plan Index

> Generated 2026-07-17 from 4-agent audit + 4-agent deep code verification. Continues from modules 01-29.

## Audit Corrections (v3 -> v4)

Deep code research revealed MORE "missing" features that **already work**:

| Initially "Missing" | Actual Status | Evidence |
|---|---|---|
| Bonus damage by armor class (counter-units) | WORKS | Nested loop in ActionAttack.cpp:235-248 and Missile.cpp:310-321 iterates all attack classes vs armor classes from .dat; pikemen anti-cavalry etc. is data-driven |
| Unit upgrade chains (militia->MAA->champion) | WORKS | `EffectCommand::UpgradeUnit` in Player.cpp:178-191 iterates all units and calls `setUnitData()` when tech researched |
| Splash/blast damage (area of effect) | WORKS | Missile.cpp:245-281 scans 3x3 tile grid, collects hit units within blast radius, deals damage to all (lines 293-331) |
| Shift-queue waypoints | WORKS | UnitManager.cpp:751 `shiftHeld ? AssignType::Queue : AssignType::Replace`; Engine.cpp:2106 detects shift via SDL_GetModState |
| Idle villager button | WORKS | ActionPanel FindIdleVillager command + F1 key in Engine.cpp:1596-1630 |
| Control groups (Ctrl+1-9) | WORKS | Engine.cpp:1625-1689, m_controlGroups[10], double-tap centers camera |
| Background music | WORKS | Engine.cpp:395-403 plays random xmusic{N}.mp3; HomeScreen plays open.mp3/open.mid |
| Shallows terrain | WORKS | TerrainId 4 = shallows, used in Rivers map for fords |
| Market buy/sell code | WORKS | Player::MarketPrices with dynamic pricing, buy/sell multipliers, ActionTrade |
| Fishing boats | WORKS | Dock/Fisher infrastructure, fish traps auto-reseed |
| Minimap modes (Normal/Economic/Diplomatic) | WORKS | Minimap.h MinimapMode enum, cycled via F4 |

## Modules 30-34: Genuine Remaining Gaps (after v4 corrections)

### Tier 1 — Gameplay Impact

| # | Module | Effort | Impact | Plan File |
|---|--------|--------|--------|-----------|
| 30 | **Elevation LOS Bonus** | Small | High | `module-30-elevation-los.md` |
| 31 | **Hunt/Carcass Food Decay** | Small | Medium | `module-31-hunt-decay.md` |

### Tier 2 — QoL / Noticeable Absence

| # | Module | Effort | Impact | Plan File |
|---|--------|--------|--------|-----------|
| 32 | **Double-Click Select Same Type** | Small | Medium | `module-32-double-click-select.md` |
| 33 | **Minimap Flares/Pings** | Medium | Medium | `module-33-minimap-flares.md` |

### Tier 3 — Content / Polish

| # | Module | Effort | Impact | Plan File |
|---|--------|--------|--------|-----------|
| 34 | **Campaign Briefing Screens** | Medium | Low | `module-34-campaign-briefings.md` |

## Not Planned (separate project scope)

- Scenario Editor (full rewrite needed, currently stub)
- Replay system (lockstep infrastructure exists but no consumer)
- .rms random map script parsing (15 hardcoded types sufficient)
- NAT traversal / matchmaking
- Post-game statistics timeline/graphs
- BIK video playback for cinematics
- Group pathfinding / flow-field
- Campaign save state / unlock tracking
- Formation movement enforcement (defined, positions calculated, but enforcing during movement is complex pathfinding work)
- Gate lock/unlock player UI (works via triggers, player UI is polish)
- Building decay without repair (not in base AoE2 either — buildings don't decay)

## Implementation Order (Recommended)

1. Module 30 (Elevation LOS) — ~20 lines, self-contained in Unit.cpp
2. Module 31 (Hunt Decay) — ~15 lines, self-contained in ActionGather.cpp
3. Module 32 (Double-Click Select) — ~30 lines in Engine.cpp + UnitManager
4. Module 33 (Minimap Flares) — ~60 lines across Engine/Minimap/multiplayer
5. Module 34 (Campaign Briefings) — ~150 lines, new UI screen

## Completeness Assessment

With modules 01-29 (mostly done) + corrections above:
- **~85% of AoE2 gameplay features are implemented**
- Core combat, economy, tech tree, AI, campaigns, multiplayer all functional
- Remaining 15% is polish, editor, replay, and advanced features

## What Was Already Done

- Modules 01-10: See `master-audit-plan.md`
- Modules 11-20: See `master-audit-plan-v2.md`
- Modules 21-29: See `master-audit-plan-v3.md`
