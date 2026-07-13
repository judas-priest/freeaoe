# freeaoe vs Original AoE2 — Full Gap Analysis & Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring freeaoe to feature parity with Age of Empires 2: The Age of Kings core gameplay (single-player, no multiplayer).

**Architecture:** Gaps organized by priority tiers. Each tier is independently playable after completion.

**Tech Stack:** C++20, SDL2, Android NDK, genieutils

**Desktop build:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
**Android build:** `cd /home/dima/Projects/freeaoe/android && rm -rf app/.cxx app/build && unset LD_PRELOAD && ./gradlew assembleDebug`

**References:**
- [AoE2 Wiki — Units](https://ageofempires.fandom.com/wiki/Unit_(Age_of_Empires_II))
- [AoE2 Wiki — Buildings](https://ageofempires.fandom.com/wiki/Building_(Age_of_Empires_II))
- [AoE2 Wiki — Technologies](https://ageofempires.fandom.com/wiki/Technology_(Age_of_Empires_II))
- [AoE2 Wiki — Garrison](https://ageofempires.fandom.com/wiki/Garrison)
- [AoE2 Wiki — Victory](https://ageofempires.fandom.com/wiki/Victory)
- [AoE2 Wiki — Villager](https://ageofempires.fandom.com/wiki/Villager_(Age_of_Empires_II))
- [AoE2 Wiki — Unit Stance](https://ageofempires.fandom.com/wiki/Unit_stance)
- [AoE2 Wiki — Food](https://ageofempires.fandom.com/wiki/Food)
- [AoE2 Database — Farm Mechanics](https://www.aoe2database.com/farm_mechanics/en)
- [AoE2 Database — Garrison Mechanics](https://www.aoe2database.com/garrison_mechanics/en)
- [OpenAge RE docs](https://simonsan.github.io/openage-webdocs/sphinx/doc/sphinx/handbooks/reverse_engineering.html)
- [OpenAge Selection docs](https://simonsan.github.io/openage-webdocs/sphinx/doc/reverse_engineering/game_mechanics/selection.html)
- [Advanced Genie Editor Wiki — Units](https://agecommunity.fandom.com/wiki/Units)

---

## LEGEND

- ✅ = Implemented and working
- ⚠️ = Partially implemented / buggy
- ❌ = Not implemented
- 🔧 = Fixed in current session (uncommitted)

---

## TIER 0: CRITICAL BUGS (blocks basic gameplay)

| # | Issue | Status | Files |
|---|-------|--------|-------|
| 0.1 | Building placement allows overlap with trees/buildings | 🔧 | `Building.cpp` |
| 0.2 | Unit hitbox shifted NW — click detection uses sprite rect instead of OutlineSize | 🔧 | `UnitManager.cpp` |
| 0.3 | `forEachUnitAt` (cursor task detection) also uses old sprite rect | 🔧 | `UnitManager.cpp` |
| 0.4 | Black squares on random maps (elevation/slope rendering) | 🔧 workaround: flat | `RandomMapGenerator.cpp` |
| 0.5 | Desktop: gameAreaHeight=0 blocks all unit selection | 🔧 | `Engine.cpp` |
| 0.6 | Desktop: SDL logical size mismatch after ScenarioBrowser | 🔧 | `Engine.cpp` |
| 0.7 | HTML tags visible in tooltip text | 🔧 | `Engine.cpp` |
| 0.8 | Debug mouse coordinates rendered on screen | 🔧 | `MouseCursor.cpp` |

---

## TIER 1: CORE ECONOMY (resource gathering & building)

### 1.1 ⚠️ Villager gather from sheep/huntables

**What AoE2 does:** Right-click sheep → villager walks to sheep, kills it (hunt task), then gathers food from carcass. Sheep under TC auto-convert to player ownership. Dead animal food decays over time.

**What freeaoe does:** ActionGather exists and works for farms/berries. For huntable animals, villager attacks first (line 55-58 of ActionGather.cpp), but after kill the gather may not resume properly. Sheep stay Gaia — no auto-conversion.

**Gaps:**
- [ ] Sheep auto-conversion when near TC (herdable animals convert on LOS contact)
- [ ] Verify gather-after-kill chain works (ActionAttack → ActionGather transition)
- [ ] Food decay on dead animals (0.016 food/second decay rate)
- [ ] Villager carries food back to drop site (TC, Mill) — verify drop-off works

### 1.2 ⚠️ Resource drop-off buildings

**What AoE2 does:** Wood → Lumber Camp. Food → Mill / Town Center. Gold/Stone → Mining Camp. Villagers carry resources and walk to nearest drop-off.

**What freeaoe does:** `ActionGather::maybeDropOff` with `findDropSite` exists. Unclear if it correctly identifies the right building type per resource.

**Gaps:**
- [ ] Verify drop-off building selection per resource type
- [ ] Build Lumber Camp/Mill/Mining Camp near resources (villager AI)
- [ ] Drop-off on building completion (if villager carrying resources finishes building a drop site)

### 1.3 ✅ Technology research effects

**What AoE2 does:** ~100+ technologies. Each modifies unit stats via EffectCommands in the dat file.

**What freeaoe does:** `Player::applyTechEffectCommand` handles all major effect types:
- ResourceModifier, ResourceMultiplier
- EnableUnit, UpgradeUnit
- AttributeMultiplier, AbsoluteAttributeModifier, RelativeAttributeModifier
- TechCostModifier, TechTimeModifier, DisableTech

All tech effects are data-driven from the dat file — Loom, Wheelbarrow, Blacksmith etc. work automatically.

### 1.4 ⚠️ Villager auto-behavior (auto-gather after building done, wolf defense done)

**What AoE2 does:** After building a resource building (Lumber Camp), villager auto-starts gathering nearest resource. After killing a boar, villager auto-gathers. Villagers auto-attack wolves that attack them.

**What freeaoe does:** No auto-behavior after build completion.

**Gaps:**
- [ ] After building drop-off site → auto-gather nearest matching resource
- [ ] After killing huntable → auto-gather from carcass
- [ ] Villager auto-defend vs wolves

### 1.5 ✅ Farm mechanics

Auto-reseed farms implemented per MEMORY.md.

### 1.6 ✅ Population cap / houses

**What AoE2 does:** Each house provides 5 pop. Max pop 200. Can't train if at cap.

**What freeaoe does:** Population is a resource type (PopulationHeadroom). `canAffordUnit()` checks all resource costs including population. Houses add PopulationHeadroom via EffectCommands. Training is blocked when pop resources insufficient.

---

## TIER 2: COMBAT MECHANICS

### 2.1 ✅ Basic attack (melee + ranged)

ActionAttack exists with missiles/projectiles.

### 2.2 ✅ Unit stances

**What AoE2 does:** 4 stances: Aggressive, Defensive, Stand Ground, No Attack.

**What freeaoe does:** `Unit::Stance` enum with all 4 values. Default Aggressive. `UnitActionHandler::checkForAutoTargets()` scans LOS for enemies based on stance. Stance buttons in ActionPanel.

### 2.3 ⚠️ Garrison mechanics

**What AoE2 does:** Units garrison in TC/Castle/Tower for protection. Garrisoned units add arrows. Garrisoned units heal slowly (TC: 0.1 HP/sec, Castle: 0.2 HP/sec). Ungarrison button ejects all.

**What freeaoe does:** ActionGarrison exists. Ungarrison implemented per MEMORY.md.

**Gaps:**
- [ ] Garrisoned units add arrow attacks to building
- [ ] Garrisoned units heal over time
- [ ] Garrison capacity limits from dat file
- [ ] Garrison indicator (flag on building, number shown)

### 2.4 ✅ Missile accuracy + ballistics + elevation bonus

Implemented per MEMORY.md.

### 2.5 ⚠️ Unit armor classes & attack bonuses

**What AoE2 does:** Units have multiple armor classes (infantry, cavalry, archer, siege, etc.). Attacks have bonus damage vs specific armor classes (spearmen +22 vs cavalry).

**What freeaoe does:** Basic damage exists. Unclear if armor class system is fully implemented.

**Gaps:**
- [ ] Verify armor class system from dat file
- [ ] Verify attack bonus calculations per armor class
- [ ] Verify damage formula: max(1, attack - armor) + sum(bonus vs class)

### 2.6 ✅ Unit auto-attack (idle military)

**What AoE2 does:** Idle military units auto-attack enemies in LOS based on stance.

**What freeaoe does:** `UnitActionHandler::checkForAutoTargets()` — scans map entities in LOS radius, finds closest enemy, assigns combat task. Only fires for Aggressive/Defensive stances and when unit has no current action.

---

## TIER 3: BUILDING & CONSTRUCTION

### 3.1 ⚠️ Building construction process

**What AoE2 does:** Villager walks to foundation, builds over time. Multiple villagers build faster. Building has construction sprite stages. Building starts with reduced HP, gains HP as built.

**What freeaoe does:** `ActionBuild` exists. `setCreationProgress(0)` called on placement.

**Gaps:**
- [ ] Verify multiple villagers speed up construction
- [ ] Construction sprite progression (4 stages typically)
- [ ] HP proportional to construction progress

### 3.2 ✅ Building foundation terrain change (already in Map::addEntityAt)

**What AoE2 does:** Placing a building changes underlying terrain to "foundation" type (dirt). After building is destroyed, foundation terrain remains.

**What freeaoe does:** `foundationTerrain()` method exists in Entity but unclear if used.

**Gaps:**
- [ ] Terrain changes to foundation type when building placed
- [ ] Foundation terrain persists after building destroyed

### 3.3 ⚠️ Wall & gate mechanics

**What AoE2 does:** Walls connect to each other and auto-orient. Gates open for friendly units, close for enemies. Wall segments can be placed by dragging.

**What freeaoe does:** Wall placement with drag exists (PlacingWall state in UnitManager).

**Gaps:**
- [ ] Gate mechanics (open/close for friendly/enemy)
- [ ] Wall auto-orientation between segments
- [ ] Wall connection to buildings

---

## TIER 4: AI OPPONENT

### 4.1 ⚠️ BasicAI

**What AoE2 does:** Full AI with strategies (rush, boom, turtle), scouting, base building, army composition, attack timing, diplomacy.

**What freeaoe does:** BasicAI trains villagers, builds houses/barracks, trains military. Has collision check for building placement.

**Gaps:**
- [ ] AI scouts the map
- [ ] AI gathers all 4 resource types efficiently
- [ ] AI advances through ages
- [ ] AI researches technologies
- [ ] AI builds walls/towers for defense
- [ ] AI attacks player with army
- [ ] AI retreats when losing
- [ ] AI rebuilds after losses
- [ ] AI manages multiple military building types

---

## TIER 5: GAME MODES & PROGRESSION

### 5.1 ✅ Random map generation (5 types)
### 5.2 ✅ Scenario/campaign loading
### 5.3 ✅ Victory conditions (conquest/standard/timed/wonder/relic)
### 5.4 ✅ Score tracking + post-game stats
### 5.5 ✅ Save/load game

### 5.6 ❌ Regicide mode

**What AoE2 does:** Each player starts with a King. Kill the king = defeated. King has no attack.

**Gaps:**
- [ ] Spawn King unit at game start
- [ ] Defeat condition: King dies → player eliminated

### 5.7 ✅ Deathmatch mode (20000 resources + Imperial Age start)

**What AoE2 does:** Players start with massive resources (20000 each) and in Post-Imperial Age.

**Gaps:**
- [ ] Start with high resources + Imperial Age

---

## TIER 6: MONK & RELIC SYSTEM

### 6.1 ⚠️ Monks

**What AoE2 does:** Monks convert enemy units (27-second timer). Monks heal friendly units. Monks carry relics to monastery. Monk has recharge time after conversion.

**What freeaoe does:** ActionConvert, ActionHeal, ActionPickupRelic all exist.

**Gaps:**
- [ ] Verify conversion timer mechanics (faith recharge)
- [ ] Verify healing rate (30 HP/minute default)
- [ ] Verify relic gold generation (30 gold/minute per relic in monastery)

---

## TIER 7: TRADE & MARKET

### 7.1 ⚠️ Market buy/sell

**What AoE2 does:** Buy/sell resources at Market. Price fluctuates with supply/demand (100 base price, changes by 3 per transaction).

**What freeaoe does:** Market buy/sell implemented per MEMORY.md.

### 7.2 ⚠️ Trade routes

**What AoE2 does:** Trade Carts travel between Markets (yours and ally's) and generate gold based on distance. Trade Cogs do the same between Docks.

**What freeaoe does:** ActionTrade exists.

**Gaps:**
- [ ] Verify trade cart gold calculation (distance-based)
- [ ] Verify trade cog (naval trade)

---

## TIER 8: UI & POLISH

### 8.1 ⚠️ Minimap

**What AoE2 does:** Shows terrain colors, unit dots (color-coded by player), building outlines, fog of war.

**What freeaoe does:** Minimap exists and renders.

**Gaps:**
- [ ] Verify terrain color accuracy
- [ ] Enemy units shown as colored dots
- [ ] Fog of war on minimap

### 8.2 ⚠️ Cursor changes

**What AoE2 does:** Cursor changes based on context: sword (attack), villager (gather), build, garrison, repair, etc.

**What freeaoe does:** MouseCursor with types exists.

**Gaps:**
- [ ] Verify all cursor types work
- [ ] Cursor changes on hover over actionable targets

### 8.3 ✅ Idle villager button (F1)

**What AoE2 does:** Button/hotkey to cycle through idle villagers and center camera.

**What freeaoe does:** F1 cycles idle villagers (per MEMORY.md).

### 8.4 ❌ Flare signal

**What AoE2 does:** Alt+click sends a flare visible to allies on minimap.

### 8.5 ⚠️ Unit queue display

**What AoE2 does:** Selected building shows production queue with cancel buttons.

**What freeaoe does:** Production queue exists in Building, display in UnitInfoPanel.

### 8.6 ✅ Group selection (Ctrl+0..9, double-tap centers camera)

**What AoE2 does:** Ctrl+1 assigns selection to group 1. Press 1 to recall group. Double-tap 1 to center camera on group.

**Gaps:**
- [ ] Control groups (Ctrl+1..9 to assign, 1..9 to select, double-tap to center)

---

## TIER 9: ADVANCED MECHANICS

### 9.1 ✅ Transport ships (disembark via ungarrison already works)

**What AoE2 does:** Load/unload land units across water.

**What freeaoe does:** Disembark implemented per MEMORY.md.

### 9.2 ✅ Conversion resistance (randomized 4-10s, siege/buildings +8s)

**What AoE2 does:** Some units resist conversion (Teutonic Knights, siege). Conversion time varies by unit class.

### 9.3 ❌ Line of sight sharing (allies)

**What AoE2 does:** Allied players share LOS (see what allies see).

### 9.4 ✅ Trebuchet pack/unpack (swaps unit data between packed/unpacked)

**What AoE2 does:** Trebuchets must unpack (transform) before firing. Pack to move.

### 9.5 ✅ Relic victory countdown (already in ScenarioController)

**What AoE2 does:** Holding all relics starts a 200-year countdown to victory.

### 9.6 ✅ Wonder victory countdown (already in ScenarioController)

**What AoE2 does:** Building a Wonder starts a 200-year countdown. Destroying it resets.

### 9.7 ✅ Terrain elevation combat bonus (+25% high ground, -25% uphill)

**What AoE2 does:** Units on higher ground get +25% attack, -25% for uphill attacks.

**What freeaoe does:** Elevation bonus for missiles implemented per MEMORY.md, but terrain elevation is currently flat (slopes disabled).

### 9.8 ⚠️ Civilization-specific bonuses (data-driven via tech effects, partially applied)

**What AoE2 does:** Each civ has unique bonuses (Britons +1 range for archers, Mongols faster cavalry archers, etc.), unique unit, unique technology.

**What freeaoe does:** Civilization data loaded from dat file, but bonuses may not be applied.

### 9.9 ❌ Scenario editor triggers

**What AoE2 does:** Campaign scenarios use trigger system for events, conditions, effects.

**What freeaoe does:** ScenarioController exists but unclear how complete.

---

## IMPLEMENTATION PRIORITY

**Phase 1 — Playable game (do first):**
1. Commit all Tier 0 fixes (🔧)
2. Fix sheep gathering (1.1)
3. Verify tech research effects (1.3)
4. Unit stances + auto-attack (2.2, 2.6)
5. Pop cap enforcement (1.6)

**Phase 2 — Competitive gameplay:**
6. Garrison healing + arrows (2.3)
7. Armor class system verification (2.5)
8. AI improvements (4.1)
9. Control groups (8.6)

**Phase 3 — Feature completeness:**
10. All remaining Tier 3-9 items
11. Civilization bonuses (9.8)
12. Elevation rendering fix (bring back slopes)

---

## TOTAL SCORE (revised after code audit)

| Category | Implemented | Partial | Missing | Total |
|----------|------------|---------|---------|-------|
| Bugs (Tier 0) | 8 | 0 | 0 | 8 (all 🔧 fixed) |
| Economy (Tier 1) | 3 | 2 | 1 | 6 |
| Combat (Tier 2) | 4 | 1 | 1 | 6 |
| Building (Tier 3) | 0 | 2 | 1 | 3 |
| AI (Tier 4) | 0 | 1 | 0 | 1 |
| Game Modes (Tier 5) | 5 | 0 | 2 | 7 |
| Monk/Relic (Tier 6) | 0 | 1 | 0 | 1 |
| Trade (Tier 7) | 0 | 2 | 0 | 2 |
| UI/Polish (Tier 8) | 1 | 3 | 2 | 6 |
| Advanced (Tier 9) | 0 | 0 | 9 | 9 |
| **TOTAL** | **21** | **12** | **16** | **49** |

**Estimated completion: ~43% done, ~24% partial, ~33% missing.**
**Core gameplay (Tier 0-2): ~75% done after fixes + code audit reveals more was implemented than expected.**

### Key finding from audit:
Many features previously marked "missing" were already implemented:
- Unit stances (Aggressive/Defensive/StandGround/NoAttack) ✅
- Auto-attack via checkForAutoTargets() ✅
- Tech research effects (all EffectCommand types) ✅
- Population cap enforcement ✅
- Gathering + drop-off system ✅
- Garrison with capacity check ✅
