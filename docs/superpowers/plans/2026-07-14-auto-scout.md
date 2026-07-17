# Auto-Scout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a DE-style auto-scout button that makes scout units automatically explore unexplored areas of the map.
**Architecture:** A new ActionAutoScout action queries the player's VisibilityMap for the nearest cluster of unexplored tiles, then moves the scout there in a spiral search pattern. The ActionPanel shows a compass button (reusing the Patrol icon, index 6) for scout-class units. Any manual right-click command cancels the auto-scout.
**Tech Stack:** C++, SDL2
---

## Task 1: Create ActionAutoScout Header

**File:** `src/actions/ActionAutoScout.h` (new file)

```cpp
#pragma once

#include "actions/IAction.h"
#include "core/Types.h"

#include <memory>

struct VisibilityMap;

class ActionAutoScout : public IAction
{
public:
    ActionAutoScout(const std::shared_ptr<Unit> &unit, const Task &task);

    UpdateResult update(Time time) override;
    UnitState unitState() const override { return UnitState::Moving; }
    genie::ActionType taskType() const override { return genie::ActionType::Explore; }

private:
    /// Find the nearest unexplored tile cluster and return its map position
    MapPos findUnexploredTarget(const std::shared_ptr<Unit> &unit);

    MapPos m_currentTarget;
    bool m_hasTarget = false;
    Time m_lastRetarget = 0;
    int m_spiralStep = 0; // tracks progress in spiral search
};
```

## Task 2: Create ActionAutoScout Implementation

**File:** `src/actions/ActionAutoScout.cpp` (new file)

```cpp
#include "ActionAutoScout.h"

#include "ActionMove.h"
#include "core/Logger.h"
#include "core/Constants.h"
#include "mechanics/Unit.h"
#include "mechanics/Player.h"
#include "mechanics/UnitManager.h"
#include "mechanics/Map.h"

#include <cmath>

ActionAutoScout::ActionAutoScout(const std::shared_ptr<Unit> &unit, const Task &task)
    : IAction(Type::Patrol, unit, task) // reuse Patrol type for animation
{
    DBG << unit->debugName << "starting auto-scout";
}

IAction::UpdateResult ActionAutoScout::update(Time time)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) {
        return UpdateResult::Completed;
    }

    // Only retarget every 2 seconds or when we reach destination
    if (m_hasTarget && time - m_lastRetarget < 2000) {
        // Check if we've reached the target (within 2 tiles)
        float dist = unit->position().distance(m_currentTarget);
        if (dist > Constants::TILE_SIZE * 2) {
            return UpdateResult::NotUpdated;
        }
    }

    // Find new unexplored target
    MapPos target = findUnexploredTarget(unit);
    if (target.x < 0) {
        DBG << unit->debugName << "auto-scout: no unexplored tiles left";
        return UpdateResult::Completed;
    }

    m_currentTarget = target;
    m_hasTarget = true;
    m_lastRetarget = time;

    // Queue a move to the target
    unit->actions.clearActionQueue();
    unit->actions.queueAction(ActionMove::moveUnitTo(unit, target, m_task));
    // Re-queue ourselves after the move
    unit->actions.queueAction(std::make_shared<ActionAutoScout>(unit, m_task));

    return UpdateResult::Completed;
}

MapPos ActionAutoScout::findUnexploredTarget(const std::shared_ptr<Unit> &unit)
{
    Player::Ptr player = unit->player().lock();
    if (!player || !player->visibility) {
        return MapPos(-1, -1);
    }

    const auto &visMap = player->visibility;
    const MapPtr &map = unit->unitManager().map();
    if (!map) {
        return MapPos(-1, -1);
    }

    const int cols = map->columnCount();
    const int rows = map->rowCount();

    // Current tile position of the scout
    const int startCol = static_cast<int>(unit->position().x / Constants::TILE_SIZE);
    const int startRow = static_cast<int>(unit->position().y / Constants::TILE_SIZE);

    // Spiral search outward from scout position
    // Search in expanding rings for the first cluster of unexplored tiles
    MapPos bestTarget(-1, -1);
    float bestScore = -1.f;

    const int maxRadius = std::max(cols, rows);

    for (int radius = 3; radius < maxRadius; radius += 4) {
        bool foundInRing = false;

        for (int angle = 0; angle < 8; angle++) {
            // 8 directions around the ring
            int col = startCol + static_cast<int>(radius * std::cos(angle * M_PI / 4.0));
            int row = startRow + static_cast<int>(radius * std::sin(angle * M_PI / 4.0));

            // Clamp to map bounds
            col = std::clamp(col, 1, cols - 2);
            row = std::clamp(row, 1, rows - 2);

            // Count unexplored tiles in a 5x5 area around this point
            int unexploredCount = 0;
            for (int dc = -2; dc <= 2; dc++) {
                for (int dr = -2; dr <= 2; dr++) {
                    int c = col + dc;
                    int r = row + dr;
                    if (c < 0 || c >= cols || r < 0 || r >= rows) continue;
                    if (visMap->visibilityAt(c, r) == VisibilityMap::Unexplored) {
                        unexploredCount++;
                    }
                }
            }

            if (unexploredCount > 5) {
                // Score: more unexplored = better, closer = better
                float dist = std::sqrt(float((col - startCol) * (col - startCol) +
                                             (row - startRow) * (row - startRow)));
                float score = unexploredCount / (dist + 1.f);

                if (score > bestScore) {
                    bestScore = score;
                    bestTarget = MapPos(col * Constants::TILE_SIZE + Constants::TILE_SIZE / 2,
                                        row * Constants::TILE_SIZE + Constants::TILE_SIZE / 2);
                    foundInRing = true;
                }
            }
        }

        if (foundInRing) {
            break; // Found a good target in this ring, don't search further
        }
    }

    return bestTarget;
}
```

