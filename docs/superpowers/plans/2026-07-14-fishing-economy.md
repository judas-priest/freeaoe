# Fishing Economy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Enable fishing ships to gather from shore fish, deep fish, and fish traps, with automatic re-targeting when a fish source is depleted.
**Architecture:** The ActionGather loop is already generic and data-driven. Fishing ships use DropSites from the dat file (Dock ID 45), and fish traps share the Farm class (genie::Unit::Farm). The only missing piece is auto-seeking the next fish after a source is depleted -- currently the gather action just completes when the target is empty and resources are dropped off.
**Tech Stack:** C++, SDL2
---

## Task 1: Verify Dock Drop-Off Works for Fishing Ships

**File:** `src/actions/ActionGather.cpp`

The `findDropSite()` function (line 151-180) already iterates `unit->data()->Action.DropSites` and matches against all units. Fishing Ships in the dat file have Dock (ID 45) in their DropSites array. No code change needed -- just verify the data path works.

**Verification:** Add a temporary DBG log to confirm. No permanent code change.

## Task 2: Verify Fish Trap Rebuild Path

**File:** `src/resource/GameSpecific.cpp`

Line 29 shows `genie::Unit::Farm` class covers both farms and fish traps. The Farm class (`src/mechanics/Farm.cpp`) handles auto-rebuild when food hits 0. Fish traps use the same class in `UnitFactory::createUnit()` (line 197: `if (ID == Unit::Farm)`).

**Issue:** Fish traps have a different unit ID than farms (ID 199 vs ID 50). The `UnitFactory` only checks `ID == Unit::Farm` (50). Fish traps won't get the Farm class treatment.

**Fix in** `src/mechanics/UnitFactory.cpp`, line 197:

Change:
```cpp
    if (ID == Unit::Farm) { // Farms are very special (shortbus special), so better to just use a special class
```

To:
```cpp
    if (gunit.Class == genie::Unit::Farm) { // Farms and Fish Traps share the Farm class
```

This uses the dat file's class field instead of hardcoded ID, so fish traps (class == Farm) also get the Farm treatment.

## Task 3: Add Auto-Seek Next Fish After Source Depleted

**File:** `src/actions/ActionGather.cpp`, function `maybeDropOff()` (lines 126-149)

Currently, when the target is empty (line 144: `target->resources[m_resourceType] > 0` is false), the gather action just drops off and completes. Add logic to find the nearest un-depleted fish of the same type and queue a new gather action to it.

Replace lines 143-146:
```cpp
    Unit::Ptr target = m_target.lock();
    if (target && target->resources[m_resourceType] > 0) {
        unit->actions.queueAction(std::make_shared<ActionGather>(unit, m_task));
    }
```

With:
```cpp
    Unit::Ptr target = m_target.lock();
    if (target && target->resources[m_resourceType] > 0) {
        // Target still has resources -- return to it after drop-off
        unit->actions.queueAction(std::make_shared<ActionGather>(unit, m_task));
    } else {
        // Target depleted -- find nearest fish/resource of same type
        Unit::Ptr nextTarget = findNextGatherTarget(unit);
        if (nextTarget) {
            Task newTask = m_task;
            newTask.target = nextTarget;
            unit->actions.queueAction(ActionMove::moveUnitTo(unit, nextTarget));
            unit->actions.queueAction(std::make_shared<ActionGather>(unit, newTask));
        }
    }
```

## Task 4: Implement findNextGatherTarget()

**File:** `src/actions/ActionGather.h`

Add the declaration after `findDropSite`:
```cpp
private:
    UpdateResult maybeDropOff(const std::shared_ptr<Unit> &unit);
    std::shared_ptr<Unit> findDropSite(const std::shared_ptr<Unit> &unit);
    std::shared_ptr<Unit> findNextGatherTarget(const std::shared_ptr<Unit> &unit);
```

**File:** `src/actions/ActionGather.cpp`

Add after `findDropSite()` (after line 180):
```cpp
std::shared_ptr<Unit> ActionGather::findNextGatherTarget(const std::shared_ptr<Unit> &unit)
{
    Unit::Ptr oldTarget = m_target.lock();
    const int targetClass = oldTarget ? oldTarget->data()->Class : -1;

    float closestDistance = std::numeric_limits<float>::max();
    Unit::Ptr closestUnit;

    for (const Unit::Ptr &other : unit->unitManager().units()) {
        if (other == oldTarget) {
            continue; // skip the depleted one
        }

        // Must be same class (e.g. OceanFish, DeepSeaFish, ShoreFish)
        if (targetClass >= 0 && other->data()->Class != targetClass) {
            continue;
        }

        // Must have the resource we're gathering
        if (other->resources[m_resourceType] <= 0) {
            continue;
        }

        // Must be alive (not a dead fish carcass)
        if (other->isDead() || other->isDying()) {
            continue;
        }

        const float distance = unit->position().distance(other->position());
        if (distance < closestDistance) {
            closestDistance = distance;
            closestUnit = other;
        }
    }

    return closestUnit;
}
```

## Task 5: Include ActionMove Header

**File:** `src/actions/ActionGather.cpp`

The `#include "ActionMove.h"` is already present at line 5. No change needed.

## Summary of Changes

| File | Change |
|------|--------|
| `src/mechanics/UnitFactory.cpp:197` | Use `gunit.Class == genie::Unit::Farm` instead of `ID == Unit::Farm` |
| `src/actions/ActionGather.cpp:143-146` | Add auto-seek next fish after drop-off when target depleted |
| `src/actions/ActionGather.cpp` (new function) | Add `findNextGatherTarget()` to scan for nearest same-class resource |
| `src/actions/ActionGather.h` | Declare `findNextGatherTarget()` |
