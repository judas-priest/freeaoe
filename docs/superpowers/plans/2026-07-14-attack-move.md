# Plan: ActionAttackMove

**Goal:** Attack-Move command — unit moves toward a destination and auto-attacks any enemy in LOS along the way, then resumes moving. One-shot (not repeating like Patrol).

**Status:** Not started

---

## Overview

Attack-Move is a standard RTS command. The unit moves to the target position. On each update tick, it scans for the closest enemy within its line-of-sight range. If found, it attacks, then resumes moving once the target is dead or out of range. The action completes when the destination is reached.

Pattern: mirrors `ActionPatrol` for the move loop, mirrors `ActionAttack` for the combat check.

---

## New files

### `src/actions/ActionAttackMove.h`

```cpp
#pragma once

#include "IAction.h"
#include "core/Types.h"
#include "mechanics/Unit.h"

class ActionAttackMove : public IAction
{
public:
    ActionAttackMove(const Unit::Ptr &unit, const MapPos &destination);

    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Attack; }
    UnitState unitState() const override { return UnitState::Moving; }

private:
    Unit::Ptr findEnemyInRange(const Unit::Ptr &unit) const;

    MapPos m_destination;
    bool m_moveQueued = false;
    bool m_attacking = false;
};
```

### `src/actions/ActionAttackMove.cpp`

```cpp
#include "ActionAttackMove.h"
#include "ActionAttack.h"
#include "ActionMove.h"

#include "core/Logger.h"
#include "mechanics/UnitManager.h"
#include "mechanics/Player.h"

#include <genie/dat/Unit.h>

ActionAttackMove::ActionAttackMove(const Unit::Ptr &unit, const MapPos &destination)
    : IAction(Type::Move, unit, Task())
    , m_destination(destination)
{
}

ActionAttackMove::UpdateResult ActionAttackMove::update(Time /*time*/)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) return UpdateResult::Completed;

    // If we were attacking, check if the sub-action is done
    if (m_attacking) {
        // The attack action was prepended; once it completes, we resume
        // (ActionAttack removes itself when done — we just re-evaluate next tick)
        m_attacking = false;
        m_moveQueued = false; // force re-queue of move
    }

    // Scan for nearby enemies
    Unit::Ptr enemy = findEnemyInRange(unit);
    if (enemy) {
        // Cancel pending move, attack the enemy
        m_moveQueued = false;
        m_attacking = true;

        Task attackTask;
        attackTask.target = enemy;
        unit->actions.prependAction(std::make_shared<ActionAttack>(unit, attackTask));
        return UpdateResult::Updated;
    }

    // No enemy — move toward destination
    float dist = unit->position().distance(m_destination);
    if (dist < 8.f) {
        return UpdateResult::Completed;
    }

    if (!m_moveQueued) {
        auto move = ActionMove::moveUnitTo(unit, m_destination);
        if (move) {
            unit->actions.queueAction(move);
            m_moveQueued = true;
        }
    }

    return UpdateResult::Updated;
}

Unit::Ptr ActionAttackMove::findEnemyInRange(const Unit::Ptr &unit) const
{
    const float losRange = unit->data()->LineOfSight * 48.f; // tiles → pixels
    const int myPlayerId = unit->playerId();

    Unit::Ptr closest;
    float closestDist = losRange;

    for (const Unit::Ptr &other : unit->unitManager().units()) {
        if (!other || !other->isAlive()) continue;
        if (other->playerId() == myPlayerId) continue;
        if (other->playerId() == 0) continue; // skip Gaia

        const float dist = unit->position().distance(other->position());
        if (dist < closestDist) {
            closestDist = dist;
            closest = other;
        }
    }

    return closest;
}
```

---

## Wire into `UnitManager`

### `src/mechanics/UnitManager.h`

Add to the `State` enum (after `SelectingHealTarget`):

```cpp
SelectingAttackMoveTarget,
```

Add declaration:

```cpp
void selectAttackMoveTarget();
```

### `src/mechanics/UnitManager.cpp`

In `onRightClick` where `SelectingAttackTarget` is handled, add parallel case:

```cpp
case State::SelectingAttackMoveTarget: {
    for (const Unit::Ptr &unit : m_selectedUnits) {
        unit->actions.assignAction(std::make_shared<ActionAttackMove>(unit, mapPos));
    }
    m_state = State::Default;
    break;
}
```

Add the method:

```cpp
void UnitManager::selectAttackMoveTarget()
{
    m_state = State::SelectingAttackMoveTarget;
}
```

---

## Wire into `ActionPanel`

### `src/ui/ActionPanel.h`

The `Command` enum already has `AttackGround = 60`. We repurpose no existing slot — we add to the `Other` case in `handleButtonClick`. But first we need a button. The standard AoE2 button for attack-move is icon 121 in the command panel (InterfaceButton at position index 10, page 0, for military units). For now hook it onto hotkey 'A' only and add the button alongside `Patrol`.

In `handleButtonClick`, inside the `Other` switch:

```cpp
case Command::AttackGround:
    // existing: m_unitManager->selectAttackTarget();
    // Attack-move is triggered by hotkey 'A' in handleKeyEvent, not this button
    m_unitManager->selectAttackTarget();
    break;
```

**Add the hotkey** in `Engine.cpp` `handleKeyEvent`:

Find where `SDLK_p` triggers patrol, add after it:

```cpp
case SDLK_a:
    if (!m_unitManager->selected().isEmpty()) {
        m_unitManager->selectAttackMoveTarget();
    }
    return true;
```

---

## CMakeLists

In whichever `CMakeLists.txt` lists action sources, add:

```cmake
src/actions/ActionAttackMove.cpp
```

Search for the existing pattern:

```
src/actions/ActionPatrol.cpp
```

and add the new file directly below it.

---

## Testing

1. Select a knight, press `A`, click a position beyond enemy units.
2. Knight should stop and fight each enemy it encounters along the path, then resume moving.
3. If no enemies, knight walks straight to destination.
4. Patrol (`P`) should be unaffected.