## Task 3: Add ActionAutoScout to CMakeLists.txt

**File:** `CMakeLists.txt`, in the `ACTIONS_SRC` set (after `src/actions/ActionGarrison.h` at line 337)

Add:
```
    src/actions/ActionAutoScout.cpp
    src/actions/ActionAutoScout.h
```

## Task 4: Add AutoScout Command to ActionPanel

**File:** `src/ui/ActionPanel.h`

In the `Command` enum (line 166-243), add `AutoScout` before `Undefined`:

```cpp
        // Special ones not actually in the SLP
        PreviousPage,
        Garrison,
        AutoScout,

        Undefined
```

## Task 5: Show Auto-Scout Button for Scout Units

**File:** `src/ui/ActionPanel.cpp`, in `addMilitaryButtons()` (line 584-673)

After the stance buttons block (after line 672, before the closing `}`), add:

```cpp
    // Auto-scout button for scout-class units
    if (unit->data()->Class == genie::Unit::Scout ||
        unit->data()->ID == 448 ||  // Scout Cavalry
        unit->data()->ID == 546 ||  // Eagle Scout / Eagle Warrior
        unit->data()->ID == 751) {  // Eagle Scout (alt)
        InterfaceButton autoScoutBtn;
        autoScoutBtn.type = InterfaceButton::Other;
        autoScoutBtn.action = Command::Patrol; // Reuse patrol icon (compass)
        autoScoutBtn.index = 10; // Row 3, column 1
        autoScoutBtn.interfacePage = 0;
        currentButtons.push_back(autoScoutBtn);
    }
```

Wait -- we need the Command to be `AutoScout` not `Patrol` for the handler, but we want to display the Patrol icon. The button type is `Other` and the `action` field serves double duty as both the icon index and the command. We need to map `AutoScout` to an icon.

Better approach: use `Command::Patrol` as the icon (index 6 in the SLP), but store the actual command separately. Since the system currently uses `action` for both icon and command identity, let's add `AutoScout` to the `helpTextIds` and handle it in `handleButtonClick`.

Actually, looking at the code more carefully, the icon is drawn via `m_commandIcons[button.action]` (line 200). Since `AutoScout` won't be in the SLP, we need to reuse an existing icon. The simplest approach: use the `SignalFlare` icon (index 34) which looks like a compass/flare.

