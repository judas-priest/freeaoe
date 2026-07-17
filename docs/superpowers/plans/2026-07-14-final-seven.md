# Final Seven Quick Fixes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the 7 remaining quick fixes from the techdebt audit to reach ~99% feature parity with AoE2 core gameplay.

**Architecture:** All fixes are small, independent changes mostly in Engine.cpp, ActionPanel.cpp, and ActionConvert.cpp. No new files except none — all changes modify existing code.

**Tech Stack:** C++, SDL2, existing action/player/visibility systems

---

## Task 1: Select All Military Hotkey

**Files:**
- Modify: `src/Engine.cpp` (handleKeyEvent)

- [ ] **Step 1: Add Ctrl+A handler in handleKeyEvent**

In `src/Engine.cpp`, in the `handleKeyEvent` switch, find the existing `case input::Key::A:` (attack-move). Replace it to check for Ctrl modifier:

```cpp
    case input::Key::A:
        if (event.key.shift) {
            // Shift+A or Ctrl+Shift+A: select all military on screen
            const Player::Ptr &human = state->humanPlayer();
            if (human) {
                UnitVector military;
                for (const Unit::Ptr &unit : state->unitManager()->units()) {
                    if (unit->playerId() != human->playerId) continue;
                    if (unit->data()->Class == genie::Unit::Civilian) continue;
                    if (unit->data()->Type >= genie::Unit::BuildingType) continue;
                    if (!unit->isVisible) continue;
                    military.push_back(unit);
                }
                if (!military.empty()) {
                    state->unitManager()->setSelectedUnits(military);
                }
            }
            return true;
        }
        // Plain A: attack-move
        if (!state->unitManager()->selected().isEmpty()) {
            state->unitManager()->selectAttackMoveTarget();
        }
        return true;
```

Note: `event.key.shift` is a `bool` field on `KeyEvent` (see `EventTypes.h:40`).

- [ ] **Step 2: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

- [ ] **Step 3: Commit**

```bash
git add src/Engine.cpp
git commit -m "feat: Shift+A selects all visible military units"
```

---

## Task 2: Camera Follow Unit (F6)

**Files:**
- Modify: `src/Engine.h` (add m_followUnit)
- Modify: `src/Engine.cpp` (F6 handler + updateCamera)

- [ ] **Step 1: Add follow state to Engine.h**

After the `m_objectivesVisible` member:

```cpp
    // Camera follow
    std::weak_ptr<Unit> m_followUnit;
```

- [ ] **Step 2: Add F6 handler in handleKeyEvent**

```cpp
    case input::Key::F6: {
        if (m_followUnit.lock()) {
            m_followUnit.reset(); // toggle off
            addMessage("Camera follow OFF");
        } else {
            const auto &sel = state->unitManager()->selected();
            if (!sel.isEmpty()) {
                m_followUnit = sel.first();
                addMessage("Camera follow ON");
            }
        }
        return true;
    }
```

- [ ] **Step 3: Update camera in updateCamera()**

In `Engine::updateCamera()`, after the `#ifdef ANDROID return false;` guard and before the edge-panning block, add:

```cpp
    // Camera follow mode
    Unit::Ptr followTarget = m_followUnit.lock();
    if (followTarget && followTarget->isAlive()) {
        renderTarget_->camera()->setTargetPosition(followTarget->position());
        return true;
    } else if (followTarget) {
        m_followUnit.reset(); // target died
    }
```

Cancel follow on manual camera input — in the edge-panning block, if `m_cameraDeltaX != 0 || m_cameraDeltaY != 0`:

```cpp
    if (m_cameraDeltaX != 0 || m_cameraDeltaY != 0) {
        m_followUnit.reset(); // manual camera movement cancels follow
    }
```

- [ ] **Step 4: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

- [ ] **Step 5: Commit**

```bash
git add src/Engine.h src/Engine.cpp
git commit -m "feat: F6 toggles camera follow on selected unit"
```

---

## Task 3: Idle Villager UI Button

**Files:**
- Modify: `src/ui/ActionPanel.h` (add FindIdleVillager command)
- Modify: `src/ui/ActionPanel.cpp` (add button + handler)

- [ ] **Step 1: Add command to enum**

In `src/ui/ActionPanel.h`, in the `Command` enum, add after `AutoScout`:

```cpp
        FindIdleVillager = 243,
```

Add to the LogPrinter switch:

```cpp
    case ActionPanel::Command::FindIdleVillager: os << "FindIdleVillager"; break;
```

- [ ] **Step 2: Add button in updateButtons**

