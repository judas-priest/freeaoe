# Module 11: Conquest Victory Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Detect when all enemy players are eliminated and trigger a victory/defeat screen.

**Architecture:** Add a `checkConquestVictory()` method to ScenarioController that checks if any player has lost all units. Called every update tick alongside existing checkWonderVictory/checkRelicVictory. Uses existing `onPlayerWin()` from GameState and `Player::alive` field.

**Tech Stack:** C++, existing GameState/ScenarioController/Player APIs

**Verified APIs:**
- `GameState::players()` returns `const vector<shared_ptr<Player>>&`
- `GameState::player(size_t id)` returns `shared_ptr<Player>`
- `GameState::humanPlayer()` returns `shared_ptr<Player>` (no `humanPlayerId()`)
- `GameState::onPlayerWin(int playerId)` exists
- `GameState::players()` returns `const vector<shared_ptr<Player>>&`
- `GameState::unitManager()` returns `shared_ptr<UnitManager>`
- `Player::alive` is a public `bool` field (not `isAlive`)
- `Player::playerId` is a public `const int` field (not a method)
- `Player::diplomaticStanceTo(uint8_t playerId)` returns `DiplomaticStance` (not `diplomaticStanceFor`)
- `Player::DiplomaticStance::Allied` (not `Ally`)
- `Unit::isDead()`, `Unit::isDying()` exist
- `Unit::playerId()` — verify if it's a method on Unit or field
- `ScenarioController` has raw pointer `m_gameState`

---

### Task 1: Add conquest victory check to ScenarioController

**Files:**
- Modify: `src/mechanics/ScenarioController.h` (add method declaration + member)
- Modify: `src/mechanics/ScenarioController.cpp` (add method + call from update)

- [ ] **Step 1: Add method declaration and member to ScenarioController.h**

In `ScenarioController.h`, add after `checkRegicide` declaration:

```cpp
void checkConquestVictory(Time time);
```

Add in the private section, after `m_pendingAISignals`:

```cpp
std::set<int> m_defeatedPlayers;
```

Add `#include <set>` at the top if not already present.

- [ ] **Step 2: Implement checkConquestVictory in ScenarioController.cpp**

```cpp
void ScenarioController::checkConquestVictory(Time time)
{
    const auto &allPlayers = m_gameState->players();

    // Check each non-Gaia, non-defeated player
    for (size_t i = 1; i < allPlayers.size(); i++) {
        if (m_defeatedPlayers.count(i)) {
            continue;
        }

        const auto &player = allPlayers[i];
        if (!player || !player->alive) {
            continue;
        }

        // Check if player has any living units
        bool hasAnything = false;
        for (const Unit::Ptr &unit : m_gameState->unitManager()->units()) {
            if (!unit || unit->playerId() != static_cast<int>(i)) {
                continue;
            }
            if (unit->isDead() || unit->isDying()) {
                continue;
            }
            hasAnything = true;
            break;
        }

        if (!hasAnything) {
            m_defeatedPlayers.insert(i);
            player->alive = false;
        }
    }

    // Check if all enemies of human player are defeated
    auto human = m_gameState->humanPlayer();
    if (!human || !human->alive) {
        return;
    }

    int humanId = human->playerId;

    bool allEnemiesDefeated = true;
    for (size_t i = 1; i < allPlayers.size(); i++) {
        if (static_cast<int>(i) == humanId) {
            continue;
        }
        const auto &other = allPlayers[i];
        if (!other) {
            continue;
        }
        // Allies don't count as enemies
        if (human->diplomaticStanceTo(i) == Player::DiplomaticStance::Allied) {
            continue;
        }
        if (other->alive) {
            allEnemiesDefeated = false;
            break;
        }
    }

    if (allEnemiesDefeated) {
        m_gameState->onPlayerWin(humanId);
    }
}
```

- [ ] **Step 3: Call checkConquestVictory from update()**

In `ScenarioController::update()`, after `checkRegicide(time);`, add:

```cpp
checkConquestVictory(time);
```

- [ ] **Step 4: Build and test**

```bash
cd build && make -j$(nproc)
```

Expected: compiles without errors.

- [ ] **Step 5: Commit**

```bash
git add src/mechanics/ScenarioController.h src/mechanics/ScenarioController.cpp
git commit -m "feat: conquest victory — detect when all enemy players eliminated"
```
