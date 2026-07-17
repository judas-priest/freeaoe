# Civilizations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete civilizations -- civ bonuses from dat file, disabled techs/units, unique techs, team bonuses

**Architecture:**

The AoE2 civ system is **almost entirely data-driven** through the .dat file. Each `genie::Civ` in the dat file has:
- `TechTreeID` (int16_t) -- an **effect ID** that applies the civ's tech tree (disabling units/techs, setting bonuses). This is the **primary civ differentiation mechanism**.
- `TeamBonusID` (int16_t) -- an effect ID applied to all allies of this civ.
- Per-civ unit data (`Units` vector) -- each civ already has its own unit stats in the dat.
- Per-civ resources (`Resources` vector) -- starting resources including age-specific tech effect IDs.

The `Civilization` class already copies per-civ unit data and filters techs by `tech.Civ`. The `Player` class already has `applyTechEffect()` which processes effect commands including `EnableUnit`, `UpgradeUnit`, attribute modifiers, and resource modifiers.

**Current gaps (root cause analysis):**

1. **TechTreeID never applied** -- `genie::Civ::TechTreeID` holds an effect ID that disables/enables units and techs for the civ. It is read from the dat file but **never called** via `Player::applyTechEffect()`. This single missing call is why all civs see all techs and all units.

2. **DisableTech effect is a no-op** -- In `Player::applyTechEffectCommand()`, `genie::EffectCommand::DisableTech` (type 102) is handled with just `DBG << "Disable tech"` -- it logs but does nothing. The tech is never actually removed from the available techs map.

3. **TeamBonusID never applied** -- `genie::Civ::TeamBonusID` is read but never applied to allied players.

4. **Scenario disabled lists ignored** -- `ScnMainPlayerData::disables` contains `disabledTechs[16]`, `disabledUnits[16]`, `disabledBuildings[16]` per player, but `GameState::setupScenario()` never reads or applies them.

5. **TechCostModifier and TechTimeModifier are no-ops** -- Both log but do nothing (lines 158-165 of Player.cpp).

**Tech Stack:** C++20, SDL2

---

## Task 1: Apply TechTreeID on player creation (CRITICAL)

This is the single most important fix. Each civ in the dat file has a `TechTreeID` which is an effect ID. Applying it at player creation will automatically disable unavailable units/techs and apply civ bonuses.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

In the `Player` constructor, after `updateAvailableTechs()`, add:

```cpp
    // Apply civ tech tree effect -- this disables units/techs and applies civ bonuses
    const genie::Civ &civData = DataManager::Inst().civilization(civId);
    if (civData.TechTreeID >= 0) {
        applyTechEffect(civData.TechTreeID);
    }
```

Add include at the top if not present:
```cpp
#include "resource/DataManager.h"
```

Note: `DataManager.h` is already included in Player.cpp (line 16).

The full constructor becomes:

```cpp
Player::Player(const int id, const int civId, const std::shared_ptr<Map> &map, const ResourceMap &startingResources) :
    playerId(id),
    civilization(civId),
    name("Player " + std::to_string(id)),
    visibility(std::make_shared<VisibilityMap>(id)),
    m_resourcesAvailable(civilization.startingResources()),
    m_map(map)
{
    // Override
    for (const std::pair<const genie::ResourceType, float> &r : startingResources) {
        m_resourcesAvailable[r.first] = r.second;
    }

    EventManager::registerListener(this, EventManager::DiscoveredUnit);
    EventManager::registerListener(this, EventManager::TileDiscovered);
    EventManager::registerListener(this, EventManager::TileHidden);
    EventManager::registerListener(this, EventManager::UnitMoved);

    EventManager::registerListener(this, EventManager::UnitGarrisoned);
    EventManager::registerListener(this, EventManager::UnitCreated);
    EventManager::registerListener(this, EventManager::UnitDestroyed);

    updateAvailableTechs();

    // Apply civ tech tree effect -- this disables units/techs and applies civ bonuses
    const genie::Civ &civData = DataManager::Inst().civilization(civId);
    if (civData.TechTreeID >= 0) {
        applyTechEffect(civData.TechTreeID);
    }
}
```

**Commit:** `feat: apply civ TechTreeID effect on player creation`

---

## Task 2: Implement DisableTech effect command

