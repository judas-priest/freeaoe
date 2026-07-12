# freeaoe — Full Mobile Game Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform freeaoe Android port from a tech demo into a fully playable mobile AoE2 experience — fix every TODO, implement every stub, polish every UI element.

**Architecture:** Work through 5 phases: (1) Critical fixes that block gameplay, (2) UI/HUD polish for mobile, (3) Missing gameplay mechanics, (4) Trigger system completion, (5) AI system. Each task is independent and produces a working build.

**Tech Stack:** C++20, SDL2, SDL_ttf, Android NDK, genieutils, miniaudio

**Build:** `cd android && rm -rf app/.cxx app/build && unset LD_PRELOAD && ./gradlew assembleDebug`
**Install:** `unset LD_PRELOAD && adb install -r android/app/build/outputs/apk/debug/app-debug.apk`
**Logs:** `adb logcat -s "FreeAoE"`
**Force restart:** `adb shell am force-stop org.freeaoe && adb shell am start -n org.freeaoe/.FreeAoEActivity`

**IMPORTANT:** Gradle NDK caching is broken — always do `rm -rf app/.cxx app/build` before building to ensure changes compile. Incremental builds silently skip recompilation.

---

## Phase 1: Critical Gameplay Fixes

### Task 1: Fix camera start position for campaign scenarios

**Problem:** Camera starts at wrong position, showing unexplored black area instead of player's units.

**Root cause:** `GameState::setupScenario()` at `GameState.cpp:338-344` reads `player1CameraX/Y` from scenario data. These coordinates may be wrong (negative, zero, or pointing to wrong location). AoE2 scenarios have flipped X/Y in many places.

**Files:**
- Modify: `src/mechanics/GameState.cpp:338-344`

- [ ] **Step 1: Fix camera position — try multiple sources, fall back to first unit**

```cpp
// In GameState::setupScenario(), replace camera position code:
MapPos cameraPos;
bool cameraSet = false;

// Try scenario camera position (note: X and Y are swapped in genie format)
if (scenario_->playerData.player1CameraX > 0 && scenario_->playerData.player1CameraY > 0) {
    cameraPos = MapPos(scenario_->playerData.player1CameraY * Constants::TILE_SIZE,
                       scenario_->playerData.player1CameraX * Constants::TILE_SIZE);
    cameraSet = true;
}

// Try per-player camera position
if (!cameraSet && humanPlayerId < scenario_->players.size()) {
    float cx = scenario_->players[humanPlayerId].initCameraX;
    float cy = scenario_->players[humanPlayerId].initCameraY;
    if (cx > 0 && cy > 0) {
        cameraPos = MapPos(cy * Constants::TILE_SIZE, cx * Constants::TILE_SIZE);
        cameraSet = true;
    }
}

// Fall back to first human player unit position
if (!cameraSet) {
    for (const genie::ScnUnit &u : scenario_->playerUnits[humanPlayerId].units) {
        cameraPos = MapPos(u.positionY * Constants::TILE_SIZE, u.positionX * Constants::TILE_SIZE);
        cameraSet = true;
        break;
    }
}

if (cameraSet) {
    renderTarget_->camera()->setTargetPosition(cameraPos);
}
```

- [ ] **Step 2: Build and test**
- [ ] **Step 3: Commit**

```bash
git commit -m "fix: correct camera start position for campaign scenarios"
```

---

### Task 2: Fix UI overlay (bottom panel background)

**Problem:** Bottom UI panel (action buttons, unit info, minimap) has no background image on Android. Buttons and text float over black.

**Root cause:** `Engine::loadUiOverlay()` at `Engine.cpp:437` tries to load SLP overlay files (e.g. `51100.slp` for Viking civ at 1280x1024). HD Edition may not have these in the expected format, or the resolution doesn't match phone screen.

**Files:**
- Modify: `src/Engine.cpp:435-467` (loadUiOverlay)
- Modify: `src/Engine.cpp:469-530` (drawUi)

- [ ] **Step 1: Add fallback when UI overlay SLP not found**

If no SLP overlay loads, draw a solid dark background for the bottom panel area:

