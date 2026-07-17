# Module 14: Patrol & Guard Combat Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Patrol auto-attack enemies encountered along the route, and Guard auto-attack enemies threatening the guarded unit. Both are core AoE2 commands that currently only do movement.

**Architecture:** Both actions already have the movement loop working. Add enemy scanning during movement. When an enemy is found, queue an attack action. After the enemy dies, resume the patrol/guard behavior.

**Tech Stack:** C++, ActionPatrol, ActionGuard, UnitActionHandler

**Verified APIs:**
- Unit accessed via IAction base class (no `m_unit` member — use `unit()` or equivalent accessor from IAction)
- `unit->actions.checkForAutoTargets()` returns `Task` (on UnitActionHandler, not Unit directly)
- `ActionPatrol` members: `m_startPos`, `m_destPos`, `m_returning`, `m_moveQueued`
- `ActionGuard` members: `m_guardTarget` (weak_ptr), `m_isFollowing`, `FOLLOW_DISTANCE = 48.f`
- `UnitManager::State::SelectingPatrolTarget` and `SelectingGuardTarget` both exist

---

### Task 1: Patrol auto-attacks enemies

**Files:**
- Modify: `src/actions/ActionPatrol.h` (add `m_isAttacking` member)
- Modify: `src/actions/ActionPatrol.cpp` (add enemy scanning to update)

**Context:** `ActionPatrol::update()` is ~20 lines. It toggles between start and destination via `ActionMove`. In AoE2, patrol behaves like attack-move — the unit attacks any enemy in range while moving, then resumes patrol.

- [ ] **Step 1: Read IAction base class to find how to access the unit**

Read `src/actions/IAction.h` to find the unit accessor. It is likely `m_unit` as a `weak_ptr<Unit>` or a `unit()` method. Use whatever the base class provides.

- [ ] **Step 2: Add m_isAttacking member to ActionPatrol.h**

```cpp
bool m_isAttacking = false;
```

- [ ] **Step 3: Modify ActionPatrol::update() to scan for enemies**

Read the current `ActionPatrol.cpp` fully first. Then modify `update()`:

```cpp
IAction::UpdateResult ActionPatrol::update(Time time)
{
    // Access unit through IAction base (adapt accessor name from Step 1)
    Unit::Ptr unit = /* IAction unit accessor */;
    if (!unit) {
        return Completed;
    }

    // Check for enemies — use the auto-target system
    if (!m_isAttacking) {
        Task autoTarget = unit->actions.checkForAutoTargets();
        if (autoTarget.data) {
            m_isAttacking = true;
            // Let the unit action handler assign the attack task
            unit->actions.setCurrentTask(autoTarget);
            return Updated;
        }
    }

    // If we were attacking and the attack finished, resume patrol
    if (m_isAttacking) {
        m_isAttacking = false;
        // Fall through to re-queue movement
    }

    // Original patrol logic
    if (m_moveQueued) {
        m_moveQueued = false;
        m_returning = !m_returning;
    }

    MapPos target = m_returning ? m_startPos : m_destPos;
    auto move = ActionMove::moveUnitTo(unit, target);
    if (move) {
        unit->actions.queueAction(move);
        m_moveQueued = true;
    }

    return Updated;
}
```

**Note:** The exact method to trigger an attack from a `Task` depends on how the action handler works. Read `UnitActionHandler.h/cpp` to find how `checkForAutoTargets()` result is normally consumed. It may be `assignTask()` or `setCurrentTask()` or similar. Adapt accordingly.

- [ ] **Step 4: Add required includes**

In `ActionPatrol.cpp`, add if needed:

```cpp
#include "ActionAttack.h"
#include "ActionMove.h"
```

- [ ] **Step 5: Build and test**

```bash
cd build && make -j$(nproc)
```

Test: select a unit, patrol past an enemy — should attack and resume patrol.

- [ ] **Step 6: Commit**

```bash
git add src/actions/ActionPatrol.cpp src/actions/ActionPatrol.h
git commit -m "feat: patrol auto-attacks enemies encountered along route"
```

---

### Task 2: Guard auto-attacks enemies threatening target

**Files:**
- Modify: `src/actions/ActionGuard.h` (add `m_isAttacking` member)
- Modify: `src/actions/ActionGuard.cpp` (add enemy defense logic)

**Context:** `ActionGuard::update()` is ~24 lines. It follows the target unit if distance > 48px. In AoE2, guard also attacks any enemy near the guarded unit. Strategy: scan for enemy units near the guard target's position.

- [ ] **Step 1: Add m_isAttacking member to ActionGuard.h**

```cpp
bool m_isAttacking = false;
```

- [ ] **Step 2: Modify ActionGuard::update() to defend the target**

Read the current `ActionGuard.cpp` fully first. Then modify:

```cpp
IAction::UpdateResult ActionGuard::update(Time time)
{
    Unit::Ptr unit = /* IAction unit accessor */;
    Unit::Ptr target = m_guardTarget.lock();
    if (!unit || !target) {
        return Completed;
    }

    // Check for enemies near the guarded unit — attack them
    if (!m_isAttacking) {
        // Use auto-target system — it already respects stances and LOS
        Task autoTarget = unit->actions.checkForAutoTargets();
        if (autoTarget.data) {
            m_isAttacking = true;
            unit->actions.setCurrentTask(autoTarget);
            return Updated;
        }
    }

    // If done attacking, resume guarding
    if (m_isAttacking) {
        m_isAttacking = false;
    }

    // Follow the guarded unit
    float dist = unit->position().distance(target->position());
    if (dist > FOLLOW_DISTANCE && !m_isFollowing) {
        m_isFollowing = true;
        auto move = ActionMove::moveUnitTo(unit, target->position());
        if (move) {
            unit->actions.queueAction(move);
        }
    } else if (dist <= FOLLOW_DISTANCE) {
        m_isFollowing = false;
    }

    return m_isFollowing ? Updated : NotUpdated;
}
```

- [ ] **Step 3: Build, test, commit**

```bash
cd build && make -j$(nproc)
git add src/actions/ActionGuard.cpp src/actions/ActionGuard.h
git commit -m "feat: guard auto-attacks enemies threatening guarded unit"
```

---

### Task 3: Add Patrol (P) and Guard (G) hotkeys

**Files:**
- Modify: `src/Engine.cpp` (handleKeyEvent, after the existing key cases)

**Context:** `handleKeyEvent()` is a switch on `event.key.code`. `UnitManager::State::SelectingPatrolTarget` and `SelectingGuardTarget` both exist (verified). `UnitManager` also has `selectPatrolTarget()` and `selectGuardTarget()` methods.

- [ ] **Step 1: Add P and G hotkeys**

In `Engine::handleKeyEvent()`, add cases in the switch:

```cpp
case input::P:
    if (!state->unitManager()->selected().empty()) {
        state->unitManager()->setState(UnitManager::State::SelectingPatrolTarget);
        addMessage("Select patrol destination");
    }
    return true;

case input::G:
    if (!state->unitManager()->selected().empty()) {
        state->unitManager()->setState(UnitManager::State::SelectingGuardTarget);
        addMessage("Select unit to guard");
    }
    return true;
```

Check `input::Key` enum for the exact names of P and G keys — they may be `input::Key::P` / `input::Key::G` or similar.

- [ ] **Step 2: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/Engine.cpp
git commit -m "feat: P and G hotkeys for patrol and guard commands"
```