Currently `DisableTech` (effect type 102) logs but does nothing. It needs to actually remove the tech from the civilization's available techs and update the player's available research.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Civilization.h`

Add a new public method:

```cpp
    void disableTech(const uint16_t techId);
    void disableUnit(const uint16_t unitId);
```

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Civilization.cpp`

Add implementations:

```cpp
void Civilization::disableTech(const uint16_t techId)
{
    auto it = m_techs.find(techId);
    if (it == m_techs.end()) {
        return;
    }

    // Remove from research-available-at-building lists
    int16_t location = it->second.ResearchLocation;
    if (location > 0) {
        auto &vec = m_researchAvailable[location];
        vec.erase(
            std::remove_if(vec.begin(), vec.end(),
                [techId](const genie::Tech *t) {
                    // Tech pointers point into m_techs map, compare by research location match
                    // We need to find the one with matching ID -- techs don't store their own ID,
                    // but we can compare the pointer address
                    return true; // we'll use a different approach below
                }),
            vec.end()
        );
    }

    m_techs.erase(it);
}
```

Wait -- genie::Tech does not have an ID field (per the project memory). The tech's identity is its index in the `m_techs` map where the key IS the tech ID. So we need to remove the correct pointer. Let me revise:

```cpp
void Civilization::disableTech(const uint16_t techId)
{
    auto it = m_techs.find(techId);
    if (it == m_techs.end()) {
        return;
    }

    // Get pointer to the tech before erasing (so we can remove from research lists)
    const genie::Tech *techPtr = &it->second;
    int16_t location = it->second.ResearchLocation;

    // Remove from research-available-at-building lists
    if (location > 0) {
        auto locIt = m_researchAvailable.find(location);
        if (locIt != m_researchAvailable.end()) {
            auto &vec = locIt->second;
            vec.erase(
                std::remove(vec.begin(), vec.end(), techPtr),
                vec.end()
            );
        }
    }

    m_techs.erase(it);
}

void Civilization::disableUnit(const uint16_t unitId)
{
    if (unitId >= m_unitsData.size()) {
        return;
    }

    genie::Unit &unit = m_unitsData[unitId];
    unit.Enabled = false;

    // Remove from creatable units lists
    if (unit.Creatable.TrainLocationID > 0) {
        auto locIt = m_creatableUnits.find(unit.Creatable.TrainLocationID);
        if (locIt != m_creatableUnits.end()) {
            auto &vec = locIt->second;
            vec.erase(
                std::remove_if(vec.begin(), vec.end(),
                    [unitId](const genie::Unit *u) { return u->ID == unitId; }),
                vec.end()
            );
        }
    }
}
```

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

In `applyTechEffectCommand()`, change the `DisableTech` case from:

```cpp
    case genie::EffectCommand::DisableTech:
        DBG << "Disable tech" << effect.TargetUnit;
        break;
```

To:

```cpp
    case genie::EffectCommand::DisableTech:
        DBG << "Disable tech" << effect.TargetUnit;
        if (effect.TargetUnit >= 0) {
            civilization.disableTech(effect.TargetUnit);
        }
        break;
```

After `applyTechEffectCommand`, the player's `updateAvailableTechs()` should be called. This already happens in `applyResearch()`, but `applyTechEffect()` (called from the constructor for TechTreeID) does NOT call it. Fix:

In `Player::applyTechEffect()`, add `updateAvailableTechs()` at the end:

```cpp
void Player::applyTechEffect(const int effectId)
{
    if (effectId == -1) {
        return;
    }

    if (m_activeTechs.count(effectId)) {
        DBG << effectId << "already active";
        return;
    }

    m_activeTechs.insert(effectId);

    const genie::Effect &effect = DataManager::Inst().getEffect(effectId);

    for (const genie::EffectCommand &command : effect.EffectCommands) {
        applyTechEffectCommand(command);
    }

    updateAvailableTechs();
}
```

**Commit:** `feat: implement DisableTech effect command and unit disabling`

---

## Task 3: Apply scenario disabled lists