```cpp
void Engine::drawUi()
{
    // ... existing selection rect code ...

#ifdef ANDROID
    Size screenSize = renderTarget_->getSize();

    // Top bar background
    renderTarget_->draw(ScreenRect(0, 0, screenSize.width, 28),
        Drawable::Color(30, 20, 10, 220));

    // Bottom panel background — always draw, whether overlay loaded or not
    float panelH = screenSize.height - m_gameAreaHeight;
    if (panelH > 0) {
        // Dark parchment-colored background
        renderTarget_->draw(ScreenRect(0, m_gameAreaHeight, screenSize.width, panelH),
            Drawable::Color(40, 30, 15, 240));

        // Separator line at top of panel
        renderTarget_->draw(ScreenRect(0, m_gameAreaHeight, screenSize.width, 2),
            Drawable::Color(80, 60, 30, 255));
    }

    // Draw SLP overlay on top if available (has proper AoE2 art)
    if (m_uiOverlay && m_uiOverlay->isValid()) {
        float scaleX = screenSize.width / m_uiOverlay->size.width;
        m_uiOverlay->scaleX = scaleX;
        m_uiOverlay->scaleY = scaleX;
        float overlayH = m_uiOverlay->size.height * scaleX;
        renderTarget_->draw(m_uiOverlay, ScreenPos(0, screenSize.height - overlayH));
    }
#else
    renderTarget_->draw(m_uiOverlay, ScreenPos(0, m_uiOverlayOffset));
#endif
    // ... rest of drawUi unchanged ...
```

- [ ] **Step 2: Build and test**
- [ ] **Step 3: Commit**

```bash
git commit -m "fix: draw dark panel background when UI overlay missing on Android"
```

---

### Task 3: Fix gameAreaHeight calculation for Android

**Problem:** `m_gameAreaHeight` may be wrong, causing action panel / unit info to overlap game area or be clipped.

**Files:**
- Modify: `src/Engine.cpp` — where m_gameAreaHeight is set (around line 1000-1040)

- [ ] **Step 1: Set gameAreaHeight based on logical screen size and UI panel**

After `uiSize` is calculated for Android (line 1017):
```cpp
#ifdef ANDROID
    // Game area = screen height minus bottom panel
    // Bottom panel should be ~40% of height to fit action panel + unit info + minimap
    m_gameAreaHeight = uiSize.height * 0.6f;
#endif
```

Verify by checking where `m_gameAreaHeight` is used: it gates mouse clicks between game area and UI (`mousePos.y < m_gameAreaHeight`). It must match where the UI panel starts.

- [ ] **Step 2: Build and test**
- [ ] **Step 3: Commit**

```bash
git commit -m "fix: correct gameAreaHeight for Android screen proportions"
```

---

### Task 4: Fix Minimap rendering

**Problem:** Minimap shows as black diamond on Android.

**Root cause:** `Minimap::draw()` at `Minimap.cpp:41-51` uses hardcoded rect positions for specific screen heights (1024, 768, 600). Android's logical height (e.g. 581) doesn't match any case, falling through to a tiny `else` rect.

**Files:**
- Modify: `src/ui/Minimap.cpp:41-51` (rect calculation)

- [ ] **Step 1: Calculate minimap rect dynamically from screen size**

```cpp
ScreenRect Minimap::rect() const
{
    Size screenSize = m_renderTarget->getSize();
    // Minimap in bottom-right corner, size relative to screen
    int minimapSize = std::min(200, int(screenSize.height * 0.3f));
    int x = screenSize.width - minimapSize - 10;
    int y = screenSize.height - minimapSize - 10;
    return ScreenRect(x, y, minimapSize, minimapSize);
}
```

- [ ] **Step 2: Build and test**
- [ ] **Step 3: Commit**

```bash
git commit -m "fix: dynamic minimap positioning for any screen size"
```

---

### Task 5: Remove CHEAT_VISIBILITY

**Problem:** `CHEAT_VISIBILITY` was enabled during debugging, must be disabled for real gameplay.

**Files:**
- Modify: `src/mechanics/Player.cpp:19`

- [ ] **Step 1: Ensure CHEAT_VISIBILITY is commented out**

```cpp
//#define CHEAT_VISIBILITY 1
```

Verify fog of war works: only tiles around player's units should be visible.

- [ ] **Step 2: Commit**

```bash
git commit -m "fix: disable CHEAT_VISIBILITY — restore fog of war"
```

---

## Phase 2: Missing Unit Commands & Mechanics

### Task 6: Implement Follow command

**Problem:** Follow button shown for military units but no handler. Follow = move to keep up with another unit without attacking.

