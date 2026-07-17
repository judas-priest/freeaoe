# Module 2: Combat Stances

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make stances fully functional: Aggressive = chase far, Defensive = fight back then return, StandGround = fire in range but never move, NoAttack = ignore enemies.

**Architecture:** `UnitActionHandler::checkForAutoTargets()` (UnitActionHandler.cpp:215) already filters out StandGround and NoAttack from auto-targeting. But StandGround should still auto-target enemies within weapon range (not LOS). And ActionAttack (ActionAttack.cpp:107) always chases with no pursuit limit — need stance-aware pursuit distance and return-to-position for Defensive.

**Tech Stack:** C++20, modify 3 existing files.

---

### Task 1: StandGround auto-targets within weapon range only

**Files:**
- Modify: `src/mechanics/UnitActionHandler.cpp:215-299`

- [ ] **Step 1: Change checkForAutoTargets to allow StandGround within weapon range**

Currently line 218 blocks both StandGround and NoAttack. Change to only block NoAttack, and add range filtering for StandGround:

```cpp
Task UnitActionHandler::checkForAutoTargets()
{
    // NoAttack: never auto-target
    if (m_unit->stance == Unit::Stance::NoAttack
        || m_autoTargetTasks.size() == 0 || m_currentAction) {
        return {};
    }

    const MapPtr map = m_unit->m_map.lock();
    if (!map) {
        WARN << "no map";
        return {};
    }

    const genie::Unit *data = m_unit->m_data;

    // StandGround: scan weapon range, not LOS
    const int scanRange = (m_unit->stance == Unit::Stance::StandGround)
        ? static_cast<int>(m_unit->effectiveRange())
        : data->LineOfSight;

    Task newTask;
    Unit::Ptr target;

    const MapPos position = m_unit->position();
    const int left = position.x / Constants::TILE_SIZE - scanRange;
    const int top = position.y / Constants::TILE_SIZE - scanRange;
    const int right = position.x / Constants::TILE_SIZE + scanRange;
    const int bottom = position.y / Constants::TILE_SIZE + scanRange;

    float closestDistance = scanRange * Constants::TILE_SIZE;

    // ... rest of scanning loop unchanged ...
```

- [ ] **Step 2: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

- [ ] **Step 3: Commit**

```bash
git add src/mechanics/UnitActionHandler.cpp
git commit -m "$(cat <<'EOF'
feat: StandGround auto-targets enemies within weapon range, not LOS
EOF
)"
```

---

### Task 2: StandGround prevents chase in ActionAttack

**Files:**
- Modify: `src/actions/ActionAttack.cpp:107-120`

- [ ] **Step 1: Add stance check before chasing target**

At line 107 in ActionAttack.cpp, when unit is out of range and needs to move closer, check stance:

```cpp
// Check if we are too far away
if (!overlaps && distance > unit->effectiveRange()) {
    // StandGround: never chase, just stop attacking
    if (unit->stance == Unit::Stance::StandGround) {
        return IAction::UpdateResult::Completed;
    }

    if (!unit->effectiveSpeed()) {
        DBG << "this unit can't move...";
        return IAction::UpdateResult::Failed;
    }
    DBG << unit->debugName << "is too far away" << distance << unit->effectiveRange();

    std::shared_ptr<ActionMove> moveAction = ActionMove::moveUnitTo(unit, targetUnit);
    moveAction->maxDistance = unit->effectiveRange() * Constants::TILE_SIZE;
    unit->actions.prependAction(moveAction);

    return IAction::UpdateResult::NotUpdated;
}
```

- [ ] **Step 2: Build and test**

Set unit to StandGround, send enemy past it. Unit should fire while in range but never chase.

- [ ] **Step 3: Commit**

```bash
git add src/actions/ActionAttack.cpp
git commit -m "$(cat <<'EOF'
feat: StandGround stance prevents chasing in ActionAttack
EOF
)"
```

---

### Task 3: Defensive stance — return to original position

**Files:**
- Modify: `src/mechanics/Unit.h` — add defense position field
- Modify: `src/actions/ActionAttack.cpp` — limit pursuit and return home

- [ ] **Step 1: Add defense position to Unit.h**

Add near the `stance` field (around line 153):

```cpp
MapPos defensePosition;
bool hasDefensePosition = false;
```

- [ ] **Step 2: Record defense position when Defensive unit starts attacking**

At the top of `ActionAttack::update()`, after getting the unit pointer (around line 73):

```cpp
if (unit->stance == Unit::Stance::Defensive && !unit->hasDefensePosition) {
    unit->defensePosition = unit->position();
    unit->hasDefensePosition = true;
}
```

- [ ] **Step 3: Limit Defensive pursuit to 5 tiles from defense position**

After the range check (line 107), when the unit would chase, check distance from home:

```cpp
if (!overlaps && distance > unit->effectiveRange()) {
    if (unit->stance == Unit::Stance::StandGround) {
        return IAction::UpdateResult::Completed;
    }

    // Defensive: don't chase beyond 5 tiles from original position
    if (unit->stance == Unit::Stance::Defensive && unit->hasDefensePosition) {
        float distFromHome = unit->position().distance(unit->defensePosition) / Constants::TILE_SIZE;
        if (distFromHome > 5.f) {
            // Disengage and return home
            unit->hasDefensePosition = false;
            m_unitManager->moveUnitTo(unit, unit->defensePosition);
            return IAction::UpdateResult::Completed;
        }
    }

    // ... existing chase code ...
}
```

- [ ] **Step 4: Return to defense position when attack completes**

When ActionAttack::update() returns Completed (target dead at line 179):

```cpp
if (targetUnit && targetUnit->healthLeft() <= 0.f) {
    // Defensive: return to original position
    if (unit->stance == Unit::Stance::Defensive && unit->hasDefensePosition) {
        unit->hasDefensePosition = false;
        m_unitManager->moveUnitTo(unit, unit->defensePosition);
    }
    return IAction::UpdateResult::Completed;
}
```

Note: `m_unitManager` access — the unit has a reference via `unit->m_unitManager`. Check the exact field name; may need `UnitManager &unitManager` from `m_unit`. Alternatively, use:

```cpp
auto moveAction = ActionMove::moveUnitTo(unit, unit->defensePosition, m_task);
unit->actions.queueAction(moveAction);
```

- [ ] **Step 5: Build and test**

Set archer to Defensive. Enemy walks by — archer attacks, chases up to 5 tiles, then returns to starting position.

- [ ] **Step 6: Commit**

```bash
git add src/mechanics/Unit.h src/actions/ActionAttack.cpp
git commit -m "$(cat <<'EOF'
feat: Defensive stance — limited pursuit range, return to original position
EOF
)"
```