Scenarios can specify per-player disabled techs, units, and buildings. These are currently parsed but never applied.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/GameState.cpp`

In `setupScenario()`, after the player creation loop but before unit spawning (after the `m_players.push_back(player)` loop ends and before `m_unitManager->setPlayers`), add:

```cpp
    // Apply scenario-disabled techs/units/buildings
    const genie::ScnDisables &disables = scenario_->playerData.disables;
    for (size_t playerNum = 0; playerNum < m_players.size() && playerNum < 16; playerNum++) {
        Player::Ptr &player = m_players[playerNum];

        // Disabled techs
        for (size_t i = 0; i < disables.numDisabledTechs[playerNum] && i < disables.disabledTechs[playerNum].size(); i++) {
            uint32_t techId = disables.disabledTechs[playerNum][i];
            if (techId != 0xFFFFFFFF && techId > 0) {
                player->civilization.disableTech(techId);
            }
        }

        // Disabled units
        for (size_t i = 0; i < disables.numDisabledUnits[playerNum] && i < disables.disabledUnits[playerNum].size(); i++) {
            uint32_t unitId = disables.disabledUnits[playerNum][i];
            if (unitId != 0xFFFFFFFF && unitId > 0) {
                player->civilization.disableUnit(unitId);
            }
        }

        // Disabled buildings (buildings are units in genie)
        for (size_t i = 0; i < disables.numDisabledBuildings[playerNum] && i < disables.disabledBuildings[playerNum].size(); i++) {
            uint32_t buildingId = disables.disabledBuildings[playerNum][i];
            if (buildingId != 0xFFFFFFFF && buildingId > 0) {
                player->civilization.disableUnit(buildingId);
            }
        }
    }
```

Note: The scenario disables array is indexed by scenario player number. Gaia is player 0, human is typically 1. The `m_players` vector uses the same indexing (0=gaia, 1-N=players), but the ScnDisables arrays are 16-wide and indexed differently. For scenarios, `playerNum` in the m_players loop corresponds to scenario player indices. However, the disables array in scenarios uses indices 0-15 where 0=player 1, not gaia. Need to verify -- looking at the serialization, it does `serialize<uint32_t, 16>`, so it's 16 entries, one per possible player slot. We should use the player's slot index.

Actually, looking more carefully: the scenario disables are in `ScnMainPlayerData` which has a separate indexing. For safety, let's use the player index directly but skip gaia (index 0):

```cpp
    // Apply scenario-disabled techs/units/buildings
    const genie::ScnDisables &disables = scenario_->playerData.disables;
    for (size_t playerIdx = 1; playerIdx < m_players.size(); playerIdx++) {
        // Scenario disables use 0-based player indices (0 = player 1, not gaia)
        size_t disableIdx = playerIdx - 1;
        if (disableIdx >= 16) break;

        Player::Ptr &player = m_players[playerIdx];

        for (size_t i = 0; i < disables.numDisabledTechs[disableIdx] && i < disables.disabledTechs[disableIdx].size(); i++) {
            uint32_t techId = disables.disabledTechs[disableIdx][i];
            if (techId != 0xFFFFFFFF) {
                player->civilization.disableTech(static_cast<uint16_t>(techId));
            }
        }

        for (size_t i = 0; i < disables.numDisabledUnits[disableIdx] && i < disables.disabledUnits[disableIdx].size(); i++) {
            uint32_t unitId = disables.disabledUnits[disableIdx][i];
            if (unitId != 0xFFFFFFFF) {
                player->civilization.disableUnit(static_cast<uint16_t>(unitId));
            }
        }

        for (size_t i = 0; i < disables.numDisabledBuildings[disableIdx] && i < disables.disabledBuildings[disableIdx].size(); i++) {
            uint32_t bldId = disables.disabledBuildings[disableIdx][i];
            if (bldId != 0xFFFFFFFF) {
                player->civilization.disableUnit(static_cast<uint16_t>(bldId));
            }
        }
    }
```

Add the include for ScnPlayerData if not already present (it is via `genie/script/ScnFile.h` already included).

**Commit:** `feat: apply scenario per-player disabled techs/units/buildings`

---

## Task 4: Apply team bonuses

Each civ has a `TeamBonusID` effect that should be applied to all allied players (including the player itself).

**File:** `/home/dima/Projects/freeaoe/src/mechanics/GameState.cpp`

In `setupScenario()`, after all players are created and diplomacy is set, add team bonus application. The best place is right before `m_unitManager->setPlayers(m_players)`:

```cpp
    // Apply team bonuses to allied players
    for (const Player::Ptr &player : m_players) {
        if (player->playerId == 0) continue; // skip gaia

        const genie::Civ &civData = DataManager::Inst().civilization(
            player->civilization.id());
        if (civData.TeamBonusID < 0) continue;

        for (Player::Ptr &ally : m_players) {
            if (ally->playerId == 0) continue;
            if (!ally->isAllied(player->playerId)) continue;

            ally->applyTechEffect(civData.TeamBonusID);
        }
    }