In `src/ui/ActionPanel.cpp`, in `addMilitaryButtons()` or `addDefaultButtons()` — find where buttons are added when no unit is selected. Add the idle villager button to the default (no-selection) button set, or add it for civilian units.

Actually, the simplest approach: add it in the main `updateButtons()` method as a persistent button. Find where the help text map is defined and add:

```cpp
        { Command::FindIdleVillager, 4922 }, // "Find Idle Villager" help text
```

In `addDefaultButtons()` (or the appropriate location where buttons are shown when units are selected):

```cpp
    // Idle villager button — always available
    InterfaceButton idleBtn;
    idleBtn.action = Command::FindIdleVillager;
    idleBtn.index = 9; // bottom-right of action panel
    currentButtons.push_back(idleBtn);
```

- [ ] **Step 3: Add handler in handleButtonClick**

In the command switch:

```cpp
        case Command::FindIdleVillager: {
            // Reuse the F1 idle villager cycle logic
            Player::Ptr human = m_humanPlayer.lock();
            if (!human) break;
            static int lastIdleIdx = -1;
            int startIdx = lastIdleIdx + 1;
            const auto &allUnits = m_unitManager->units();
            for (size_t i = 0; i < allUnits.size(); i++) {
                int idx = (startIdx + i) % allUnits.size();
                const Unit::Ptr &unit = allUnits[idx];
                if (!unit || unit->playerId() != human->playerId) continue;
                if (unit->data()->Class != genie::Unit::Civilian) continue;
                if (unit->actions.currentAction()) continue; // not idle
                lastIdleIdx = idx;
                UnitVector sel = { unit };
                m_unitManager->setSelectedUnits(sel);
                break;
            }
            break;
        }
```

- [ ] **Step 4: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

- [ ] **Step 5: Commit**

```bash
git add src/ui/ActionPanel.h src/ui/ActionPanel.cpp
git commit -m "feat: idle villager UI button in action panel"
```

---

## Task 4: Signal Flare

**Files:**
- Modify: `src/ui/ActionPanel.cpp` (handler)
- Modify: `src/mechanics/UnitManager.h` (add SelectingFlareTarget state)
- Modify: `src/mechanics/UnitManager.cpp` (handle flare click)

- [ ] **Step 1: Add state to UnitManager::State enum**

In `src/mechanics/UnitManager.h`, add before `Default`:

```cpp
        SelectingFlareTarget,
```

Add declaration:

```cpp
    void selectFlareTarget();
```

- [ ] **Step 2: Implement selectFlareTarget and onLeftClick handler**

In `src/mechanics/UnitManager.cpp`:

```cpp
void UnitManager::selectFlareTarget()
{
    m_state = State::SelectingFlareTarget;
}
```

In `onLeftClick()` switch, before `case State::Default:`:

```cpp
    case State::SelectingFlareTarget: {
        MapPos targetPos = camera->absoluteMapPos(screenPos);
        // Show the move target marker as a visual ping at the flare position
        m_moveTargetMarker->moveTo(targetPos);
        break;
    }
```

- [ ] **Step 3: Wire button in ActionPanel**

In `handleButtonClick`:

```cpp
        case Command::SignalFlare:
            m_unitManager->selectFlareTarget();
            break;
```

- [ ] **Step 4: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

- [ ] **Step 5: Commit**

```bash
git add src/ui/ActionPanel.cpp src/mechanics/UnitManager.h src/mechanics/UnitManager.cpp
git commit -m "feat: signal flare — click map to ping a location"
```

---

## Task 5: Map Reveal (marco cheat)

**Files:**
- Modify: `src/mechanics/Player.h` (add revealAll to VisibilityMap)
- Modify: `src/Engine.cpp` (onChatMessage cheat check)

- [ ] **Step 1: Add revealAll() to VisibilityMap**

In `src/mechanics/Player.h`, in `struct VisibilityMap`, add after `setExplored`:

```cpp
    void revealAll() {
        std::fill(m_visibility.begin(), m_visibility.end(), Visible);
        isDirty = true;
    }
```

- [ ] **Step 2: Add cheat detection in onChatMessage**

In `src/Engine.cpp`, in `Engine::onChatMessage`, after the `addMessage(display)` call:

```cpp
    // Cheat codes
    if (message == "marco") {
        auto activeState = state_manager_.getActiveState();
        if (activeState) {
            const Player::Ptr &human = activeState->humanPlayer();
            if (human && human->visibility) {
                human->visibility->revealAll();
                addMessage("Map revealed!");
            }
        }
    }
```

