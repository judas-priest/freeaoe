# Plan: AI Difficulty Levels

**Date:** 2026-07-14
**Branch:** android-port
**Goal:** Wire `ai::DifficultyLevel` into BasicAI so the AI actually plays differently at Easiest/Easy/Moderate/Hard/Hardest. Currently all thresholds are hardcoded constants that ignore the difficulty field that already exists on `AiPlayer`.

---

## Current State

- `AiPlayer::difficultyLevel` exists and defaults to `ai::DifficultyLevel::Moderate` (AiPlayer.h:17).
- The enum `ai::DifficultyLevel { Easiest, Easy, Moderate, Hard, Hardest }` is in `src/ai/gen/enums.h:60`.
- `BasicAI` has four hardcoded thresholds in BasicAI.cpp:
  - `villagerCap = 20` (line 93)
  - `militaryCap = 30` (line 419)
  - `attackThreshold = 8` (line 343)
  - `updateInterval = 5000 ms` (line 50)
- No difficulty is ever set on any `AiPlayer` — both `setupScenario()` and `setupRandomMap()` in GameState.cpp create AiPlayers without assigning `difficultyLevel`.
- `AiPlayer::addResource()` applies an escrow mechanic but no gathering bonus.

---

## Difficulty Parameters Table

| Level    | villagerCap | militaryCap | attackThreshold | updateIntervalMs | gatherBonus | researchTechs | buildSiege |
|----------|-------------|-------------|-----------------|------------------|-------------|---------------|------------|
| Easiest  | 10          | 8           | 15              | 8000             | 1.0 (double)| false         | false      |
| Easy     | 15          | 15          | 12              | 6000             | 0.33 (33% extra) | false    | false      |
| Moderate | 20          | 30          | 8               | 5000             | 0.0         | true          | true       |
| Hard     | 35          | 50          | 6               | 4000             | 0.0         | true          | true       |
| Hardest  | 75          | 80          | 4               | 3000             | 0.0         | true          | true       |

`gatherBonus` is an extra fraction of each resource gain credited for free (simulating handicap cheats AoE2 uses at lower difficulty).

---

## Tasks

### Task 1 — Add `DifficultyParams` struct to BasicAI.h

**File:** `src/ai/BasicAI.h`

Add a public struct and static lookup method, plus store params as a member. Replace the current header content:

```cpp
#pragma once

#include "core/Types.h"
#include "ai/gen/enums.h"
#include <memory>

struct AiPlayer;
class UnitManager;

// Simple hardcoded AI that trains villagers, builds houses, and makes military
class BasicAI
{
public:
    struct DifficultyParams {
        int   villagerCap;       // max villagers to train
        int   militaryCap;       // max military before stopping production
        int   attackThreshold;   // idle military needed before attacking
        int   updateIntervalMs;  // ms between AI ticks
        float gatherBonus;       // free resource fraction added on each gather (0 = off)
        bool  researchTechs;     // whether to call researchTechs()
        bool  buildSiege;        // whether to build siege workshop + rams
    };

    static DifficultyParams paramsForDifficulty(ai::DifficultyLevel level);

    BasicAI(AiPlayer *player, UnitManager *unitManager);

    void update(Time time);

private:
    AiPlayer *m_player;
    UnitManager *m_unitManager;
    Time m_lastUpdate = 0;
    DifficultyParams m_params;

    void scoutMap();
    void trainVillagers();
    void buildHouses();
    void assignIdleVillagers();
    void buildDropOffSites();
    void researchLoom();
    void advanceAge();
    void attackWithArmy();
    void trainMilitary();
    void researchTechs();
    void buildStructure(int buildingId, int woodCost);
    void trainFromBuilding(int buildingId, int unitId);
    bool isMilitaryUnit(int unitId) const;

    int countUnitsOfType(int unitId) const;
    int countBuildingsOfType(int buildingId) const;
};
```

---

### Task 2 — Implement lookup table and replace hardcoded values in BasicAI.cpp

**File:** `src/ai/BasicAI.cpp`

**2a. Add the static lookup after the includes, before the constructor:**

