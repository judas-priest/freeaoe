# Rally Points Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make production buildings send newly-trained units to a player-chosen rally point instead of spawning them at position+24.
**Architecture:** `Building` holds `waypoint`, `rallyTarget` (weak_ptr<Unit>), and `hasRallyPoint` flag. `UnitManager` gains a `SelectingRallyTarget` state; clicking the map in that state sets the rally on all selected buildings. `finalizeUnit()` reads the flag and issues gather/garrison/attack/move orders. The waypoint flag SLP (3404, already loaded into `GameState::m_waypointFlag`) is drawn by `UnitsRenderer::display()` when a building with a live rally point is selected.
**Tech Stack:** C++, SDL2, existing building/action system

---

## Step 1 — Add `SelectingRallyTarget` to `UnitManager::State` and `selectRallyTarget()` method

**File:** `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.h`

In the `State` enum (line 140-152), add the new value:

```cpp
enum class State {
    PlacingBuilding,
    PlacingWall,
    SelectingAttackTarget,
    SelectingGarrisonTarget,
    SelectingPatrolTarget,
    SelectingGuardTarget,
    SelectingFollowTarget,
    SelectingRepairTarget,
    SelectingConvertTarget,
    SelectingHealTarget,
    SelectingRallyTarget,   // <-- ADD THIS
    Default
};
```

In the public methods section (after `selectHealTarget()`, around line 211), add:

```cpp
void selectRallyTarget();
```

In the `LogPrinter operator<<` at the bottom of the file (line 270-288), add a case for the new state inside the switch before the closing brace:

```cpp
case UnitManager::State::SelectingRallyTarget: os << "SelectingRallyTarget"; break;
```

- [ ] Edit `UnitManager.h`: add `SelectingRallyTarget` to enum, add `selectRallyTarget()` declaration, add case to LogPrinter operator.

---

## Step 2 — Add rally point fields to `Building`

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.h`

After the existing `MapPos waypoint;` field (line 55), add:

```cpp
MapPos waypoint;
std::weak_ptr<Unit> rallyTarget;
bool hasRallyPoint = false;
```

- [ ] Edit `Building.h`: add `rallyTarget` and `hasRallyPoint` after `waypoint`.

---

## Step 3 — Implement `selectRallyTarget()` in `UnitManager.cpp`

**File:** `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp`

After `selectHealTarget()` (around line 1190-1193), add:

```cpp
void UnitManager::selectRallyTarget()
{
    m_state = State::SelectingRallyTarget;
}
```

- [ ] Edit `UnitManager.cpp`: add `selectRallyTarget()` implementation.

---

## Step 4 — Handle `SelectingRallyTarget` in `UnitManager::onLeftClick()`

**File:** `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp`

In `onLeftClick()`, inside the `switch (m_state)` block (around line 379), add a new case before `case State::Default:`. The click either hits a unit (make it the rally target) or bare ground (map position only):

```cpp
case State::SelectingRallyTarget: {
    MapPos targetPos = camera->absoluteMapPos(screenPos);
    Unit::Ptr targetUnit = unitAt(screenPos, camera, NoAlignment);

    for (const Unit::Ptr &unit : m_selectedUnits) {
        if (unit->playerId() != humanPlayer->playerId) {
            continue;
        }
        Building::Ptr building = Building::fromUnit(unit);
        if (!building) {
            continue;
        }
        building->waypoint = targetUnit ? targetUnit->position() : targetPos;
        building->rallyTarget = targetUnit;
        building->hasRallyPoint = true;
    }
    m_state = State::Default;
    emit(ActionsChanged);
    return true;
}
```

Note: `Building.h` is already included in `UnitManager.cpp` (it includes `Unit.h` and `Building` is forward-declared; check the actual includes and add `#include "Building.h"` if missing).

- [ ] Edit `UnitManager.cpp`: add `SelectingRallyTarget` case in `onLeftClick()` switch.
- [ ] Verify `#include "mechanics/Building.h"` is present in `UnitManager.cpp`; add it if not.

---

## Step 5 — Wire `SetRallyPoint` and `RemoveRallyPoint` in `ActionPanel::handleButtonClick()`

**File:** `/home/dima/Projects/freeaoe/src/ui/ActionPanel.cpp`

In `handleButtonClick()`, inside the `else if (button.type == InterfaceButton::Other)` block's switch statement (around line 700), add two cases before `default:`:

```cpp
case Command::SetRallyPoint:
    m_unitManager->selectRallyTarget();
    break;
case Command::RemoveRallyPoint:
    for (const Unit::Ptr &unit : m_selectedUnits) {
        Building::Ptr building = Building::fromUnit(unit);
        if (!building) {
            continue;
        }
        building->waypoint = building->position() + MapPos(24, 24);
        building->rallyTarget.reset();
        building->hasRallyPoint = false;
    }
    break;
```

`Building.h` is already included at the top of `ActionPanel.cpp` (line 3).

- [ ] Edit `ActionPanel.cpp`: add `SetRallyPoint` and `RemoveRallyPoint` cases in the switch inside `handleButtonClick()`.

---

## Step 6 — Update `ActionPanel::updateButtons()` to show `RemoveRallyPoint` when a rally is active

**File:** `/home/dima/Projects/freeaoe/src/ui/ActionPanel.cpp`

In `addCreateButtons()`, the `SetRallyPoint` button is added at index 4 when `unit->data()->Type >= genie::Unit::BuildingType` (around line 488-493). Replace that block so it shows `RemoveRallyPoint` instead when the building already has one:

```cpp
if (unit->data()->Type >= genie::Unit::BuildingType) {
    Building::Ptr building = Building::fromUnit(unit);
    InterfaceButton rallypointButton;
    if (building && building->hasRallyPoint) {
        rallypointButton.action = Command::RemoveRallyPoint;
    } else {
        rallypointButton.action = Command::SetRallyPoint;
    }
    rallypointButton.index = 4;
    currentButtons.push_back(rallypointButton);
}
```

- [ ] Edit `ActionPanel.cpp`: update the rally button block in `addCreateButtons()` to toggle between Set/Remove.

---

## Step 7 — Update `Building::finalizeUnit()` to dispatch the newly-trained unit

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.cpp`

Replace the current `finalizeUnit()` (lines 417-441). Spawn position is still near the building. If `hasRallyPoint` is set, issue the appropriate follow-up action.

Required includes already present: `UnitManager.h`, `UnitFactory.h`. Add at top of file if not already there:

```cpp
#include "actions/ActionMove.h"
#include "actions/ActionGather.h"
#include "actions/ActionAttack.h"
#include "actions/ActionGarrison.h"
```

Replacement `finalizeUnit()`:

```cpp
void Building::finalizeUnit() noexcept
{
    Player::Ptr owner = player().lock();
    if (!owner) {
        WARN << "owner went away";
        return;
    }

    // Always spawn adjacent to the building
    const MapPos spawnPos(position().x + 24, position().y + 24);

    Unit::Ptr unit = UnitFactory::Inst().createUnit(m_currentProduct->unit->ID, owner, m_unitManager);
    if (!unit) {
        WARN << "Failed to finalize unit";
        return;
    }
    m_unitManager.add(unit, spawnPos);

    Player::Ptr unitPlayer = unit->player().lock();
    if (unitPlayer && unitPlayer->playerId == m_unitManager.humanPlayerID()) {
        AudioPlayer::instance().playSound(unit->data()->TrainSound, unitPlayer->civilization.id());
    }

    DBG << "Finalized" << unit->debugName;

    if (!hasRallyPoint) {
        return;
    }

    Unit::Ptr target = rallyTarget.lock();

    if (target && target->isAlive()) {
        // Rally on a unit — determine intent by relationship and resource type
        if (!owner->isAllied(target->playerId())) {
            // Enemy unit — attack
            Task task = unit->actions.findAnyTask(genie::ActionType::Attack, target->data()->ID);
            if (task.data) {
                task.target = target;
                unit->actions.setCurrentAction(std::make_shared<ActionAttack>(unit, task));
                return;
            }
        }

        if (target->data()->GarrisonCapacity > 0 && owner->isAllied(target->playerId())) {
            // Allied building/unit with garrison capacity — garrison
            Task task = unit->actions.findAnyTask(genie::ActionType::Garrison, target->data()->ID);
            if (task.data) {
                task.target = target;
                IAction::assignTask(task, unit, IAction::AssignType::Replace);
                return;
            }
        }

        // Resource — gather
        Task task = unit->actions.findAnyTask(genie::ActionType::Gather, target->data()->ID);
        if (task.data) {
            task.target = target;
            unit->actions.setCurrentAction(std::make_shared<ActionGather>(unit, task));
            return;
        }
    }

    // No usable target or bare ground rally — just move there
    unit->actions.setCurrentAction(ActionMove::moveUnitTo(unit, waypoint));
}
```

- [ ] Add missing `#include` lines for action headers in `Building.cpp` if not present.
- [ ] Edit `Building.cpp`: replace `finalizeUnit()` with the above.