```

Also add the same logic in `setupRandomMap()`, after the diplomacy setup loop and before the map generation:

```cpp
    // Apply team bonuses
    for (const auto &player : m_players) {
        if (player->playerId == 0) continue;

        const genie::Civ &civData = DataManager::Inst().civilization(
            player->civilization.id());
        if (civData.TeamBonusID < 0) continue;

        for (auto &ally : m_players) {
            if (ally->playerId == 0) continue;
            if (!ally->isAllied(player->playerId)) continue;

            ally->applyTechEffect(civData.TeamBonusID);
        }
    }
```

**Commit:** `feat: apply civ team bonuses to allied players`

---

## Task 5: Implement TechCostModifier and TechTimeModifier effects

These are currently logged but not implemented. They modify the cost and research time of technologies.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Civilization.h`

Add a method:

```cpp
    void modifyTechCost(const int16_t techId, const int16_t resourceType, float amount, bool isRelative);
    void modifyTechTime(const int16_t techId, float amount, bool isRelative);
```

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Civilization.cpp`

```cpp
void Civilization::modifyTechCost(const int16_t techId, const int16_t resourceType, float amount, bool isRelative)
{
    // If techId == -1, apply to all techs
    if (techId == -1) {
        for (auto &[id, tech] : m_techs) {
            for (auto &cost : tech.ResourceCosts) {
                if (cost.Type == resourceType) {
                    if (isRelative) {
                        cost.Amount += static_cast<int16_t>(amount);
                    } else {
                        cost.Amount = static_cast<int16_t>(amount);
                    }
                }
            }
        }
    } else {
        auto it = m_techs.find(techId);
        if (it == m_techs.end()) return;
        for (auto &cost : it->second.ResourceCosts) {
            if (cost.Type == resourceType) {
                if (isRelative) {
                    cost.Amount += static_cast<int16_t>(amount);
                } else {
                    cost.Amount = static_cast<int16_t>(amount);
                }
            }
        }
    }
}

void Civilization::modifyTechTime(const int16_t techId, float amount, bool isRelative)
{
    if (techId == -1) {
        for (auto &[id, tech] : m_techs) {
            if (isRelative) {
                tech.ResearchTime += static_cast<int16_t>(amount);
            } else {
                tech.ResearchTime = static_cast<int16_t>(amount);
            }
        }
    } else {
        auto it = m_techs.find(techId);
        if (it == m_techs.end()) return;
        if (isRelative) {
            it->second.ResearchTime += static_cast<int16_t>(amount);
        } else {
            it->second.ResearchTime = static_cast<int16_t>(amount);
        }
    }
}
```

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

Replace the `TechCostModifier` and `TechTimeModifier` cases:

```cpp
    case genie::EffectCommand::TechCostModifier:
        // TargetUnit = tech ID (-1 for all), UnitClassID = resource type, Amount = new/delta cost
        // Type 101 is absolute, but the mode flag is in the effect command structure
        civilization.modifyTechCost(effect.TargetUnit, effect.UnitClassID, effect.Amount, false);
        break;
    case genie::EffectCommand::TechTimeModifier:
        // TargetUnit = tech ID (-1 for all), Amount = new/delta time
        civilization.modifyTechTime(effect.TargetUnit, effect.Amount, false);
        break;
```

**Commit:** `feat: implement TechCostModifier and TechTimeModifier effects`

---

## Task 6: Protect Gaia from civ tech tree effects

Gaia (player 0) is created with civ ID 0, which may also have a TechTreeID. We should skip applying TechTreeID for Gaia since it should have access to all units.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

In the constructor, guard the TechTreeID application:

```cpp
    // Apply civ tech tree effect -- this disables units/techs and applies civ bonuses
    // Skip for Gaia (player 0) which needs access to all unit types
    if (id != 0) {
        const genie::Civ &civData = DataManager::Inst().civilization(civId);
        if (civData.TechTreeID >= 0) {
            applyTechEffect(civData.TechTreeID);
        }
    }
