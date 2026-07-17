# Module 1: Ungarrison Command

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Wire the existing `CommandType::Ungarrison` (GameCommand.h:17) to existing `Building::ungarrisonAll()` / `Unit::ungarrisonAllUnits()` methods in the command dispatch.

**Architecture:** `Building::ungarrisonAll()` and `Unit::ungarrisonAllUnits()` already exist (Building.cpp:72, Unit.h:286). The only missing piece is the `case CommandType::Ungarrison` in `GameState::executeCommands()` (GameState.cpp:355). Units keep their pre-garrison position and are placed there on ungarrison (Building.cpp:62 TODO comment).

**Tech Stack:** C++20, modify one file.

---

### Task 1: Add Ungarrison command handler

**Files:**
- Modify: `src/mechanics/GameState.cpp:479` — add case before `default:`

- [ ] **Step 1: Add Ungarrison case to executeCommands switch**

Insert before the `default:` case at line 479 in `GameState::executeCommands()`:

```cpp
case CommandType::Ungarrison: {
    for (int unitId : cmd.unitIds) {
        Unit::Ptr container = m_unitManager->unitById(static_cast<size_t>(unitId));
        if (!container || !container->isAlive()) continue;

        // Building garrison
        auto building = Building::fromUnit(container);
        if (building) {
            building->ungarrisonAll();
        } else {
            // Transport ship / ram garrison
            container->ungarrisonAllUnits();
        }
    }
    break;
}
```

- [ ] **Step 2: Build**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
Expected: Clean build

- [ ] **Step 3: Test**

Garrison units into a Town Center, then issue Ungarrison. Units should reappear at their pre-garrison positions.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/GameState.cpp
git commit -m "$(cat <<'EOF'
feat: wire Ungarrison command to existing ungarrisonAll methods
EOF
)"
```

---

### Task 2: Dispatch Ungarrison from UI

**Files:**
- Modify: `src/ui/ActionPanel.cpp` — ensure ungarrison button emits the correct GameCommand
- Modify: `src/Engine.cpp` — ensure the button click creates and dispatches a GameCommand

- [ ] **Step 1: Find how ActionPanel buttons dispatch commands**

Search ActionPanel.cpp for how existing buttons (Stop, Patrol, etc.) create GameCommands. Follow the same pattern for Ungarrison.

- [ ] **Step 2: Verify the ungarrison button creates a GameCommand with `CommandType::Ungarrison` and the building's ID in `unitIds`**

The ActionPanel already has a `Garrison` command enum (value 240). Check if there's also an ungarrison button or if it needs to be added.

- [ ] **Step 3: Build, test on device, commit**

```bash
git add src/ui/ActionPanel.cpp src/Engine.cpp
git commit -m "$(cat <<'EOF'
feat: ungarrison button dispatches GameCommand through lockstep
EOF
)"
```

---

### Task 3: Improve ungarrison positioning (optional)

**Files:**
- Modify: `src/mechanics/Building.cpp:49-70` — place units around building instead of at pre-garrison pos

Currently `Building::ungarrison()` has a TODO at line 62: "find a nice position to put the unit". Improve it:

- [ ] **Step 1: Place ungarrisoned units in a circle around the building**

```cpp
bool Building::ungarrison(const std::shared_ptr<Unit> &unit)
{
    std::vector<std::weak_ptr<Unit>>::iterator it = garrisonedUnits.begin();
    int index = 0;
    for (; it != garrisonedUnits.end(); it++) {
        Unit::Ptr garrisoned = it->lock();
        if (!garrisoned) {
            it = garrisonedUnits.erase(it);
            continue;
        }

        if (garrisoned == unit) {
            unit->garrisonedIn.reset();
            it = garrisonedUnits.erase(it);

            // Place around building perimeter
            float angle = (2.f * M_PI * index) / std::max(1, (int)garrisonedUnits.size() + 1);
            float radius = std::max(tileSize().width, tileSize().height) * Constants::TILE_SIZE / 2.f + Constants::TILE_SIZE;
            MapPos exitPos = position();
            exitPos.x += std::cos(angle) * radius;
            exitPos.y += std::sin(angle) * radius;
            unit->setPosition(exitPos);

            return true;
        }
        index++;
    }
    return false;
}
```

- [ ] **Step 2: Same for ungarrisonAll — spread units in a circle**

```cpp
void Building::ungarrisonAll()
{
    int count = garrisonedUnits.size();
    float radius = std::max(tileSize().width, tileSize().height) * Constants::TILE_SIZE / 2.f + Constants::TILE_SIZE;
    int i = 0;
    for (auto &weakUnit : garrisonedUnits) {
        Unit::Ptr unit = weakUnit.lock();
        if (unit) {
            unit->garrisonedIn.reset();
            float angle = (2.f * M_PI * i) / std::max(1, count);
            MapPos exitPos = position();
            exitPos.x += std::cos(angle) * radius;
            exitPos.y += std::sin(angle) * radius;
            unit->setPosition(exitPos);
            i++;
        }
    }
    garrisonedUnits.clear();
}
```

- [ ] **Step 3: Build, test, commit**

```bash
git add src/mechanics/Building.cpp
git commit -m "$(cat <<'EOF'
feat: place ungarrisoned units in circle around building perimeter
EOF
)"
```
