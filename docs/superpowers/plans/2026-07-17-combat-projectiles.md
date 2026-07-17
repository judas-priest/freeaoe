# Combat & Projectiles Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete combat -- formations (already partially done), transport ships, trebuchet pack/unpack polish, petards (self-destruct), double-click select-by-type on desktop, Ctrl+click add/remove selection

**Architecture:**
- `Unit::Formation` enum (Line/Box/Flank/SpreadOut) exists in `/home/dima/Projects/freeaoe/src/mechanics/Unit.h:138-144` with a global static `s_formation`.
- Formation movement logic is already implemented in `UnitManager::onRightClick()` at `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp:736-809` -- calculates offsets per unit based on active formation. ActionPanel buttons (Line/Box/Flank/SpreadOut) already set `s_formation`.
- Trebuchet pack/unpack is already wired: `ActionPanel.cpp:891-916` handles Command::Pack/Unpack by creating `ActionTransform` to swap unit IDs 331 (packed) <-> 42 (unpacked) with 11.1s duration. `ActionTransform` (`/home/dima/Projects/freeaoe/src/actions/ActionTransform.cpp`) swaps genie unit data via `unit->setUnitData()`.
- Garrison logic lives in `ActionGarrison` (`/home/dima/Projects/freeaoe/src/actions/ActionGarrison.cpp`) and only accepts `Building::Ptr` targets, not generic units.
- Selection is in `UnitManager::selectUnits()` (`UnitManager.cpp:960-1014`). Double-tap select-by-type exists for touch (Engine.cpp:1559-1571) but NOT for desktop mouse.
- Desktop mouse events go through `handleMouseRelease()` (`Engine.cpp:1627-1678`). The `input::MouseButtonEvent` struct has no `clicks` field, but SDL2's `sdlEvent.button.clicks` provides double-click count.
- `input::Event` is defined in `/home/dima/Projects/freeaoe/src/render/EventTypes.h`. SDL event conversion in `/home/dima/Projects/freeaoe/src/render/SdlRenderTarget.cpp:940-962`.

**Tech Stack:** C++20, SDL2

**Build/test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)` -- no unit tests, verify by running.

---

## Task 1: Add `clicks` field to input::MouseButtonEvent (2 min)

SDL2 tracks double-clicks natively via `sdlEvent.button.clicks`. Expose it.

**File:** `/home/dima/Projects/freeaoe/src/render/EventTypes.h`

Add `clicks` field to `MouseButtonEvent`:

```cpp
    struct MouseButtonEvent {
        MouseButton button;
        int x = 0;
        int y = 0;
        int clicks = 1;  // 1=single, 2=double, etc.
    };
```

**File:** `/home/dima/Projects/freeaoe/src/render/SdlRenderTarget.cpp`

In the `SDL_MOUSEBUTTONDOWN` case (line ~940), after setting x/y, add:

```cpp
        event.mouseButton.clicks = sdlEvent.button.clicks;
```

In the `SDL_MOUSEBUTTONUP` case (line ~952), add the same:

```cpp
        event.mouseButton.clicks = sdlEvent.button.clicks;
```

**File:** `/home/dima/Projects/freeaoe/src/Engine.cpp` (SFML path, line ~139)

In the `sfEventToInput` function for `MouseButtonPressed`/`MouseButtonReleased`, leave `clicks` at default 1 (SFML does not provide it).

**Commit:** `feat: expose SDL2 double-click count in input events`

---

## Task 2: Double-click to select all units of same type on screen (3 min)

On desktop, double-clicking a unit should select all visible units of the same type owned by the same player, mirroring the touch double-tap logic already at `Engine.cpp:1559-1571`.

**File:** `/home/dima/Projects/freeaoe/src/Engine.cpp`

In `handleMouseRelease()` (line ~1660), where single left-click selection happens, add a double-click check BEFORE the existing `selectUnits` call:

```cpp
    if (event.mouseButton.button == input::MouseButton::Left && m_selecting) {
        m_selecting = false;
        m_selectionRect = ScreenRect();

        // Double-click: select all visible units of same type
        if (event.mouseButton.clicks >= 2) {
            Unit::Ptr clickedUnit = state->unitManager()->unitAt(
                mousePos, renderTarget_->camera(), NoAlignment);
            if (clickedUnit && clickedUnit->data()) {
                Size ss = renderTarget_->getSize();
                ScreenRect gameArea(ScreenPos(0, 0), ScreenPos(ss.width, m_gameAreaHeight));
                state->unitManager()->selectUnitsByType(
                    clickedUnit->data()->ID, clickedUnit->playerId(),
                    gameArea, renderTarget_->camera());
                return true;
            }
        }

        // Single click: normal box/point select (existing code)
        ScreenRect selectRect(m_selectionStart, mousePos);
        if (selectRect.width < 15 && selectRect.height < 15) {
            selectRect = ScreenRect(mousePos - ScreenPos(15, 15), mousePos + ScreenPos(15, 15));
        }
        state->unitManager()->selectUnits(selectRect, renderTarget_->camera());
        return true;
    }