**Files:**
- Create: `src/actions/ActionFollow.h`
- Create: `src/actions/ActionFollow.cpp`
- Modify: `src/actions/IAction.h` — add Type::Follow
- Modify: `src/ui/ActionPanel.cpp` — wire button
- Modify: `src/mechanics/UnitManager.h` — add SelectingFollowTarget state
- Modify: `src/mechanics/UnitManager.cpp` — handle target selection
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create ActionFollow**

Similar to ActionGuard but without auto-attack. Follow = stay near target, don't engage enemies.

```cpp
// src/actions/ActionFollow.h
#pragma once
#include "IAction.h"
#include "mechanics/Unit.h"

class ActionFollow : public IAction
{
public:
    ActionFollow(const Unit::Ptr &unit, const Unit::Ptr &target);
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Follow; }
private:
    std::weak_ptr<Unit> m_followTarget;
    bool m_isMoving = false;
    static constexpr float FOLLOW_DISTANCE = 64.f;
};
```

```cpp
// src/actions/ActionFollow.cpp
#include "ActionFollow.h"
#include "ActionMove.h"

ActionFollow::ActionFollow(const Unit::Ptr &unit, const Unit::Ptr &target)
    : IAction(Type::Follow, unit, Task()), m_followTarget(target) {}

ActionFollow::UpdateResult ActionFollow::update(Time time)
{
    (void)time;
    Unit::Ptr unit = m_unit.lock();
    Unit::Ptr target = m_followTarget.lock();
    if (!unit || !target) return UpdateResult::Completed;

    float dist = unit->position().distance(target->position());
    if (dist > FOLLOW_DISTANCE && !m_isMoving) {
        m_isMoving = true;
        unit->actions.queueAction(ActionMove::moveUnitTo(unit, target->position()));
        return UpdateResult::Updated;
    }
    if (dist <= FOLLOW_DISTANCE) m_isMoving = false;
    return UpdateResult::NotUpdated;
}
```

- [ ] **Step 2: Add Type::Follow to IAction enum, wire in ActionPanel and UnitManager**

Same pattern as Guard: add `case Command::Follow:` in ActionPanel, add `State::SelectingFollowTarget` in UnitManager, handle in `onLeftClick`.

- [ ] **Step 3: Add to CMakeLists.txt, build, test**
- [ ] **Step 4: Commit**

```bash
git commit -m "feat: implement Follow command"
```

---

### Task 7: Implement Repair command

**Problem:** Repair button shown for villagers but no handler.

**Files:**
- Create: `src/actions/ActionRepair.h`
- Create: `src/actions/ActionRepair.cpp`
- Modify: `src/ui/ActionPanel.cpp`
- Modify: `src/mechanics/UnitManager.h`
- Modify: `src/mechanics/UnitManager.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create ActionRepair**

Repair = villager heals a building, costs resources proportional to building cost.

```cpp
// src/actions/ActionRepair.h
#pragma once
#include "IAction.h"
#include "mechanics/Unit.h"

class ActionRepair : public IAction
{
public:
    ActionRepair(const Unit::Ptr &unit, const Unit::Ptr &building);
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Repair; }
private:
    std::weak_ptr<Unit> m_target;
    static constexpr float REPAIR_RANGE = 16.f;
    static constexpr float REPAIR_RATE = 0.5f; // HP per game tick
};
```

```cpp
// src/actions/ActionRepair.cpp
#include "ActionRepair.h"
#include "ActionMove.h"

ActionRepair::ActionRepair(const Unit::Ptr &unit, const Unit::Ptr &building)
    : IAction(Type::Repair, unit, Task()), m_target(building) {}