Check how visibility is accessed from Player. Look for `visibility` member:

In `src/mechanics/Player.h`, the `VisibilityMap` is stored as:

```cpp
    std::shared_ptr<VisibilityMap> visibility;
```

This is already a public member.

- [ ] **Step 3: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/Player.h src/Engine.cpp
git commit -m "feat: marco cheat — type in chat to reveal entire map"
```

---

## Task 6: Monk Tech Effects (Heresy/Theocracy/Illumination/Faith)

**Files:**
- Modify: `src/actions/ActionConvert.cpp` (all 4 tech checks)

- [ ] **Step 1: Add Heresy check in doConvert**

In `ActionConvert::doConvert()`, after `target->setPlayer(monkOwner)`, add:

```cpp
    // Heresy: converted units die instead of switching sides
    auto targetOwner = target->player().lock();
    if (targetOwner && targetOwner->hasResearched(439)) { // Heresy tech ID
        target->kill();
        DBG << "Heresy: converted unit killed";
    } else {
        target->setPlayer(monkOwner);
    }
```

Wait — the order matters. Heresy is researched by the **target's original owner** (the defending player). So check the target's player BEFORE conversion:

```cpp
void ActionConvert::doConvert(const Unit::Ptr &monk, const Unit::Ptr &target)
{
    auto monkOwner = monk->player().lock();
    if (!monkOwner) return;

    // Check if target's owner has Heresy — unit dies instead of converting
    auto targetOwner = target->player().lock();
    if (targetOwner && targetOwner->hasResearched(439)) {
        DBG << "Heresy: " << target->debugName << " killed instead of converted";
        target->kill();
    } else {
        DBG << "Converted" << target->debugName << "to player" << monkOwner->playerId;
        target->setPlayer(monkOwner);
    }

    // Drain monk's faith to 0
    monk->resources[genie::ResourceType::Faith] = 0.f;
}
```

- [ ] **Step 2: Add Faith tech resistance in conversion start**

In `ActionConvert::update()`, after the cavalry/siege resistance blocks (around line 81), add:

```cpp
        // Faith tech: target's owner researched Faith (45) → +50% resistance
        auto targetOwner = target->player().lock();
        if (targetOwner && targetOwner->hasResearched(45)) {
            m_minConvertTime = m_minConvertTime * 3 / 2;
            m_maxConvertTime = m_maxConvertTime * 3 / 2;
        }
```

- [ ] **Step 3: Add Illumination in faith recharge**

In `ActionConvert::update()`, in the faith recharge block (line 36), replace the delta calculation:

```cpp
        // Recharge faith passively
        float &f = monk->resources[genie::ResourceType::Faith];
        if (f < FAITH_MAX) {
            float rechargeRate = FAITH_RECHARGE_PER_MS;
            // Illumination tech (233): 2x faith recharge
            auto monkOwner = monk->player().lock();
            if (monkOwner && monkOwner->hasResearched(233)) {
                rechargeRate *= 2.f;
            }
            const float delta = (time - m_prevTime) * rechargeRate;
            f = std::min(FAITH_MAX, f + delta);
        }
```

- [ ] **Step 4: Theocracy note**

Theocracy (438) means only one monk in a group conversion loses faith. This requires knowing if multiple monks are converting the same target simultaneously — which the current action system doesn't track per-target. **Skip for now** — it's a multiplayer/group-micro optimization. Leave a comment:

```cpp
    // TODO: Theocracy (438) — only one monk loses faith in group conversion
    //       Requires tracking multiple monks converting the same target
```

- [ ] **Step 5: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

- [ ] **Step 6: Commit**

```bash
git add src/actions/ActionConvert.cpp
git commit -m "feat: monk techs — Heresy kills on convert, Faith +50% resistance, Illumination 2x recharge"
```

---

## Task 7: Dynamic Market Prices

**Files:**
- Modify: `src/mechanics/Player.h` (add market price tracking)
- Modify: `src/ui/ActionPanel.cpp` (use dynamic prices)

- [ ] **Step 1: Add market price state to Player**

In `src/mechanics/Player.h`, in `struct Player`, add in the public section:

```cpp
    // Market price tracking — sell price drops, buy price rises with volume
    // Index: 0=wood, 1=food, 2=stone
    struct MarketPrices {
        float sellPrice[3] = { 70.f, 70.f, 70.f };   // gold received per 100 resource sold
        float buyPrice[3]  = { 130.f, 130.f, 130.f }; // gold cost per 100 resource bought

        void onSell(int resourceIdx) {
            sellPrice[resourceIdx] = std::max(20.f, sellPrice[resourceIdx] - 3.f);
            buyPrice[resourceIdx]  = std::max(30.f, buyPrice[resourceIdx] - 3.f);
        }
        void onBuy(int resourceIdx) {
            sellPrice[resourceIdx] = std::min(200.f, sellPrice[resourceIdx] + 3.f);
            buyPrice[resourceIdx]  = std::min(300.f, buyPrice[resourceIdx] + 3.f);
        }
    } marketPrices;