```

This replaces lines 1660-1668. The key change is the `if (event.mouseButton.clicks >= 2)` branch before the existing selection code.

**Commit:** `feat: double-click selects all units of same type on screen`

---

## Task 3: Ctrl+click to add/remove units from selection (3 min)

Ctrl+click on a unit should toggle it in/out of the current selection without clearing other selected units.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.h`

Add method declaration:

```cpp
    void toggleUnitInSelection(const Unit::Ptr &unit);
```

**File:** `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp`

Add implementation after `setSelectedUnits`:

```cpp
void UnitManager::toggleUnitInSelection(const Unit::Ptr &unit)
{
    if (!unit) return;
    if (m_selectedUnits.contains(unit)) {
        EventManager::unitDeselected(unit.get());
        m_selectedUnits.remove(unit);
    } else {
        m_selectedUnits.add(unit);
        EventManager::unitSelected(unit.get());
    }
    m_availableActionsChanged = true;
}
```

**File:** `/home/dima/Projects/freeaoe/src/Engine.cpp`

In `handleMouseRelease()`, inside the left-button selection block, after the double-click check and BEFORE the normal selectUnits call, add a Ctrl+click check:

```cpp
        // Ctrl+click: toggle unit in selection
        bool ctrlHeld = (SDL_GetModState() & KMOD_CTRL) != 0;
        if (ctrlHeld) {
            ScreenRect clickRect(mousePos - ScreenPos(15, 15), mousePos + ScreenPos(15, 15));
            Unit::Ptr clickedUnit = state->unitManager()->unitAt(
                mousePos, renderTarget_->camera(), NoAlignment);
            if (clickedUnit) {
                state->unitManager()->toggleUnitInSelection(clickedUnit);
                return true;
            }
        }
```

**Commit:** `feat: Ctrl+click to add/remove units from selection`

---

## Task 4: Petard self-destruct on attack (4 min)

Petards (genie class `genie::Unit::Petard`, unit ID 440) should die after dealing their attack. The attack itself already works via `ActionAttack` -- we just need the self-destruct after damage is dealt.

**File:** `/home/dima/Projects/freeaoe/src/actions/ActionAttack.cpp`

In the `update()` method, after damage is dealt (after the missile spawn / direct damage section, around line 200, just before the final `return UpdateResult::Updated`), add:

```cpp
    // Petard: self-destruct after attacking
    if (unit->data()->Class == genie::Unit::Petard) {
        unit->kill();
        return IAction::UpdateResult::Completed;
    }
```

This goes right before the final `return IAction::UpdateResult::Updated;` at line 203. Petards deal melee damage (no projectile), so this triggers after the direct-damage block at lines 187-197.

**Commit:** `feat: petards self-destruct after attacking`

---

## Task 5: Trebuchet attack-move integration (3 min)

Currently trebuchet pack/unpack works via the ActionPanel button. But a packed trebuchet (ID 331) right-clicked on an enemy should auto-unpack, then attack. An unpacked trebuchet (ID 42) told to move should auto-pack first.

**File:** `/home/dima/Projects/freeaoe/src/actions/ActionAttack.cpp`

In the constructor `ActionAttack::ActionAttack(attacker, task)`, at the top (after line 42), add auto-unpack logic:

```cpp
    // Packed trebuchet: auto-unpack before attacking
    if (attacker->data()->ID == 331) { // 331 = packed trebuchet
        Task transformTask;
        transformTask.target = attacker;
        attacker->actions.prependAction(
            std::make_shared<ActionTransform>(attacker, transformTask, 42, 11100));
    }
```