```

**Commit:** `fix: skip TechTreeID application for Gaia player`

---

## Task 7: Update tech tree screen to show civ-specific data

The TechTreeScreen should reflect which techs are available vs disabled for the current player's civ.

**File:** `/home/dima/Projects/freeaoe/src/ui/TechTreeScreen.cpp`

In `buildTechList()`, the tech list should come from the human player's `civilization.availableTechs()` rather than from the global DataManager tech list. Verify the current implementation and update if it uses the global list.

This is a read-and-fix task. Read the file, check if it uses `DataManager::Inst().allTechs()` and if so, change it to use `m_state->humanPlayer()->civilization.availableTechs()`.

**Commit:** `fix: tech tree screen shows civ-specific available techs`

---

## Task 8: Log civ info for debugging

Add logging at player creation to verify civ bonuses are being applied correctly.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

After the TechTreeID application in the constructor, add:

```cpp
    DBG << "Player" << id << "civ" << civilization.name()
        << "TechTreeID=" << civData.TechTreeID
        << "TeamBonusID=" << civData.TeamBonusID
        << "available techs=" << civilization.availableTechs().size();
```

For Android builds, also add:

```cpp
#ifdef ANDROID
    {
        const genie::Civ &civData = DataManager::Inst().civilization(civId);
        ALOG("Player %d: civ=%s TechTreeID=%d TeamBonusID=%d techs=%zu",
             id, civilization.name().c_str(), civData.TechTreeID, civData.TeamBonusID,
             civilization.availableTechs().size());
    }
#endif
```

**Commit:** `feat: log civ tech tree info at player creation for debugging`

---

## Task 9: Build and test

**Command:**
```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Verify:
1. Desktop build compiles without errors
2. Start a random map game -- different civs should now have different available techs/units
3. Load a campaign scenario -- civ restrictions should be enforced

If the build fails, fix compiler errors (likely missing includes or type mismatches).

**Commit:** (fix any build issues)

---

## Testing Checklist

- [ ] Build succeeds on desktop (`cd build && make -j$(nproc)`)
- [ ] Random map: Britons player should NOT see Frank-specific unique techs
- [ ] Random map: Civ-disabled units should not appear in training lists
- [ ] Scenario: Per-player disabled techs from scenario file are applied
- [ ] Team bonuses: Allied players receive each other's team bonuses
- [ ] Gaia: Still has access to all unit types (trees, animals, etc.)
- [ ] Tech tree screen: Shows civ-filtered techs, not all techs
- [ ] Age advancement still works (TechTreeID effects don't break age-up)

---

## Key Files Modified

| File | Changes |
|------|---------|
| `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp` | Apply TechTreeID in constructor, implement DisableTech, TechCostModifier, TechTimeModifier |
| `/home/dima/Projects/freeaoe/src/mechanics/Civilization.h` | Add `disableTech()`, `disableUnit()`, `modifyTechCost()`, `modifyTechTime()` |
| `/home/dima/Projects/freeaoe/src/mechanics/Civilization.cpp` | Implement new methods |
| `/home/dima/Projects/freeaoe/src/mechanics/GameState.cpp` | Apply scenario disables, team bonuses |
| `/home/dima/Projects/freeaoe/src/ui/TechTreeScreen.cpp` | Use civ-specific tech list |

## Architecture Notes

- **TechTreeID is an effect ID**, not a tech ID. It points into the Effects table (`DataManager::getEffect()`). When applied, it runs a series of `EffectCommand`s that disable techs (type 102), enable/disable units, and modify attributes -- automatically differentiating civs.
- **All civ bonuses are already in the dat file** as effect commands. We do not need to hard-code any civ-specific logic. The engine just needs to apply the effects.
- **genie::Tech has no ID field** -- its identity is its index in the vector/map. The `m_techs` map in `Civilization` uses the tech index as the key.
- **DisableTech (type 102)** uses `effect.TargetUnit` to specify the tech ID to disable (confusing naming -- it's called TargetUnit but holds a tech ID for this effect type).
- **The dat file's per-civ unit data already differs** between civs (e.g., different unit stats). The `Civilization::applyData()` method already copies per-civ unit data. What's missing is the TechTreeID effect that disables units/techs that shouldn't be available.