```

Add `#include <algorithm>` if not already present.

- [ ] **Step 2: Replace hardcoded prices in ActionPanel.cpp**

Replace the sell blocks (lines 780-808):

```cpp
        case Command::SellWood: {
            auto player = m_unitManager->humanPlayer();
            if (player && player->resourcesAvailable(genie::ResourceType::WoodStorage) >= 100) {
                player->setAvailableResource(genie::ResourceType::WoodStorage,
                    player->resourcesAvailable(genie::ResourceType::WoodStorage) - 100);
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) + player->marketPrices.sellPrice[0]);
                player->marketPrices.onSell(0);
            }
            break;
        }
        case Command::SellFood: {
            auto player = m_unitManager->humanPlayer();
            if (player && player->resourcesAvailable(genie::ResourceType::FoodStorage) >= 100) {
                player->setAvailableResource(genie::ResourceType::FoodStorage,
                    player->resourcesAvailable(genie::ResourceType::FoodStorage) - 100);
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) + player->marketPrices.sellPrice[1]);
                player->marketPrices.onSell(1);
            }
            break;
        }
        case Command::SellStone: {
            auto player = m_unitManager->humanPlayer();
            if (player && player->resourcesAvailable(genie::ResourceType::StoneStorage) >= 100) {
                player->setAvailableResource(genie::ResourceType::StoneStorage,
                    player->resourcesAvailable(genie::ResourceType::StoneStorage) - 100);
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) + player->marketPrices.sellPrice[2]);
                player->marketPrices.onSell(2);
            }
            break;
        }
```

Replace the buy blocks (lines 810-833):

```cpp
        case Command::CollectWood:
        case Command::BuyFood:
        case Command::CollectFood: {
            auto player = m_unitManager->humanPlayer();
            int resIdx = (button.action == Command::CollectWood) ? 0 : 1;
            float cost = player ? player->marketPrices.buyPrice[resIdx] : 130.f;
            if (player && player->resourcesAvailable(genie::ResourceType::GoldStorage) >= cost) {
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) - cost);
                genie::ResourceType res = (button.action == Command::CollectWood)
                    ? genie::ResourceType::WoodStorage : genie::ResourceType::FoodStorage;
                player->setAvailableResource(res, player->resourcesAvailable(res) + 100);
                player->marketPrices.onBuy(resIdx);
            }
            break;
        }
        case Command::CollectStone:
        case Command::BuyStone: {
            auto player = m_unitManager->humanPlayer();
            float cost = player ? player->marketPrices.buyPrice[2] : 130.f;
            if (player && player->resourcesAvailable(genie::ResourceType::GoldStorage) >= cost) {
                player->setAvailableResource(genie::ResourceType::GoldStorage,
                    player->resourcesAvailable(genie::ResourceType::GoldStorage) - cost);
                player->setAvailableResource(genie::ResourceType::StoneStorage,
                    player->resourcesAvailable(genie::ResourceType::StoneStorage) + 100);
                player->marketPrices.onBuy(2);
            }
            break;
        }
```

- [ ] **Step 3: Build and verify**

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/Player.h src/ui/ActionPanel.cpp
git commit -m "feat: dynamic market prices — sell/buy prices shift with each trade"
```

---

## Build & Test All

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Android:
```bash
cd /home/dima/Projects/freeaoe/android && rm -rf app/.cxx app/build && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && adb install -r app/build/outputs/apk/debug/app-debug.apk
```

## Test Checklist

1. **Shift+A** — selects all visible military units (not villagers, not buildings)
2. **F6** — camera follows selected unit; F6 again to stop; manual scroll cancels
3. **Idle villager button** — tap button in action panel, cycles to next idle villager
4. **Signal flare** — tap flare button, tap map, marker appears at location
5. **marco** — type in chat, fog of war disappears
6. **Monk + Heresy** — research Heresy as defender, monk converts your unit → unit dies
7. **Market** — sell wood 3x, each sell gives less gold (~70, 67, 64...)