```cpp
BasicAI::DifficultyParams BasicAI::paramsForDifficulty(ai::DifficultyLevel level)
{
    switch (level) {
    case ai::DifficultyLevel::Easiest:
        return { 10, 8,  15, 8000, 1.0f,  false, false };
    case ai::DifficultyLevel::Easy:
        return { 15, 15, 12, 6000, 0.33f, false, false };
    default:
    case ai::DifficultyLevel::Moderate:
        return { 20, 30,  8, 5000, 0.0f,  true,  true  };
    case ai::DifficultyLevel::Hard:
        return { 35, 50,  6, 4000, 0.0f,  true,  true  };
    case ai::DifficultyLevel::Hardest:
        return { 75, 80,  4, 3000, 0.0f,  true,  true  };
    }
}
```

**2b. Update constructor to initialise `m_params`:**

```cpp
BasicAI::BasicAI(AiPlayer *player, UnitManager *unitManager)
    : m_player(player), m_unitManager(unitManager),
      m_params(paramsForDifficulty(player->difficultyLevel))
{
}
```

**2c. Replace hardcoded update interval (line 50):**

```cpp
// Before:
if (time - m_lastUpdate < 5000) return; // Every 5 seconds

// After:
if (time - m_lastUpdate < static_cast<Time>(m_params.updateIntervalMs)) return;
```

**2d. Replace hardcoded villagerCap (line 93 in `trainVillagers`):**

```cpp
// Before:
if (villagerCount >= 20) return;

// After:
if (villagerCount >= m_params.villagerCap) return;
```

**2e. Replace hardcoded attackThreshold (line 343 in `attackWithArmy`):**

```cpp
// Before:
if (idleMilitary < 8) return;

// After:
if (idleMilitary < m_params.attackThreshold) return;
```

**2f. Replace hardcoded militaryCap (line 419 in `trainMilitary`):**

```cpp
// Before:
if (totalMilitary >= 30) return;

// After:
if (totalMilitary >= m_params.militaryCap) return;
```

**2g. Guard siege workshop construction with `buildSiege` flag (in `trainMilitary`).**

The two siege-related blocks currently run unconditionally. Wrap them:

```cpp
// Build siege workshop (49, 200W) in Castle Age
if (m_params.buildSiege && countBuildingsOfType(49) == 0 && m_player->currentAge() >= Player::CastleAge) {
    if (wood >= 200) buildStructure(49, 200);
}
```

And the ram training block:

```cpp
// Train from siege workshop: battering ram (35, 160W 75G)
if (m_params.buildSiege && countBuildingsOfType(49) > 0 && wood >= 160 && gold >= 75) {
    if (countUnitsOfType(35) < 3) {
        trainFromBuilding(49, 35);
    }
}
```

**2h. Guard `researchTechs()` call in `update()` with `researchTechs` flag:**

```cpp
// Before:
researchTechs();

// After:
if (m_params.researchTechs) researchTechs();
```

---

### Task 3 — Apply `gatherBonus` in AiPlayer::addResource()

**File:** `src/ai/AiPlayer.cpp`

The bonus simulates AoE2's lower-difficulty resource handicap: at Easiest the AI gets double resources (gatherBonus = 1.0 means +100%), at Easy +33%, at Moderate+ nothing extra.

Only apply it to the four economy resource types to avoid corrupting population/age counters.

```cpp
#include "AiPlayer.h"
#include "BasicAI.h"
#include "resource/DataManager.h"

void AiPlayer::addResource(const genie::ResourceType type, float amount)
{
    // Apply difficulty gather bonus for economy resources only
    if (m_params.gatherBonus > 0.f) {
        switch (type) {
        case genie::ResourceType::FoodStorage:
        case genie::ResourceType::WoodStorage:
        case genie::ResourceType::GoldStorage:
        case genie::ResourceType::StoneStorage:
            amount *= (1.f + m_params.gatherBonus);
            break;
        default:
            break;
        }
    }

    float toEscrow = amount * m_escrowPercentages[type] / 100.;
    m_reserves[type] += toEscrow;

    Player::addResource(type, amount - toEscrow);

    EventManager::registerListener(this, EventManager::ChatMessage);
}
```

To access `m_params` in `AiPlayer`, the difficulty params need to be stored there too (see Task 4).

---

### Task 4 — Store DifficultyParams on AiPlayer