Add include at top of file:

```cpp
#include "ActionTransform.h"
```

**File:** `/home/dima/Projects/freeaoe/src/actions/ActionMove.cpp`

In the static factory `ActionMove::moveUnitTo(unit, destination)` (line ~610), add auto-pack for unpacked trebuchets:

```cpp
std::shared_ptr<ActionMove> ActionMove::moveUnitTo(const Unit::Ptr &unit, MapPos destination) noexcept
{
    static genie::Task defaultGenieMoveTask;
    defaultGenieMoveTask.ActionType = genie::ActionType::MoveTo;

    // Unpacked trebuchet: auto-pack before moving
    if (unit->data()->ID == 42) { // 42 = unpacked trebuchet
        Task transformTask;
        transformTask.target = unit;
        unit->actions.prependAction(
            std::make_shared<ActionTransform>(unit, transformTask, 331, 11100));
    }

    return moveUnitTo(unit, destination, Task(&defaultGenieMoveTask, -1));
}
```

Add include at top of ActionMove.cpp:

```cpp
#include "ActionTransform.h"
```

**Commit:** `feat: trebuchet auto-packs for move, auto-unpacks for attack`

---

## Task 6: Transport ship -- garrison onto non-building units (5 min)

Transport ships (genie class `TransportBoat`) can garrison units like buildings. Currently `ActionGarrison` only accepts `Building::Ptr`. We need to allow garrisoning into any unit with `GarrisonCapacity > 0` (transports, rams).

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Unit.h`

Add garrison storage to `Unit` (currently only `Building` has it, but transports are not buildings):

```cpp
    // Garrison support for non-building units (transport ships, rams)
    std::vector<std::weak_ptr<Unit>> garrisonedUnits;
```

Add after `std::weak_ptr<Building> garrisonedIn;` (line 148). Note: `Building` also has `garrisonedUnits` -- that is fine, `Building` inherits from `Unit` and will shadow this member. Actually, to avoid duplication, we should move `garrisonedUnits` from `Building` to `Unit`.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.h`

Remove `std::vector<std::weak_ptr<Unit>> garrisonedUnits;` from Building (line 28) -- it is now inherited from Unit.

**File:** `/home/dima/Projects/freeaoe/src/actions/ActionGarrison.h`

Change target from `Building::Ptr` to `Unit::Ptr`:

```cpp
private:
    std::weak_ptr<Unit> m_target;
```

**File:** `/home/dima/Projects/freeaoe/src/actions/ActionGarrison.cpp`

Rewrite to accept any unit with garrison capacity:

```cpp
#include "ActionGarrison.h"
#include "ActionMove.h"

#include "mechanics/Building.h"
#include "mechanics/Unit.h"

#include "global/EventManager.h"

#include <genie/dat/Unit.h>

ActionGarrison::ActionGarrison(const std::shared_ptr<Unit> &unit, const Task &task) :
    IAction(Type::Garrison, unit, task),
    m_target(task.target)
{
}

IAction::UpdateResult ActionGarrison::update(Time /*time*/)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) {
        WARN << "impossible, lost own unit";
        return UpdateResult::Failed;
    }
    Unit::Ptr target = m_target.lock();
    if (!target) {
        WARN << "garrison target lost";
        return UpdateResult::Failed;
    }

    if (target->data()->GarrisonCapacity <= 0) {
        WARN << "Target has no garrison capacity";
        return UpdateResult::Failed;
    }

    if (unit->distanceTo(target) > 1.) {
        DBG << "Out of range, moving closer";
        unit->actions.prependAction(ActionMove::moveUnitTo(unit, target));
        return UpdateResult::Updated;
    }

    if (static_cast<int>(target->garrisonedUnits.size()) >= target->data()->GarrisonCapacity) {
        WARN << "Target full, can't garrison";
        return UpdateResult::Failed;
    }

    target->garrisonedUnits.push_back(unit);
    unit->garrisonedIn = Building::fromUnit(target); // may be null for non-buildings, that is OK

    EventManager::unitGarrisoned(unit.get(), target.get());

    return UpdateResult::Completed;
}
```