ActionRepair::UpdateResult ActionRepair::update(Time time)
{
    (void)time;
    Unit::Ptr unit = m_unit.lock();
    Unit::Ptr target = m_target.lock();
    if (!unit || !target) return UpdateResult::Completed;

    // Check if building is fully repaired
    if (target->healthPoints() >= target->data()->HitPoints) {
        return UpdateResult::Completed;
    }

    float dist = unit->position().distance(target->position());
    if (dist > REPAIR_RANGE) {
        unit->actions.queueAction(ActionMove::moveUnitTo(unit, target->position()));
        return UpdateResult::Updated;
    }

    // Repair: add HP (simplified — no resource cost for now)
    target->setHitpoints(target->healthPoints() + REPAIR_RATE);
    return UpdateResult::Updated;
}
```

- [ ] **Step 2: Add Type::Repair, wire button, add SelectingRepairTarget state**
- [ ] **Step 3: Build, test, commit**

```bash
git commit -m "feat: implement Repair command for villagers"
```

---

### Task 8: Fix victory/defeat — show proper screens

**Problem:** Victory shows "You are victorious!" text, defeat shows placeholder. No restart/quit options.

**Files:**
- Modify: `src/Engine.cpp:273-278` (result overlay)
- Modify: `src/mechanics/GameState.cpp:219` (winner handler)

- [ ] **Step 1: Improve result overlay with restart option**

```cpp
if (state->result != GameState::Result::Running) {
    if (state->result == GameState::Result::Won) {
        m_resultOverlay->string = "Victory! Tap menu to return.";
    } else {
        m_resultOverlay->string = "Defeat. Tap menu to return.";
    }
    // Draw semi-transparent overlay
    renderTarget_->draw(ScreenRect(0, 0, renderTarget_->getSize().width, renderTarget_->getSize().height),
                        Drawable::Color(0, 0, 0, 160));
    renderTarget_->draw(m_resultOverlay);
}
```

- [ ] **Step 2: Commit**

```bash
git commit -m "fix: improved victory/defeat overlay with semi-transparent background"
```

---

### Task 9: Fix elevation damage bonus

**Problem:** `ActionAttack.cpp:178` has `// TODO: damage multiplier from elevation`. Units on higher ground should deal 25% more damage.

**Files:**
- Modify: `src/actions/ActionAttack.cpp:178`

- [ ] **Step 1: Apply elevation damage multiplier**

```cpp
// After calculating base damage, before applying:
float elevationMultiplier = 1.0f;
Unit::Ptr attacker = m_unit.lock();
if (attacker && targetUnit) {
    float attackerZ = attacker->position().z;
    float targetZ = targetUnit->position().z;
    if (attackerZ > targetZ) {
        elevationMultiplier = 1.25f; // 25% bonus for higher ground
    } else if (attackerZ < targetZ) {
        elevationMultiplier = 0.75f; // 25% penalty for lower ground
    }
}
damage *= elevationMultiplier;
```

- [ ] **Step 2: Commit**

```bash
git commit -m "feat: elevation damage bonus — 25% for high ground"
```

---

## Phase 3: Trigger System Completion

### Task 10: Implement missing trigger conditions

**Problem:** Only 7 conditions implemented. Missing: PlayerDefeated, CaptureObject, ResearchTechnology, and others needed for campaign missions.

**Files:**
- Modify: `src/mechanics/ScenarioController.cpp`
- Modify: `src/mechanics/ScenarioController.h`

- [ ] **Step 1: Add PlayerDefeated condition**

Track defeated players and check in condition evaluation.

- [ ] **Step 2: Add ResearchTechnology condition**

Check if a specific tech has been researched by the trigger's source player.

- [ ] **Step 3: Commit**

```bash
git commit -m "feat: implement PlayerDefeated and ResearchTechnology trigger conditions"
```

---

### Task 11: Implement missing trigger effects

**Problem:** Several trigger effects fall through to "not implemented" warning. Key missing ones needed for campaigns.

**Files:**
- Modify: `src/mechanics/ScenarioController.cpp`

Effects to implement:
- `ChangeObjectName` (line 507) — change unit's display name
- `FreezeUnit` — prevent unit from moving
- `ChangeSpeed` — modify unit movement speed
- `EnableDisableObject` — show/hide units

- [ ] **Step 1: Implement ChangeObjectName**

```cpp
case genie::TriggerEffect::ChangeObjectName: {
    forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
        unit->debugName = effect.message;
    });
    break;
}
```

- [ ] **Step 2: Implement other effects**
- [ ] **Step 3: Commit**

```bash
git commit -m "feat: implement ChangeObjectName and other trigger effects"
```

---

## Phase 4: UI Polish for Mobile

### Task 12: Scale action panel buttons for touch

**Problem:** Action panel buttons (5x3 grid) may be too small for finger tapping on phone.

**Files:**
- Modify: `src/ui/ActionPanel.h:336` — `m_buttonSize = 40`
- Modify: `src/ui/ActionPanel.cpp` — button layout

- [ ] **Step 1: Increase button size for Android**

```cpp
#ifdef ANDROID
    int m_buttonSize = 52; // Larger for touch
#else
    int m_buttonSize = 40;
#endif
```

