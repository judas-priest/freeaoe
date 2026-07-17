# Module 5: Unit Formations

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When multiple units are selected and given a move command, they arrange in the selected formation (Line, Box, Flank, SpreadOut).

**Architecture:** Create `FormationHelper` utility that computes offset positions. Modify `GameState::executeCommands()` Move handler (GameState.cpp:359) to use it. Currently all units pathfind to the same MapPos — instead, each gets a unique formation-offset target. `Unit::s_formation` (Unit.h) already tracks the current formation, and `CommandType::SetFormation` (GameCommand.h:11) is defined but unhandled.

**Tech Stack:** C++20, new utility + modify GameState.cpp.

---

### Task 1: Create FormationHelper

**Files:**
- Create: `src/mechanics/FormationHelper.h`
- Create: `src/mechanics/FormationHelper.cpp`

- [ ] **Step 1: Create FormationHelper header**

```cpp
// src/mechanics/FormationHelper.h
#pragma once

#include "core/Types.h"
#include "Unit.h"
#include <vector>
#include <memory>

struct FormationHelper {
    static std::vector<MapPos> computePositions(
        const std::vector<Unit::Ptr> &units,
        const MapPos &targetCenter,
        Unit::Formation formation,
        float facingAngle
    );
};
```

- [ ] **Step 2: Create FormationHelper implementation**

```cpp
// src/mechanics/FormationHelper.cpp
#include "FormationHelper.h"
#include "core/Constants.h"
#include <cmath>

std::vector<MapPos> FormationHelper::computePositions(
    const std::vector<Unit::Ptr> &units,
    const MapPos &targetCenter,
    Unit::Formation formation,
    float facingAngle)
{
    const size_t count = units.size();
    std::vector<MapPos> positions(count, targetCenter);
    if (count <= 1) return positions;

    const float spacing = Constants::TILE_SIZE * 1.5f;
    const float cosA = std::cos(facingAngle);
    const float sinA = std::sin(facingAngle);
    const float perpX = -sinA;
    const float perpY = cosA;

    switch (formation) {
    case Unit::Formation::Line: {
        float totalWidth = (count - 1) * spacing;
        float startOffset = -totalWidth / 2.f;
        for (size_t i = 0; i < count; ++i) {
            float offset = startOffset + i * spacing;
            positions[i].x = targetCenter.x + perpX * offset;
            positions[i].y = targetCenter.y + perpY * offset;
        }
        break;
    }
    case Unit::Formation::Box: {
        int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
        int rows = static_cast<int>(std::ceil(static_cast<float>(count) / cols));
        float colStart = -(cols - 1) * spacing / 2.f;
        float rowStart = -(rows - 1) * spacing / 2.f;
        for (size_t i = 0; i < count; ++i) {
            int col = i % cols;
            int row = i / cols;
            float perpOffset = colStart + col * spacing;
            float fwdOffset = rowStart + row * spacing;
            positions[i].x = targetCenter.x + perpX * perpOffset + cosA * fwdOffset;
            positions[i].y = targetCenter.y + perpY * perpOffset + sinA * fwdOffset;
        }
        break;
    }
    case Unit::Formation::Flank: {
        positions[0] = targetCenter;
        for (size_t i = 1; i < count; ++i) {
            int side = (i % 2 == 1) ? 1 : -1;
            int rank = static_cast<int>((i + 1) / 2);
            float perpOffset = side * rank * spacing;
            float fwdOffset = -rank * spacing * 0.7f;
            positions[i].x = targetCenter.x + perpX * perpOffset + cosA * fwdOffset;
            positions[i].y = targetCenter.y + perpY * perpOffset + sinA * fwdOffset;
        }
        break;
    }
    case Unit::Formation::SpreadOut: {
        float radius = count * spacing / (2.f * static_cast<float>(M_PI));
        radius = std::max(radius, spacing);
        for (size_t i = 0; i < count; ++i) {
            float angle = (2.f * static_cast<float>(M_PI) * i) / count;
            positions[i].x = targetCenter.x + std::cos(angle) * radius;
            positions[i].y = targetCenter.y + std::sin(angle) * radius;
        }
        break;
    }
    }

    return positions;
}
```

- [ ] **Step 3: Add to CMakeLists.txt**

- [ ] **Step 4: Build**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

- [ ] **Step 5: Commit**

```bash
git add src/mechanics/FormationHelper.h src/mechanics/FormationHelper.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: FormationHelper — compute positions for Line, Box, Flank, SpreadOut
EOF
)"
```

---

### Task 2: Apply formations to move commands

**Files:**
- Modify: `src/mechanics/GameState.cpp:359-367` — use FormationHelper for multi-unit moves

- [ ] **Step 1: Replace Move handler to use formations**

```cpp
#include "mechanics/FormationHelper.h"

case CommandType::Move: {
    MapPos targetPos(cmd.x, cmd.y, 0);

    // Collect units
    std::vector<Unit::Ptr> units;
    for (int unitId : cmd.unitIds) {
        Unit::Ptr unit = m_unitManager->unitById(static_cast<size_t>(unitId));
        if (unit && unit->isAlive()) {
            units.push_back(unit);
        }
    }

    if (units.size() > 1) {
        // Calculate facing angle from group center to target
        MapPos center;
        for (auto &u : units) {
            center.x += u->position().x;
            center.y += u->position().y;
        }
        center.x /= units.size();
        center.y /= units.size();
        float angle = std::atan2(targetPos.y - center.y, targetPos.x - center.x);

        auto positions = FormationHelper::computePositions(
            units, targetPos, Unit::s_formation, angle);

        for (size_t i = 0; i < units.size(); ++i) {
            m_unitManager->moveUnitTo(units[i], positions[i]);
        }
    } else {
        for (auto &unit : units) {
            m_unitManager->moveUnitTo(unit, targetPos);
        }
    }
    break;
}
```

- [ ] **Step 2: Add SetFormation command handler**

```cpp
case CommandType::SetFormation: {
    // cmd.amount or cmd.resourceType used for formation index
    int formIdx = cmd.amount;
    if (formIdx >= 0 && formIdx <= 3) {
        Unit::s_formation = static_cast<Unit::Formation>(formIdx);
    }
    break;
}
```

- [ ] **Step 3: Build and test**

Select 6+ units, set formation to Box, right-click to move. They should arrange in a square grid.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/GameState.cpp
git commit -m "$(cat <<'EOF'
feat: formation positioning for group move commands + SetFormation handler
EOF
)"
```