**Note:** `unit->garrisonedIn` is typed `weak_ptr<Building>`. For transport ships (not buildings), it will remain empty. We should also add a `weak_ptr<Unit> garrisonedInUnit` to Unit for the general case. However, to keep changes minimal, we can leave `garrisonedIn` as-is and just hide the unit visually when garrisoned (the unit's position stays the same -- it gets hidden by the renderer when garrisoned).

Actually, let's add a simpler approach -- change `garrisonedIn` to `weak_ptr<Unit>`:

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Unit.h`

Change line 148:

```cpp
    std::weak_ptr<Unit> garrisonedIn;  // was weak_ptr<Building>
```

Then fix all `garrisonedIn` users to use `Unit::Ptr` instead of `Building::Ptr`. Grep for `garrisonedIn` to find all references and update.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.cpp`

In `ungarrison()`, change:

```cpp
    unit->garrisonedIn.reset();
```

This already works since `Unit::Ptr` has `reset()`.

Search for all other `garrisonedIn` usages and update cast types as needed.

**Commit:** `feat: transport ships and rams can garrison units`

---

## Task 7: Ungarrison from non-building units (3 min)

Add `ungarrison`/`ungarrisonAll` to `Unit` (move from Building, or duplicate).

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Unit.h`

Add public methods:

```cpp
    bool ungarrisonUnit(const std::shared_ptr<Unit> &unit);
    void ungarrisonAllUnits();
```

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Unit.cpp` (or a new section in the existing file)

```cpp
bool Unit::ungarrisonUnit(const std::shared_ptr<Unit> &unit)
{
    auto it = garrisonedUnits.begin();
    for (; it != garrisonedUnits.end(); ++it) {
        Unit::Ptr garrisoned = it->lock();
        if (!garrisoned) {
            it = garrisonedUnits.erase(it);
            continue;
        }
        if (garrisoned == unit) {
            garrisonedUnits.erase(it);
            unit->garrisonedIn.reset();
            // Place unit near transport's current position
            MapPos exitPos = position();
            exitPos.x += 24;
            exitPos.y += 24;
            unit->setPosition(exitPos);
            return true;
        }
    }
    return false;
}

void Unit::ungarrisonAllUnits()
{
    float offset = 0;
    for (auto &weak : garrisonedUnits) {
        Unit::Ptr unit = weak.lock();
        if (!unit) continue;
        unit->garrisonedIn.reset();
        MapPos exitPos = position();
        exitPos.x += 20 + offset;
        exitPos.y += 20 + offset;
        unit->setPosition(exitPos);
        offset += 10;
    }
    garrisonedUnits.clear();
}
```

**Commit:** `feat: ungarrison support for transport ships and rams`

---

## Task 8: Hide garrisoned units from rendering (2 min)

Units garrisoned inside a transport/ram should not be rendered. Check if the renderer already handles this.

**File:** `/home/dima/Projects/freeaoe/src/render/UnitsRenderer.cpp` (or wherever unit rendering happens)

Search for `garrisonedIn` in the render loop. If units are already hidden when `garrisonedIn` is set, no change needed. If not, add a check:

```cpp
    if (!unit->garrisonedIn.expired()) {
        continue; // Skip rendering garrisoned units
    }
```

This goes at the top of the per-unit render loop.

**Commit:** `fix: hide garrisoned units from rendering`

---

## Task 9: Formation movement -- orient perpendicular to move direction (4 min)

The current formation code applies offsets in absolute X/Y coordinates. In AoE2, formations orient perpendicular to the movement direction. Update the formation offset calculation.

**File:** `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp`

In `onRightClick()`, around line 736, replace the formation offset block. Calculate the angle from the group center to the destination, then rotate offsets:

```cpp
    // Calculate formation offsets
    int unitCount = 0;
    MapPos groupCenter;
    for (const Unit::Ptr &u : m_selectedUnits) {
        if (u->playerId() == humanPlayer->playerId) {
            groupCenter += u->position();
            unitCount++;
        }
    }
    if (unitCount > 0) {
        groupCenter /= unitCount;
    }

    float moveAngle = groupCenter.angleTo(mapPos);
    float perpAngle = moveAngle + M_PI_2; // perpendicular to move direction
```

Then in the per-unit loop, after computing raw `offsetX`/`offsetY`, rotate them:

```cpp
        if (unitCount > 1) {
            float rawOffsetX = 0, rawOffsetY = 0;
            switch (Unit::s_formation) {
            case Unit::Formation::Line: {
                int cols = std::min(unitCount, 8);
                int col = unitIndex % cols;
                int row = unitIndex / cols;
                rawOffsetX = (col - cols / 2.f) * spacing;
                rawOffsetY = row * spacing;
                break;
            }
            case Unit::Formation::Box: {
                int side = static_cast<int>(std::ceil(std::sqrt(unitCount)));
                int col = unitIndex % side;
                int row = unitIndex / side;
                rawOffsetX = (col - side / 2.f) * spacing;
                rawOffsetY = (row - side / 2.f) * spacing;
                break;
            }
            case Unit::Formation::Flank: {
                int col = unitIndex % 2;
                int row = unitIndex / 2;
                rawOffsetX = (col - 0.5f) * spacing * 2;
                rawOffsetY = row * spacing;
                break;
            }
            case Unit::Formation::SpreadOut: {
                int side = static_cast<int>(std::ceil(std::sqrt(unitCount)));
                int col = unitIndex % side;
                int row = unitIndex / side;
                rawOffsetX = (col - side / 2.f) * spacing * 2;
                rawOffsetY = (row - side / 2.f) * spacing * 2;
                break;
            }
            }
            // Rotate offsets to be relative to move direction
            float cosA = std::cos(moveAngle);
            float sinA = std::sin(moveAngle);
            float rotatedX = rawOffsetX * cosA - rawOffsetY * sinA;
            float rotatedY = rawOffsetX * sinA + rawOffsetY * cosA;
            formationTarget.x += rotatedX;
            formationTarget.y += rotatedY;
            formationTarget = formationTarget.clamped(m_map->pixelSize());
        }
```

**Commit:** `feat: formations orient perpendicular to movement direction`

---

## Task 10: Build verification and smoke test (2 min)

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Fix any compilation errors. Then run the game:
1. Select multiple units, right-click to move -- verify formation shapes
2. Double-click a unit -- verify all same-type units get selected
3. Ctrl+click units -- verify add/remove from selection
4. Create a petard, attack a building -- verify petard dies after attack
5. Create a trebuchet, right-click enemy -- verify it unpacks then attacks
6. Move an unpacked trebuchet -- verify it packs first

**Commit:** (only if fixes needed) `fix: compilation fixes for combat features`

---

## Summary of files modified

| File | Changes |
|------|---------|
| `/home/dima/Projects/freeaoe/src/render/EventTypes.h` | Add `clicks` field to MouseButtonEvent |
| `/home/dima/Projects/freeaoe/src/render/SdlRenderTarget.cpp` | Pass SDL2 click count through |
| `/home/dima/Projects/freeaoe/src/Engine.cpp` | Double-click select-by-type, Ctrl+click toggle |
| `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.h` | Add `toggleUnitInSelection()` |
| `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp` | Implement toggle, rotate formation offsets |
| `/home/dima/Projects/freeaoe/src/mechanics/Unit.h` | Move `garrisonedUnits` here, change `garrisonedIn` type, add ungarrison methods |
| `/home/dima/Projects/freeaoe/src/mechanics/Unit.cpp` | Implement `ungarrisonUnit()`, `ungarrisonAllUnits()` |
| `/home/dima/Projects/freeaoe/src/mechanics/Building.h` | Remove `garrisonedUnits` (now in Unit) |
| `/home/dima/Projects/freeaoe/src/actions/ActionAttack.cpp` | Petard self-destruct, trebuchet auto-unpack |
| `/home/dima/Projects/freeaoe/src/actions/ActionMove.cpp` | Trebuchet auto-pack before move |
| `/home/dima/Projects/freeaoe/src/actions/ActionGarrison.cpp` | Accept any unit as garrison target |
| `/home/dima/Projects/freeaoe/src/actions/ActionGarrison.h` | Change target type to `Unit::Ptr` |
| `/home/dima/Projects/freeaoe/src/render/UnitsRenderer.cpp` | Hide garrisoned units |

## What is NOT included (already done)

- **Formation enum and buttons** -- already exist and work (Unit.h:138-144, ActionPanel sets s_formation)
- **Formation movement offsets** -- already implemented (UnitManager.cpp:736-809), just needs rotation
- **Trebuchet pack/unpack button** -- already works via ActionPanel Command::Pack/Unpack (ActionPanel.cpp:891-916)
- **ActionTransform** -- already exists and swaps unit data (ActionTransform.cpp)