- [ ] **Step 2: Commit**

```bash
git commit -m "fix: larger action panel buttons for touch on Android"
```

---

### Task 13: Fix unit info panel layout

**Problem:** Unit name, HP, attack/defense stats may be cut off or mispositioned on phone screens.

**Files:**
- Modify: `src/ui/UnitInfoPanel.cpp`

- [ ] **Step 1: Adjust unit info positions for Android logical resolution**

Read UnitInfoPanel::draw() and adjust text positions to fit within the bottom panel area on Android.

- [ ] **Step 2: Commit**

```bash
git commit -m "fix: unit info panel layout for Android screens"
```

---

### Task 14: Add "Back to menu" from game

**Problem:** No way to return to scenario browser from in-game. Quit button closes the app entirely.

**Files:**
- Modify: `src/Engine.cpp` — game loop
- Modify: `src/main.cpp` — outer loop

- [ ] **Step 1: Change Quit to return to scenario browser**

Instead of closing the window, set a flag that breaks the game loop and returns to main():
```cpp
} else if (choice == Dialog::Quit) {
    // Instead of closing window, set flag to return to menu
    m_returnToMenu = true;
}
```

In main.cpp, wrap Engine in a loop:
```cpp
while (true) {
    Engine engine;
    engine.createWindow();
    auto scenario = ScenarioBrowser::show(...);
    if (!scenario) break;
    engine.setupGame(scenario);
    engine.start();
    if (!engine.returnToMenu()) break;
}
```

- [ ] **Step 2: Commit**

```bash
git commit -m "feat: return to scenario browser instead of quitting"
```

---

### Task 15: Clean up all debug logging

**Problem:** ALOG calls, `__android_log_print` in TerrainSprite, GameState left from debugging.

**Files:**
- Modify: `src/mechanics/GameState.cpp` — remove ALOG
- Modify: `src/resource/TerrainSprite.cpp` — remove any remaining android_log_print
- Modify: `src/ui/ScenarioBrowser.cpp` — keep ALOG (useful) but remove verbose render logs

- [ ] **Step 1: Remove all temporary debug logs, keep useful ones**
- [ ] **Step 2: Commit**

```bash
git commit -m "chore: clean up temporary debug logging"
```

---

## Phase 5: Standard Victory Conditions

### Task 16: Implement conquest victory

**Problem:** Standard/Conquest victory mode not implemented. Game runs forever.

**Files:**
- Modify: `src/mechanics/ScenarioController.cpp:143-168`

- [ ] **Step 1: Check all enemy players — if all buildings/military destroyed, player wins**

```cpp
case genie::ScnVictory::Standard:
case genie::ScnVictory::Conquest: {
    bool allEnemiesDefeated = true;
    for (const Player::Ptr &player : m_gameState->players()) {
        if (player == m_gameState->humanPlayer()) continue;
        if (player->playerId == 0) continue; // skip Gaia
        if (!m_gameState->humanPlayer()->isEnemy(player->playerId)) continue;
        // Check if player has any military or buildings
        if (m_gameState->unitManager()->playerUnitCount(player->playerId) > 0) {
            allEnemiesDefeated = false;
            break;
        }
    }
    if (allEnemiesDefeated) {
        m_gameState->result = GameState::Result::Won;
    }
    break;
}
```

Note: may need to add `playerUnitCount(int playerId)` to UnitManager.

- [ ] **Step 2: Commit**

```bash
git commit -m "feat: implement conquest victory condition"
```

---

## Summary

| Phase | Tasks | What |
|-------|-------|------|
| 1 — Critical | 1-5 | Camera position, UI overlay, gameAreaHeight, minimap, fog of war |
| 2 — Mechanics | 6-9 | Follow, Repair, victory/defeat screens, elevation bonus |
| 3 — Triggers | 10-11 | Missing conditions and effects for campaigns |
| 4 — UI Polish | 12-15 | Button sizes, unit info layout, back-to-menu, debug cleanup |
| 5 — Victory | 16 | Conquest win condition |

**Total: 16 tasks.** Phase 1 is critical path. Phases 2-5 are independent.

**NOT included (too large, separate plans needed):**
- AI system (51 stub conditions/actions — needs its own plan)
- Save/Load (serialization of full game state — needs its own plan)
- Map Editor (completely rewrite needed)
- Multiplayer (not needed for mobile)
- Random map generation (RMS parser needed)
