# Module 23: Game Mode Mechanics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement gameplay mechanics for King of the Hill, Deathmatch, and Sudden Death game modes; add victory/defeat notification UI.

**Architecture:** Game modes are already enumerated in `GameState.h:51-60`. Each mode needs specific rules applied at game start (Deathmatch: bonus resources) and/or during gameplay (KotH: monument control timer). Victory/defeat uses existing `ScenarioController` check infrastructure with a new UI overlay.

**Tech Stack:** C++20, existing GameState/ScenarioController, SDL2 rendering

---

## Background

- `GameType` enum: `Default, HighResource, MediumResource, KingOfTheHill, Deathmatch, SuddenDeath, Regicide, WonderRace` (`GameState.h:51-60`)
- Victory checks already exist: `checkConquestVictory()`, `checkRelicVictory()`, `checkWonderVictory()`, `checkRegicide()` in `ScenarioController`
- `RELIC_VICTORY_TIME = 300000ms`, `WONDER_COUNTDOWN = 300000ms` already defined
- No victory/defeat UI notification exists — player has no way to know they won/lost

## Key Files

- `src/mechanics/GameState.h:51-60` — GameType enum
- `src/mechanics/ScenarioController.h` — Victory checks, timers
- `src/mechanics/ScenarioController.cpp` — Victory check implementations
- `src/mechanics/Player.h` — Player resources
- `src/ui/ActionPanel.cpp` — Could host victory overlay

---

### Task 1: Deathmatch — starting resources

**Files:**
- Modify: `src/mechanics/GameState.cpp`

- [ ] **Step 1: Apply Deathmatch resources at game start**

In `GameState.cpp`, find where game type is set and players are initialized. After normal resource setup, if game type is Deathmatch, give each player bonus resources:

```cpp
if (m_gameType == GameType::Deathmatch) {
    for (auto &player : m_players) {
        if (!player) continue;
        player->addResource(genie::ResourceType::Food, 20000);
        player->addResource(genie::ResourceType::Wood, 20000);
        player->addResource(genie::ResourceType::Gold, 10000);
        player->addResource(genie::ResourceType::Stone, 5000);
    }
}
```

AoE2 Deathmatch values: 20000 food, 20000 wood, 10000 gold, 5000 stone.

- [ ] **Step 2: Build and test**

```bash
cd build && make -j$(nproc)
```

Start a Deathmatch game, verify resources start at elevated values.

- [ ] **Step 3: Commit**

```bash
git add src/mechanics/GameState.cpp
git commit -m "feat: Deathmatch game mode gives 20k/20k/10k/5k starting resources"
```

---

### Task 2: Sudden Death — no respawn TC

**Files:**
- Modify: `src/mechanics/ScenarioController.cpp`

- [ ] **Step 1: Add Sudden Death check**

In `ScenarioController.cpp`, in the main update loop where victory conditions are checked, add Sudden Death logic. A player loses if their last Town Center is destroyed:

```cpp
if (m_gameState->gameType() == GameType::SuddenDeath) {
    for (const auto &player : m_gameState->players()) {
        if (!player || player->isDefeated) continue;

        bool hasTownCenter = false;
        for (const auto &unit : player->units()) {
            if (unit->data()->ID == Unit::TownCenter ||
                unit->data()->ID == Unit::TownCenterFoundation) {
                hasTownCenter = true;
                break;
            }
        }

        if (!hasTownCenter) {
            defeatPlayer(player);
        }
    }
}
```

- [ ] **Step 2: Verify Town Center unit IDs**

Check that `Unit::TownCenter` (109) is defined in `Unit.h`. If `TownCenterFoundation` doesn't exist, just check for ID 109.