**Revised approach in `addMilitaryButtons()`:**

```cpp
    // Auto-scout button for scout-class units
    if (unit->data()->Class == genie::Unit::Scout ||
        unit->data()->ID == 448 ||  // Scout Cavalry
        unit->data()->ID == 546) {  // Eagle Warrior
        InterfaceButton autoScoutBtn;
        autoScoutBtn.type = InterfaceButton::Other;
        autoScoutBtn.action = Command::SignalFlare; // Reuse signal flare icon
        autoScoutBtn.index = 10;
        autoScoutBtn.interfacePage = 0;
        currentButtons.push_back(autoScoutBtn);
    }
```

Then handle `Command::SignalFlare` as auto-scout when the selected unit is a scout. But this is hacky. Cleaner: just add `AutoScout` to the command icons map manually.

**Final approach:** In `loadButtons()` (line 324), after the HAX section (after line 368), add:

```cpp
    // Auto-scout uses the patrol icon
    m_commandIcons[Command::AutoScout] = m_commandIcons[Command::Patrol];
```

Then in `addMilitaryButtons()`:

```cpp
    if (unit->data()->Class == genie::Unit::Scout ||
        unit->data()->ID == 448 || unit->data()->ID == 546) {
        InterfaceButton autoScoutBtn;
        autoScoutBtn.type = InterfaceButton::Other;
        autoScoutBtn.action = Command::AutoScout;
        autoScoutBtn.index = 10;
        autoScoutBtn.interfacePage = 0;
        currentButtons.push_back(autoScoutBtn);
    }
```

## Task 6: Handle AutoScout Button Click

**File:** `src/ui/ActionPanel.cpp`, in `handleButtonClick()` (the `switch(button.action)` block)

Add before `default:` (around line 960):

```cpp
        case Command::AutoScout: {
            for (const Unit::Ptr &unit : m_selectedUnits) {
                if (!unit) continue;
                unit->actions.clearActionQueue();
                Task scoutTask;
                scoutTask.target = unit; // self
                unit->actions.setCurrentAction(
                    std::make_shared<ActionAutoScout>(unit, scoutTask));
            }
            break;
        }
```

## Task 7: Add Include for ActionAutoScout

**File:** `src/ui/ActionPanel.cpp`, add after the existing action includes (after line 5):

```cpp
#include "actions/ActionAutoScout.h"
```

## Task 8: Cancel Auto-Scout on Manual Command

The auto-scout action re-queues itself in `update()`. When the player issues any manual command (right-click move, attack, etc.), `UnitActionHandler::clearActionQueue()` is called, which removes the queued ActionAutoScout. The current action (ActionMove) will complete, but since the ActionAutoScout was removed from the queue, the scout won't re-enter auto-scout mode. This works automatically -- no additional code needed.

## Task 9: Add to LogPrinter (Optional)

**File:** `src/ui/ActionPanel.h`

In the `operator<<` for `ActionPanel::Command` (line 350-431), add before `default:`:

```cpp
    case ActionPanel::Command::AutoScout: os << "AutoScout"; break;
```

## Summary of Changes

| File | Change |
|------|--------|
| `src/actions/ActionAutoScout.h` | New file: ActionAutoScout class with spiral search |
| `src/actions/ActionAutoScout.cpp` | New file: Implementation with VisibilityMap queries |
| `CMakeLists.txt` (ACTIONS_SRC) | Add ActionAutoScout.cpp/.h |
| `src/ui/ActionPanel.h` | Add `AutoScout` to Command enum, add to LogPrinter |
| `src/ui/ActionPanel.cpp` (`loadButtons`) | Map AutoScout icon to Patrol icon |
| `src/ui/ActionPanel.cpp` (`addMilitaryButtons`) | Show auto-scout button for scout units |
| `src/ui/ActionPanel.cpp` (`handleButtonClick`) | Handle AutoScout command |
| `src/ui/ActionPanel.cpp` (includes) | Add `#include "actions/ActionAutoScout.h"` |