**File:** `src/ai/AiPlayer.h`

`AiPlayer::addResource()` needs the params. Store a copy on the struct so it is available before `m_basicAI` is constructed.

Add `#include "BasicAI.h"` and a member:

```cpp
#pragma once

#include "mechanics/Player.h"
#include "gen/enums.h"
#include "BasicAI.h"

#include <memory>

namespace ai { struct AiScript; }

struct AiPlayer : public Player
{
    AiPlayer(const int id, const int civId, const std::shared_ptr<Map> &map,
             const ResourceMap &startingResources = {})
        : Player(id, civId, map, startingResources),
          m_params(BasicAI::paramsForDifficulty(difficultyLevel))
    {}

    ai::DifficultyLevel difficultyLevel = ai::DifficultyLevel::Moderate;
    BasicAI::DifficultyParams m_params;

    std::shared_ptr<ai::AiScript> m_aiScript;
    std::shared_ptr<BasicAI> m_basicAI;

    // Held in escrow
    ResourceMap m_reserves;

    // How much to hold in escrow
    ResourceMap m_escrowPercentages;

    void addResource(const genie::ResourceType type, float amount) override;

    float resourcesAvailableWithEscrow(const genie::ResourceType type) const {
        float ret = Player::resourcesAvailable(type);

        ResourceMap::const_iterator it = m_reserves.find(type);
        if (it != m_reserves.end()) {
            ret += it->second;
        }

        return ret;
    }

    // meh, duplicating code ish
    bool canAffordUnitWithEscrow(const int unitId) const;
    bool canAffordResearchWithEscrow(const int researchId) const;

    void onChatMessage(const int sourcePlayer, const int targetPlayer, const std::string &message) override;
};
```

Note: `difficultyLevel` is declared before `m_params`, so the member-initialiser `paramsForDifficulty(difficultyLevel)` uses the default `Moderate`. Callers that want a different difficulty must set `difficultyLevel` before `m_basicAI` is created (or call `m_basicAI = std::make_shared<BasicAI>(...)` after setting it — which they already do).

To handle the case where callers set difficulty after construction, add a setter:

```cpp
void setDifficulty(ai::DifficultyLevel level) {
    difficultyLevel = level;
    m_params = BasicAI::paramsForDifficulty(level);
    if (m_basicAI) m_basicAI->applyParams(m_params);
}
```

And in BasicAI add:

```cpp
void applyParams(const DifficultyParams &p) { m_params = p; }
```

(Add `applyParams` declaration to BasicAI.h under `update`.)

---

### Task 5 — Wire difficulty in GameState.cpp

**File:** `src/mechanics/GameState.cpp`

**5a. Add `m_difficulty` member to GameState.**

**File:** `src/mechanics/GameState.h`

In the private section, after `m_gameType`:

```cpp
ai::DifficultyLevel m_difficulty = ai::DifficultyLevel::Moderate;
```

Add include at top of GameState.h:

```cpp
#include "ai/gen/enums.h"
```

Add public setter/getter:

```cpp
void setDifficulty(ai::DifficultyLevel d) { m_difficulty = d; }
ai::DifficultyLevel difficulty() const { return m_difficulty; }
```

**5b. Apply difficulty in `setupScenario()` when creating AiPlayers.**

In GameState.cpp, after the `aiPlayer->m_basicAI = ...` line (around line 314):

```cpp
auto aiPlayer = std::make_shared<AiPlayer>(playerNum, ..., map_);
aiPlayer->setDifficulty(m_difficulty);   // <-- add this
aiPlayer->m_aiScript = std::make_shared<ai::AiScript>(aiPlayer.get());
aiPlayer->m_basicAI = std::make_shared<BasicAI>(aiPlayer.get(), m_unitManager.get());
```

**5c. Apply difficulty in `setupRandomMap()` for each AI player.**

In GameState.cpp, after `aiPlayer->playerColor = i - 1;` (around line 467):

```cpp
aiPlayer->setDifficulty(m_difficulty);   // <-- add this
aiPlayer->m_aiScript = std::make_shared<ai::AiScript>(aiPlayer.get());
aiPlayer->m_basicAI = std::make_shared<BasicAI>(aiPlayer.get(), m_unitManager.get());
```

