# freeaoe Full Audit v5 — Final Corrected Master Plan

> Generated 2026-07-17. Deep code verification of all "missing" features.

## Audit Corrections (v4 -> v5)

| Initially "Missing" | Actual Status | Evidence |
|---|---|---|
| Market buy/sell UI | WORKS | ActionPanel: BuyFood/SellFood/SellWood/BuyStone/SellStone commands with dynamic pricing (lines 830-881) |
| Gate lock/unlock UI | WORKS | ActionPanel shows CloseGate/OpenGate when gate selected (lines 646-655, 1017-1024) |
| Campaign unlock/progression | WORKS | `loadNextCampaignScenario()` in Engine.cpp (lines 2538-2565) |
| Formation movement enforcement | WORKS | UnitManager::onRightClick (lines 799-891) computes per-unit positions with rotation |
| AI walling | WORKS | BasicAI::buildWalls() places stone/palisade walls in circle around TC (lines 839-965) |
| Post-game stats table | WORKS | Tab key shows Score/Kills/Lost/Razed/Techs table (Engine.cpp lines 1001-1081) |
| Double-click select same type | WORKS | `event.mouseButton.clicks >= 2` + `selectUnitsByType()` |
| Campaign briefings | WORKS | `m_showBriefing` overlay with `scenarioInstructions` |

## Modules 35-39: Final Genuine Gaps

### Tier 1 — Quick Fixes

| # | Module | Effort | Description | Plan File |
|---|--------|--------|-------------|-----------|
| 35 | **Wire Stat Counters** | Tiny | unitsKilled/buildingsRazed/techsResearched/totalResourcesGathered never incremented | `module-35-stat-counters.md` |
| 36 | **Ice/Bridges Terrain** | Small | Add terrain types to random map generator | `module-36-ice-bridges.md` |

### Tier 2 — Medium Effort

| # | Module | Effort | Description | Plan File |
|---|--------|--------|-------------|-----------|
| 37 | **AI Multi-Prong Attacks** | Medium | Split army into 2 groups, attack from different angles | `module-37-ai-multi-attack.md` |
| 38 | **Hotkey Config File** | Medium | JSON config + runtime rebinding | `module-38-hotkey-config.md` |

### Tier 3 — Separate Project

| # | Module | Effort | Description |
|---|--------|--------|-------------|
| 39 | **.rms Script Parser** | Very Large | Full scripting language for custom maps — not planned |

## Revised Completeness: ~93%

With the v5 corrections, the genuine remaining gaps are:
- Stat counter wiring (trivial)
- Ice/bridges terrain (cosmetic)
- AI multi-prong attacks (tactical improvement)
- Hotkey config (QoL)
- .rms scripts (large scope, not needed)
- Scenario editor (separate project)
- Replay system (separate project)