---

## Step 8 — Render the waypoint flag in `UnitsRenderer::display()`

**File:** `/home/dima/Projects/freeaoe/src/render/UnitsRenderer.cpp`

The flag SLP is in `GameState` but `UnitsRenderer` only has a `weak_ptr<UnitManager>`. The simplest approach: expose the waypoint flag image via `UnitsRenderer` by giving it a setter.

**Sub-step 8a** — Add flag image field and setter to `UnitsRenderer.h`:

```cpp
// in UnitsRenderer.h, public section:
void setWaypointFlagSlp(const std::shared_ptr<genie::SlpFile> &slp) { m_waypointFlagSlp = slp; }

// in UnitsRenderer.h, private section:
std::shared_ptr<genie::SlpFile> m_waypointFlagSlp;
Drawable::Image::Ptr m_waypointFlagImage; // cached after first conversion
```

Add `#include <genie/resource/SlpFile.h>` to `UnitsRenderer.h`.

**Sub-step 8b** — In `UnitsRenderer::display()`, after the move-target-marker block and before the `PlacingBuilding` block (around line 296), add:

```cpp
// Draw waypoint flags for selected buildings that have a rally point
if (m_waypointFlagSlp && !m_waypointFlagImage) {
    m_waypointFlagImage = renderTarget->convertFrameToImage(m_waypointFlagSlp->getFrame(0));
}

if (m_waypointFlagImage) {
    for (const Unit::Ptr &unit : unitManager->selected()) {
        if (!unit->isBuilding()) {
            continue;
        }
        Building::Ptr building = Building::fromUnit(unit);
        if (!building || !building->hasRallyPoint) {
            continue;
        }
        ScreenPos flagPos = camera->absoluteScreenPos(building->waypoint);
        renderTarget->draw(m_waypointFlagImage, flagPos);
    }
}
```

Add `#include "mechanics/Building.h"` to `UnitsRenderer.cpp`.

**Sub-step 8c** — In `GameState` (wherever `m_unitsRenderer` is initialised / `setUnitManager` is called), call:

```cpp
m_unitsRenderer.setWaypointFlagSlp(m_waypointFlag);
```

Find the right location by checking where `m_unitsRenderer.setUnitManager(...)` is called in `GameState.cpp`.

- [ ] Edit `UnitsRenderer.h`: add `setWaypointFlagSlp()`, `m_waypointFlagSlp`, `m_waypointFlagImage`, and the SlpFile include.
- [ ] Edit `UnitsRenderer.cpp`: add Building include and the flag-drawing block in `display()`.
- [ ] Edit `GameState.cpp`: call `m_unitsRenderer.setWaypointFlagSlp(m_waypointFlag)` after loading the SLP.

---

## Step 9 — Build and test on desktop

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc) 2>&1 | head -60
```

Expected: clean build (no new warnings on the new code paths).

Manual test checklist:
- Select a barracks, click SetRallyPoint button (index 4), click map — newly trained militia walks to that spot.
- Click RemoveRallyPoint — next trained unit spawns at position+24 as before.
- Rally on a resource (tree/gold/stone/berry bush) — unit begins gathering.
- Rally on an enemy — unit attacks.
- Waypoint flag sprite appears at rally location while building is selected; disappears when deselected or removed.

- [ ] Run desktop build.
- [ ] Manually verify the four scenarios above.

---

## Step 10 — Build for Android

```bash
cd /home/dima/Projects/freeaoe/android && rm -rf app/.cxx app/build && unset LD_PRELOAD && ./gradlew assembleDebug 2>&1 | tail -30
```

- [ ] Android build succeeds.
- [ ] Install and smoke-test: `unset LD_PRELOAD && adb install -r android/app/build/outputs/apk/debug/app-debug.apk`

---

## Key files

- `/home/dima/Projects/freeaoe/src/mechanics/Building.h` — new fields `rallyTarget`, `hasRallyPoint`
- `/home/dima/Projects/freeaoe/src/mechanics/Building.cpp` — updated `finalizeUnit()`
- `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.h` — new `SelectingRallyTarget` state + declaration
- `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp` — `selectRallyTarget()` + `onLeftClick()` case
- `/home/dima/Projects/freeaoe/src/ui/ActionPanel.cpp` — `handleButtonClick()` cases + `addCreateButtons()` toggle
- `/home/dima/Projects/freeaoe/src/render/UnitsRenderer.h` / `.cpp` — flag rendering
- `/home/dima/Projects/freeaoe/src/mechanics/GameState.cpp` — pass SLP to renderer