- [ ] **Step 3: Build and test**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/ScenarioController.cpp
git commit -m "feat: Sudden Death mode — lose when last Town Center destroyed"
```

---

### Task 3: King of the Hill — monument control

**Files:**
- Modify: `src/mechanics/ScenarioController.h`
- Modify: `src/mechanics/ScenarioController.cpp`
- Modify: `src/mechanics/RandomMapGenerator.cpp`

- [ ] **Step 1: Add KotH state to ScenarioController**

In `ScenarioController.h`, add:

```cpp
// King of the Hill
// AoE2: 550 in-game years. At 1.5x speed ~9 real minutes. Use ~540000ms.
static constexpr int KOTH_VICTORY_TIME = 540000; // ~9 minutes in ms
static constexpr int KOTH_MIN_TIMER = 60000; // Reset to 1 min minimum when captured by non-ally
static constexpr int KOTH_RESOURCE_TRICKLE_INTERVAL = 60000; // 50 of each resource per minute
int m_kothControlPlayer = -1;
int m_kothControlTime = 0;
int m_kothTrickleAccum = 0;
MapPos m_monumentPosition;
bool m_hasMonument = false;
```

- [ ] **Step 2: Place monument in map center**

In `RandomMapGenerator.cpp`, when game type is KotH, place a monument (genie unit ID 826 = Monument) at map center:

```cpp
if (m_gameType == GameType::KingOfTheHill) {
    const int centerX = m_mapSize / 2;
    const int centerY = m_mapSize / 2;
    placeUnit(826, centerX, centerY, gaiaPlayer); // Monument, owned by Gaia
}
```

- [ ] **Step 3: Implement KotH control check**

In `ScenarioController.cpp`, add to the update loop:

```cpp
if (m_gameState->gameType() == GameType::KingOfTheHill && m_hasMonument) {
    // Find who controls the monument area (units within 5 tiles of monument)
    int controllingPlayer = -1;
    bool contested = false;

    for (const auto &player : m_gameState->players()) {
        if (!player || player->isDefeated) continue;
        for (const auto &unit : player->units()) {
            if (unit->position().distance(m_monumentPosition) < 5 * Constants::TILE_SIZE) {
                if (controllingPlayer == -1) {
                    controllingPlayer = player->playerId;
                } else if (controllingPlayer != player->playerId) {
                    contested = true;
                    break;
                }
            }
        }
        if (contested) break;
    }

    if (!contested && controllingPlayer != -1) {
        if (controllingPlayer == m_kothControlPlayer) {
            m_kothControlTime += timeDelta;

            // Resource trickle: 50 food/wood/gold/stone per minute to controller
            m_kothTrickleAccum += timeDelta;
            if (m_kothTrickleAccum >= KOTH_RESOURCE_TRICKLE_INTERVAL) {
                m_kothTrickleAccum -= KOTH_RESOURCE_TRICKLE_INTERVAL;
                auto ctrlPlayer = m_gameState->player(controllingPlayer);
                if (ctrlPlayer) {
                    ctrlPlayer->addResource(genie::ResourceType::FoodStorage, 50);
                    ctrlPlayer->addResource(genie::ResourceType::WoodStorage, 50);
                    ctrlPlayer->addResource(genie::ResourceType::GoldStorage, 50);
                    ctrlPlayer->addResource(genie::ResourceType::StoneStorage, 50);
                }
            }

            if (m_kothControlTime >= KOTH_VICTORY_TIME) {
                declareWinner(controllingPlayer);
            }
        } else {
            // New controller — timer continues from previous value but
            // resets to minimum 1 min if below that (AoE2 behavior)
            if (m_kothControlTime < KOTH_MIN_TIMER) {
                m_kothControlTime = KOTH_MIN_TIMER;
            }
            m_kothControlPlayer = controllingPlayer;
            m_kothTrickleAccum = 0;
        }
    }
    // If contested or nobody near, timer pauses (does NOT reset)
}
```

- [ ] **Step 4: Build and test**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 5: Commit**

```bash
git add src/mechanics/ScenarioController.h src/mechanics/ScenarioController.cpp src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: King of the Hill mode with monument control timer"
```

---

### Task 4: Victory/Defeat notification UI

**Files:**
- Modify: `src/Engine.cpp`
- Modify: `src/mechanics/ScenarioController.cpp`

Note: `GameState` already has a `Result` enum with `Won/Lost/Running` at `GameState.h:77-81` and a `result` member. No need to add new state.

- [ ] **Step 1: Set result when victory/defeat detected**

In ScenarioController, where `onPlayerDefeated()` fires or victory is declared, set the result:

```cpp
// In onPlayerDefeated or equivalent:
if (defeatedPlayer == m_gameState->humanPlayer().get()) {
    m_gameState->result = GameState::Result::Lost;
}

// When checking if only one non-defeated player remains:
// (inside checkConquestVictory or equivalent)
if (lastSurvivor == m_gameState->humanPlayer()) {
    m_gameState->result = GameState::Result::Won;
}
```

- [ ] **Step 2: Render victory/defeat overlay**

In Engine.cpp render loop, after normal rendering, check game result:

```cpp
if (m_gameState->result != GameState::Result::Running) {
    const char *text = (m_gameState->result == GameState::Result::Won)
        ? "VICTORY" : "DEFEAT";
    // Draw centered text overlay with semi-transparent background
    renderTextOverlay(text);
}
```

Use existing text rendering system (`LanguageManager::getString()` for localized strings if available, or hardcoded English).

- [ ] **Step 4: Build and test**

```bash
cd build && make -j$(nproc)
```

Defeat all enemies or get defeated — verify overlay appears.

- [ ] **Step 5: Commit**

```bash
git add src/mechanics/GameState.h src/mechanics/GameState.cpp src/mechanics/ScenarioController.cpp src/Engine.cpp
git commit -m "feat: victory/defeat notification overlay"
```
