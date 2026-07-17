# Module 30: Elevation LOS Bonus — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Units on higher elevation get bonus line of sight (+2 tiles per elevation level), matching AoE2 behavior.

**Architecture:** Modify `Unit::forEachVisibleTile()` to factor in the unit's current elevation vs base elevation. The LOS circle radius grows when the unit is on a hill.

**Tech Stack:** C++, genie engine data

---

### Task 1: Add elevation-based LOS bonus

**Files:**
- Modify: `src/mechanics/Unit.cpp:662-675` (forEachVisibleTile)
- Modify: `src/mechanics/Unit.h:312` (declaration — add map parameter if needed)

- [ ] **Step 1: Read current forEachVisibleTile implementation**

Current code at `Unit.cpp:662-675`:
```cpp
void Unit::forEachVisibleTile(const std::function<void (const int, const int)> &action)
{
    const int los = m_lineOfSight;
    const int tileXOffset = position().x / Constants::TILE_SIZE;
    const int tileYOffset = position().y / Constants::TILE_SIZE;
    for (int y=-los; y<= los; y++) {
        for (int x=-los; x<= los; x++) {
            if (x*x + y*y < los*los) {
                action(x + tileXOffset, y + tileYOffset);
            }
        }
    }
}
```

- [ ] **Step 2: Modify forEachVisibleTile to include elevation bonus**

In AoE2, each elevation level above flat ground gives +2 LOS tiles. The unit's elevation is available via `position().z` divided by `DataManager::Inst().terrainBlock().ElevHeight`.

Replace the function body in `Unit.cpp`:
```cpp
void Unit::forEachVisibleTile(const std::function<void (const int, const int)> &action)
{
    const float elevHeight = DataManager::Inst().terrainBlock().ElevHeight;
    const int unitElevation = (elevHeight > 0) ? static_cast<int>(position().z / elevHeight) : 0;
    const int los = m_lineOfSight + unitElevation * 2;
    const int tileXOffset = position().x / Constants::TILE_SIZE;
    const int tileYOffset = position().y / Constants::TILE_SIZE;
    for (int y=-los; y<= los; y++) {
        for (int x=-los; x<= los; x++) {
            if (x*x + y*y < los*los) {
                action(x + tileXOffset, y + tileYOffset);
            }
        }
    }
}
```

Add include if not present at top of Unit.cpp:
```cpp
#include "resource/DataManager.h"
```

- [ ] **Step 3: Update auto-target scan range to match**

In `UnitActionHandler.cpp:215-249`, the auto-target scan uses `m_data->LineOfSight` directly. It should also get the elevation bonus so units on hills can auto-attack from further away.

At `UnitActionHandler.cpp`, find the scanRange calculation:
```cpp
const int scanRange = (m_unit->stance == Unit::Stance::StandGround)
    ? static_cast<int>(m_unit->effectiveRange())
    : m_data->LineOfSight;
```

Replace with:
```cpp
const float elevHeight = DataManager::Inst().terrainBlock().ElevHeight;
const int unitElevation = (elevHeight > 0) ? static_cast<int>(m_unit->position().z / elevHeight) : 0;
const int baseLos = m_data->LineOfSight + unitElevation * 2;
const int scanRange = (m_unit->stance == Unit::Stance::StandGround)
    ? static_cast<int>(m_unit->effectiveRange())
    : baseLos;
```

Add include if not present:
```cpp
#include "resource/DataManager.h"
```

- [ ] **Step 4: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
Expected: Clean compilation, no errors.

- [ ] **Step 5: Commit**

```bash
git add src/mechanics/Unit.cpp src/mechanics/UnitActionHandler.cpp
git commit -m "feat: elevation LOS bonus (+2 tiles per elevation level)"
```
