# Trebuchet Transform Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the instant trebuchet pack/unpack swap with a timed ActionTransform that disables movement and counts down before transforming.
**Architecture:** A new ActionTransform action class handles the timed transformation. The ActionPanel pack/unpack handler queues ActionTransform instead of doing an instant setUnitData swap. The transform duration comes from the unit's dat file (RearAttackModifier field, defaulting to 4000ms). ActionTransform freezes the unit in place during the countdown.
**Tech Stack:** C++, SDL2
---

## Task 1: Create ActionTransform Header

**File:** `src/actions/ActionTransform.h` (new file)

```cpp
#pragma once

#include "actions/IAction.h"
#include "core/Types.h"

#include <memory>

class ActionTransform : public IAction
{
public:
    /// @param unit The unit transforming
    /// @param task The task context
    /// @param targetUnitId The genie unit ID to transform into
    /// @param durationMs How long the transform takes in milliseconds
    ActionTransform(const std::shared_ptr<Unit> &unit, const Task &task,
                    int targetUnitId, Time durationMs);

    UpdateResult update(Time time) override;
    UnitState unitState() const override { return UnitState::Working; }
    genie::ActionType taskType() const override { return genie::ActionType::Pack; }

private:
    int m_targetUnitId;
    Time m_durationMs;
    Time m_startTime = 0;
};
```

## Task 2: Create ActionTransform Implementation

**File:** `src/actions/ActionTransform.cpp` (new file)

```cpp
#include "ActionTransform.h"

#include "core/Logger.h"
#include "mechanics/Unit.h"
#include "mechanics/Player.h"
#include "mechanics/Civilization.h"

#include <genie/dat/Unit.h>

ActionTransform::ActionTransform(const std::shared_ptr<Unit> &unit, const Task &task,
                                 int targetUnitId, Time durationMs)
    : IAction(Type::None, unit, task),
      m_targetUnitId(targetUnitId),
      m_durationMs(durationMs)
{
    DBG << unit->debugName << "starting transform to unit" << targetUnitId
        << "duration" << durationMs << "ms";
}

IAction::UpdateResult ActionTransform::update(Time time)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) {
        return UpdateResult::Completed;
    }

    // Record start time on first update
    if (m_startTime == 0) {
        m_startTime = time;
        return UpdateResult::Updated;
    }

    // Check if duration has elapsed
    if (time - m_startTime < m_durationMs) {
        return UpdateResult::NotUpdated;
    }

    // Transform complete -- swap unit data
    Player::Ptr owner = unit->player().lock();
    if (!owner) {
        WARN << "No player for transforming unit";
        return UpdateResult::Completed;
    }

    const genie::Unit &newData = owner->civilization.unitData(m_targetUnitId);
    if (newData.ID == -1) {
        WARN << "Invalid transform target unit ID" << m_targetUnitId;
        return UpdateResult::Completed;
    }

    DBG << unit->debugName << "transform complete, becoming unit" << m_targetUnitId;
    unit->setUnitData(newData);

    return UpdateResult::Completed;
}
```

## Task 3: Add ActionTransform to CMakeLists.txt

**File:** `CMakeLists.txt`, in the `ACTIONS_SRC` set (after line 336, `src/actions/ActionGarrison.h`)

Add:
```
    src/actions/ActionTransform.cpp
    src/actions/ActionTransform.h
```

The full ACTIONS_SRC block should look like:
```cmake
set(ACTIONS_SRC
    src/actions/IAction.cpp
    src/actions/IAction.h
    src/actions/ActionAttack.cpp
    src/actions/ActionAttack.h
    src/actions/ActionBuild.cpp
    src/actions/ActionBuild.h
    src/actions/ActionGather.cpp
    src/actions/ActionGather.h
    src/actions/ActionMove.cpp
    src/actions/ActionMove.h
    src/actions/ActionFly.cpp
    src/actions/ActionFly.h
    src/actions/ActionGarrison.cpp
    src/actions/ActionAttackMove.cpp
    src/actions/ActionPatrol.cpp
    src/actions/ActionGuard.cpp
    src/actions/ActionFollow.cpp
    src/actions/ActionRepair.cpp
    src/actions/ActionConvert.cpp
    src/actions/ActionHeal.cpp
    src/actions/ActionTrade.cpp
    src/actions/ActionPickupRelic.cpp
    src/actions/ActionGarrison.h
    src/actions/ActionTransform.cpp
    src/actions/ActionTransform.h
    )
```

