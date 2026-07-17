# Module 10: Naval Units & Water Pathfinding

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ships can move on water tiles, engage in naval combat, and transport ships can load/unload land units at shorelines.

**Architecture:** `ActionMove` (ActionMove.cpp) uses A* pathfinding with terrain passability via `m_terrainMoveMultipliers[tile.terrainId]` (line 944). Water terrain IDs: 1,2,3,4,22,26. Ships have classes `genie::Unit::TradeBoat=2`, `TransportBoat=20`, `FishingBoat=21`, `Warship=22`. The passability multipliers come from `TerrainRestriction.PassableBuildableDmgMultiplier` — ships should already have water=passable, land=blocked from genie data. Naval combat should work via existing `ActionAttack` — ships are ranged units. Transport load/unload needs garrison at shore + ungarrison on land.

**Tech Stack:** C++20, modify existing pathfinding + add shore helpers.

---

### Task 1: Verify ship pathfinding works from genie data

**Files:**
- Read: `src/actions/ActionMove.cpp:940-950` — passability check

- [ ] **Step 1: Check if ship terrain restrictions already work**

The passability check at ActionMove.cpp:944:
```cpp
const MapTile &tile = m_map->getTileAt(tileX, tileY);
if (m_terrainMoveMultipliers[tile.terrainId] == 0) {
    return false;  // Impassable
}
```

`m_terrainMoveMultipliers` is loaded from genie `TerrainRestriction` data per unit. Ships should have their own terrain restriction that makes water passable and land blocked.

**Test**: Spawn a ship on an Islands map and try to move it. If it moves on water, pathfinding already works from data.

- [ ] **Step 2: If ships can't pathfind, check terrain restriction loading**

Search for where `m_terrainMoveMultipliers` is populated. It should use `unit->data()->TerrainRestriction` to index into `DataManager`'s terrain restriction table. If ships use the same restriction as land units, they'll be stuck.

Verify the restriction index for ships vs land units differs.

- [ ] **Step 3: Add Map helper methods for shore detection**

```cpp
// In Map.h, add:
bool isWaterTerrain(int terrainId) const {
    return terrainId == 1 || terrainId == 2 || terrainId == 3 ||
           terrainId == 4 || terrainId == 22 || terrainId == 26;
}

bool isWaterTile(int col, int row) const {
    if (!isValidTile(col, row)) return false;
    return isWaterTerrain(getTileAt(col, row).terrainId);
}

bool isShoreTile(int col, int row) const {
    if (!isWaterTile(col, row)) return false;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx == 0 && dy == 0) continue;
            if (isValidTile(col+dx, row+dy) && !isWaterTile(col+dx, row+dy)) {
                return true;
            }
        }
    }
    return false;
}

MapPos nearestLandTile(const MapPos &waterPos) const {
    int cx = waterPos.x / Constants::TILE_SIZE;
    int cy = waterPos.y / Constants::TILE_SIZE;
    for (int r = 1; r <= 5; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dy = -r; dy <= r; ++dy) {
                int nx = cx + dx, ny = cy + dy;
                if (isValidTile(nx, ny) && !isWaterTile(nx, ny)) {
                    return MapPos(nx * Constants::TILE_SIZE + Constants::TILE_SIZE / 2,
                                  ny * Constants::TILE_SIZE + Constants::TILE_SIZE / 2, 0);
                }
            }
        }
    }
    return waterPos; // fallback
}
```

- [ ] **Step 4: Build and commit**

```bash
git add src/mechanics/Map.h
git commit -m "$(cat <<'EOF'
feat: water/shore tile detection helpers for naval pathfinding
EOF
)"
```

---

### Task 2: Naval combat verification

**Files:**
- Read: `src/actions/ActionAttack.cpp`

- [ ] **Step 1: Verify ships can attack**

Ships are standard ranged units in genie data. `ActionAttack` shouldn't have any land-only restriction. Test: spawn two enemy warships near each other, right-click one to attack the other.

- [ ] **Step 2: If combat doesn't work, check for class-based filtering**

Search ActionAttack.cpp for any unit class checks that might exclude ships. Remove if found.

- [ ] **Step 3: Commit if changes needed**

```bash
git add src/actions/ActionAttack.cpp
git commit -m "$(cat <<'EOF'
fix: remove land-only restriction from ActionAttack for naval combat
EOF
)"
```

---

### Task 3: Transport ship load/unload at shore

**Files:**
- Modify: `src/mechanics/GameState.cpp` — Garrison handler for transport boarding
- Modify: `src/mechanics/GameState.cpp` — Ungarrison handler for transport unloading

- [ ] **Step 1: Add Garrison command handler**

`CommandType::Garrison` (GameCommand.h:6) has NO handler. Add it:

```cpp
case CommandType::Garrison: {
    Unit::Ptr target = m_unitManager->unitById(static_cast<size_t>(cmd.targetId));
    if (!target || !target->isAlive()) break;

    for (int unitId : cmd.unitIds) {
        Unit::Ptr unit = m_unitManager->unitById(static_cast<size_t>(unitId));
        if (!unit || !unit->isAlive()) continue;

        Task task = unit->actions.findAnyTask(genie::ActionType::Garrison, target->data()->ID);
        if (!task.data) continue;
        task.target = target;
        IAction::assignTask(task, unit, IAction::AssignType::Replace);
    }
    break;
}
```

- [ ] **Step 2: Modify Ungarrison for transport ships — place on land**

In the Ungarrison handler (from Module 1), add shore-aware placement for transport ships:

```cpp
case CommandType::Ungarrison: {
    for (int unitId : cmd.unitIds) {
        Unit::Ptr container = m_unitManager->unitById(static_cast<size_t>(unitId));
        if (!container || !container->isAlive()) continue;

        auto building = Building::fromUnit(container);
        if (building) {
            building->ungarrisonAll();
        } else {
            // Transport ship: place units on nearest land
            MapPtr map = m_map;
            MapPos landPos = map->nearestLandTile(container->position());

            int count = 0;
            for (auto &weakUnit : container->garrisonedUnits) {
                if (auto u = weakUnit.lock()) {
                    count++;
                }
            }

            int i = 0;
            float spacing = Constants::TILE_SIZE;
            for (auto it = container->garrisonedUnits.begin(); it != container->garrisonedUnits.end(); ) {
                auto u = it->lock();
                if (u) {
                    u->garrisonedInUnit.reset();
                    // Spread units around the landing point
                    float angle = (2.f * M_PI * i) / std::max(1, count);
                    MapPos exitPos = landPos;
                    exitPos.x += std::cos(angle) * spacing;
                    exitPos.y += std::sin(angle) * spacing;
                    u->setPosition(exitPos);
                    i++;
                }
                it = container->garrisonedUnits.erase(it);
            }
        }
    }
    break;
}
```

- [ ] **Step 3: Build and test**

Garrison villagers into transport ship → sail to enemy shore → ungarrison. Units should appear on land near the ship.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/GameState.cpp
git commit -m "$(cat <<'EOF'
feat: transport ship load/unload — garrison handler + shore-aware ungarrison
EOF
)"
```