---

## Build & Test

```bash
cd /home/dima/Projects/freeaoe/android
rm -rf app/.cxx app/build
unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && adb install -r app/build/outputs/apk/debug/app-debug.apk
adb logcat -s "FreeAoE"
```

Desktop smoke-test (faster iteration):

```bash
cd /home/dima/Projects/freeaoe/build
make -j$(nproc)
```

**Validation checklist:**
- Compile with no errors (the main risk is the circular include between AiPlayer.h and BasicAI.h — use a forward declaration in BasicAI.h and move `DifficultyParams` to its own header if needed).
- At Easiest: AI trains at most 10 villagers, attacks only when 15+ idle military are present, ticks every 8 s.
- At Hardest: AI trains up to 75 villagers, attacks with 4+ units, ticks every 3 s.
- `gatherBonus` at Easiest: verify AI accumulates resources roughly 2x faster than at Moderate by checking logcat resource counts.
- `researchTechs = false` at Easiest/Easy: confirm no blacksmith/wheelbarrow research is queued.
- `buildSiege = false` at Easiest/Easy: confirm no siege workshop or battering ram appears.

---

## Circular Include Note

`AiPlayer.h` including `BasicAI.h` while `BasicAI.h` already includes `AiPlayer` via `AiPlayer.h` would be circular. The cleanest fix is to extract `DifficultyParams` into its own tiny header:

**New file:** `src/ai/DifficultyParams.h`

```cpp
#pragma once
#include "ai/gen/enums.h"

namespace ai {

struct DifficultyParams {
    int   villagerCap;
    int   militaryCap;
    int   attackThreshold;
    int   updateIntervalMs;
    float gatherBonus;
    bool  researchTechs;
    bool  buildSiege;

    static DifficultyParams forLevel(DifficultyLevel level);
};

} // namespace ai
```

Implement `forLevel` in a new `src/ai/DifficultyParams.cpp`:

```cpp
#include "DifficultyParams.h"

namespace ai {

DifficultyParams DifficultyParams::forLevel(DifficultyLevel level)
{
    switch (level) {
    case DifficultyLevel::Easiest:  return { 10, 8,  15, 8000, 1.0f,  false, false };
    case DifficultyLevel::Easy:     return { 15, 15, 12, 6000, 0.33f, false, false };
    default:
    case DifficultyLevel::Moderate: return { 20, 30,  8, 5000, 0.0f,  true,  true  };
    case DifficultyLevel::Hard:     return { 35, 50,  6, 4000, 0.0f,  true,  true  };
    case DifficultyLevel::Hardest:  return { 75, 80,  4, 3000, 0.0f,  true,  true  };
    }
}

} // namespace ai
```

Then:
- `BasicAI.h` includes `DifficultyParams.h` and uses `ai::DifficultyParams` directly.
- `AiPlayer.h` includes `DifficultyParams.h` (no dependency on `BasicAI.h`).
- `BasicAI.cpp` calls `ai::DifficultyParams::forLevel(player->difficultyLevel)` in its constructor.

Add `DifficultyParams.cpp` to `src/ai/CMakeLists.txt` (or wherever the ai sources are listed).

---

## Files Changed Summary

| File | Change |
|------|--------|
| `src/ai/DifficultyParams.h` | NEW — params struct + static lookup |
| `src/ai/DifficultyParams.cpp` | NEW — lookup table implementation |
| `src/ai/BasicAI.h` | Include DifficultyParams.h, add `m_params` member, `applyParams()` |
| `src/ai/BasicAI.cpp` | Constructor initialises m_params; all 4 thresholds use m_params; researchTechs/buildSiege guarded |
| `src/ai/AiPlayer.h` | Include DifficultyParams.h, add `m_params` member, `setDifficulty()` |
| `src/ai/AiPlayer.cpp` | addResource applies gatherBonus multiplier before escrow |
| `src/mechanics/GameState.h` | Add `m_difficulty`, `setDifficulty()`, `difficulty()` |
| `src/mechanics/GameState.cpp` | Call `aiPlayer->setDifficulty(m_difficulty)` in setupScenario + setupRandomMap |
| `src/ai/CMakeLists.txt` (or equivalent) | Add DifficultyParams.cpp |