## Task 4: Replace Instant Swap in ActionPanel

**File:** `src/ui/ActionPanel.cpp`, lines 854-875 (the `case Command::Pack: case Command::Unpack:` block)

Replace:
```cpp
        case Command::Pack:
        case Command::Unpack: {
            Player::Ptr human = m_humanPlayer.lock();
            if (!human) break;
            for (const Unit::Ptr &unit : m_selectedUnits) {
                if (!unit) continue;
                // Trebuchet packed (331) <-> unpacked (42)
                // Bombard Cannon packed (36) has no unpack in AoE2, skip
                int currentId = unit->data()->ID;
                int swapId = -1;
                if (currentId == 331) swapId = 42;
                else if (currentId == 42) swapId = 331;
                if (swapId >= 0) {
                    const genie::Unit &newData = human->civilization.unitData(swapId);
                    if (newData.ID != -1) {
                        unit->setUnitData(newData);
                    }
                }
            }
            m_buttonsDirty = true;
            break;
        }
```

With:
```cpp
        case Command::Pack:
        case Command::Unpack: {
            Player::Ptr human = m_humanPlayer.lock();
            if (!human) break;
            for (const Unit::Ptr &unit : m_selectedUnits) {
                if (!unit) continue;
                // Trebuchet packed (331) <-> unpacked (42)
                int currentId = unit->data()->ID;
                int swapId = -1;
                Time transformTime = 4000; // default 4 seconds
                if (currentId == 331) {
                    swapId = 42;    // packed -> unpacked
                    transformTime = 4000;
                } else if (currentId == 42) {
                    swapId = 331;   // unpacked -> packed
                    transformTime = 4000;
                }
                if (swapId >= 0) {
                    unit->actions.clearActionQueue();
                    Task transformTask;
                    transformTask.target = unit; // self-target
                    unit->actions.setCurrentAction(
                        std::make_shared<ActionTransform>(unit, transformTask, swapId, transformTime));
                }
            }
            m_buttonsDirty = true;
            break;
        }
```

## Task 5: Add Include for ActionTransform in ActionPanel

**File:** `src/ui/ActionPanel.cpp`, add after line 6 (`#include "actions/ActionGarrison.h"`):

```cpp
#include "actions/ActionTransform.h"
```

## Task 6: Add Transform to IAction::Type Enum (Optional)

**File:** `src/actions/IAction.h`, in the `Type` enum (line 63-81)

Add `Transform` after `PickupRelic`:
```cpp
    enum class Type {
        None,
        Move,
        PlaceOnMap,
        Build,
        Gather,
        DropOff,
        Attack,
        Fly,
        Garrison,
        Patrol,
        Guard,
        Follow,
        Repair,
        Convert,
        Heal,
        Trade,
        PickupRelic,
        Transform
    };
```

Then update `ActionTransform.cpp` constructor to use `Type::Transform`:
```cpp
    : IAction(Type::Transform, unit, task),
```

And add the case to the LogPrinter operator<< in IAction.h (after line 163):
```cpp
    case IAction::Type::Transform: os << "Transform"; break;
```

## Summary of Changes

| File | Change |
|------|--------|
| `src/actions/ActionTransform.h` | New file: ActionTransform class declaration |
| `src/actions/ActionTransform.cpp` | New file: Timed transform with countdown |
| `CMakeLists.txt` (ACTIONS_SRC) | Add ActionTransform.cpp/.h |
| `src/ui/ActionPanel.cpp:854-875` | Replace instant swap with ActionTransform queue |
| `src/ui/ActionPanel.cpp` (includes) | Add `#include "actions/ActionTransform.h"` |
| `src/actions/IAction.h` | Add `Transform` to Type enum and LogPrinter |
