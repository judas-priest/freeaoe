# Module 13: Map Resource Placement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add missing Gaia resources to random map generation: boar, wolves, fish (on water maps), relics. Also fix resource placement to guarantee resources near each player's TC.

**Architecture:** Extend `RandomMapGenerator::placeResources()` with new placement functions for each missing resource type. Add a `placePlayerResources()` helper that guarantees standard AoE2 starting resources within a fixed radius of each TC. Uses existing `SyncRandom` for positions, `UnitFactory::createUnit()` for spawning.

**Tech Stack:** C++, RandomMapGenerator, UnitFactory, Map, SyncRandom

**Verified APIs:**
- `SyncRandom::inst()` — singleton; `.next()`, `.nextInt(max)` returns `[0,max)`, `.nextFloat()` returns `[0,1)`
- Start positions computed inline in `placeStartingUnits()` via circle formula — NOT stored in an array. Implementor must extract start positions into a `std::vector<MapPos>` or compute them the same way in `placeResources()`.
- Tile size: `Constants::TILE_SIZE` (from `core/Constants.h`)
- Unit creation: `UnitFactory::Inst().createUnit(unitId, map, players[0], pos)` where `players[0]` is Gaia
- Map terrain: `map->terrainIdAt(col, row)` or direct tile access
- **Unit IDs (verified against userpatch.aiscripters.net):** Wild Boar=48, Wolf=126, Relic=285, Forage Bush=59, Shore Fish=69, Great Fish Marlin=450, Tuna=457, Salmon=456, Snapper=458

**Important:** Start positions are NOT stored — the implementor should first refactor `placeStartingUnits()` to save positions into a member or local vector, then pass that to `placeResources()`. Or replicate the circle formula: `angle = 2*PI*p/playerCount`, `x = center + cos(angle) * radius * 0.35`, `y = center + sin(angle) * radius * 0.35`.

---

### Task 1: Add boar placement

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.cpp`

**Context:** `placeResources()` (line 209) creates Gaia units via:
```cpp
Unit::Ptr unit = UnitFactory::Inst().createUnit(unitId, map, players[0], pos);
```
where `players[0]` is Gaia. Boar ID is 48 (Wild Boar). Standard AoE2 places 2 boar per player within ~15 tiles of TC.

- [ ] **Step 1: Add boar spawning near each player**

In `placeResources()`, after the sheep placement block, add:

```cpp
// Place 2 boar per player, 10-15 tiles from start position
for (size_t p = 1; p < players.size(); p++) {
    MapPos startPos = startPositions[p]; // or however positions are stored
    for (int b = 0; b < 2; b++) {
        for (int attempt = 0; attempt < 20; attempt++) {
            float angle = syncRandom.nextFloat() * 2.f * M_PI;
            float dist = 10.f + syncRandom.nextFloat() * 5.f; // 10-15 tiles
            int col = static_cast<int>(startPos.x / TILE_SIZE + cos(angle) * dist);
            int row = static_cast<int>(startPos.y / TILE_SIZE + sin(angle) * dist);
            if (col < 1 || col >= size - 1 || row < 1 || row >= size - 1) {
                continue;
            }
            // Skip water tiles
            if (map->terrainIdAt(col, row) == 1 || map->terrainIdAt(col, row) == 2) {
                continue;
            }
            MapPos pos(col * TILE_SIZE + TILE_SIZE / 2, row * TILE_SIZE + TILE_SIZE / 2, 0);
            UnitFactory::Inst().createUnit(48, map, players[0], pos);
            break;
        }
    }
}
```

Adapt variable names to match the actual code. The key IDs: Boar = 48, TILE_SIZE is typically 48 or defined elsewhere.

- [ ] **Step 2: Build and test**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 3: Commit**

```bash
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: place 2 wild boar near each player's starting position"
```

---

### Task 2: Add wolf placement

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.cpp`

**Context:** Wolf ID is 126. AoE2 places wolves in clusters away from TCs (minimum ~20 tiles away). Wolves auto-attack nearby villagers.

- [ ] **Step 1: Add wolf spawning**

In `placeResources()`, add after the boar block:

```cpp
// Place wolf packs — away from all player starts
int wolfCount = size / 15;
for (int w = 0; w < wolfCount; w++) {
    for (int attempt = 0; attempt < 30; attempt++) {
        int col = 5 + syncRandom.next(size - 10);
        int row = 5 + syncRandom.next(size - 10);

        // Must be at least 20 tiles from any player start
        bool tooClose = false;
        for (size_t p = 1; p < players.size(); p++) {
            float dx = col * TILE_SIZE - startPositions[p].x;
            float dy = row * TILE_SIZE - startPositions[p].y;
            if (sqrt(dx * dx + dy * dy) < 20.f * TILE_SIZE) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) continue;

        // Skip water
        if (map->terrainIdAt(col, row) == 1 || map->terrainIdAt(col, row) == 2) {
            continue;
        }

        MapPos pos(col * TILE_SIZE + TILE_SIZE / 2, row * TILE_SIZE + TILE_SIZE / 2, 0);
        UnitFactory::Inst().createUnit(126, map, players[0], pos);
        break;
    }
}
```

- [ ] **Step 2: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: place wolf packs on random maps, away from player starts"
```

---

### Task 3: Add fish placement on water maps

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.cpp`

**Context:** Fish IDs: Shore Fish = 69, Deep Sea Fish = 450 (verify in dat). On Islands maps, water tiles exist but have no fish. Fish should be placed on water tiles (terrain ID 1).

- [ ] **Step 1: Add fish spawning for water maps**

In `placeResources()`, add:

```cpp
// Place fish on water tiles (Islands map)
if (settings.type == MapType::Islands) {
    int fishCount = size * size / 200; // Roughly proportional to water area
    int placed = 0;
    for (int attempt = 0; attempt < fishCount * 5 && placed < fishCount; attempt++) {
        int col = syncRandom.next(size);
        int row = syncRandom.next(size);
        if (map->terrainIdAt(col, row) != 1) { // Only on water
            continue;
        }
        MapPos pos(col * TILE_SIZE + TILE_SIZE / 2, row * TILE_SIZE + TILE_SIZE / 2, 0);
        // Alternate between shore fish (69) and deep sea fish (450)
        int fishId = (placed % 3 == 0) ? 450 : 69;
        UnitFactory::Inst().createUnit(fishId, map, players[0], pos);
        placed++;
    }
}
```

- [ ] **Step 2: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: place fish on water tiles for Islands map type"
```

---

### Task 4: Add relic placement

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.cpp`

**Context:** Relic ID = 285. AoE2 places 5 relics per map (standard), scattered across the map away from player starts. Relics are critical for relic victory and monastery gold generation.

- [ ] **Step 1: Add relic spawning**

In `placeResources()`, add:

```cpp
// Place 5 relics scattered across the map
for (int r = 0; r < 5; r++) {
    for (int attempt = 0; attempt < 50; attempt++) {
        int col = 5 + syncRandom.next(size - 10);
        int row = 5 + syncRandom.next(size - 10);

        // Skip water/beach/forest
        int terrain = map->terrainIdAt(col, row);
        if (terrain == 1 || terrain == 2 || terrain == 10) {
            continue;
        }

        // At least 8 tiles from any player start
        bool tooClose = false;
        for (size_t p = 1; p < players.size(); p++) {
            float dx = col * TILE_SIZE - startPositions[p].x;
            float dy = row * TILE_SIZE - startPositions[p].y;
            if (sqrt(dx * dx + dy * dy) < 8.f * TILE_SIZE) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) continue;

        MapPos pos(col * TILE_SIZE + TILE_SIZE / 2, row * TILE_SIZE + TILE_SIZE / 2, 0);
        UnitFactory::Inst().createUnit(285, map, players[0], pos);
        break;
    }
}
```

- [ ] **Step 2: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: place 5 relics on random maps for relic victory and gold income"
```

---

### Task 5: Guarantee resources near each player's TC

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.cpp`

**Context:** Currently gold, stone, and berries are placed at random positions with no guarantee of proximity to players. AoE2 standard: each player gets 2 gold patches (7+4 tiles), 2 stone patches (5+4 tiles), 1 berry bush cluster, all within 10-15 tiles of TC.

- [ ] **Step 1: Add per-player resource placement**

Add a helper function and call it from `placeResources()`:

```cpp
static void placePlayerResources(const std::shared_ptr<Map> &map,
                                  const std::vector<std::shared_ptr<Player>> &players,
                                  const std::vector<MapPos> &startPositions,
                                  SyncRandom &syncRandom, int size)
{
    const float TILE = 48.f; // or TILE_SIZE constant

    for (size_t p = 1; p < players.size(); p++) {
        MapPos start = startPositions[p];

        // 1 main gold patch (7 units) at 8-12 tiles
        {
            float angle = syncRandom.nextFloat() * 2.f * M_PI;
            float dist = 8.f + syncRandom.nextFloat() * 4.f;
            int baseCol = static_cast<int>(start.x / TILE + cos(angle) * dist);
            int baseRow = static_cast<int>(start.y / TILE + sin(angle) * dist);
            for (int g = 0; g < 7; g++) {
                int col = baseCol + (g % 3) - 1;
                int row = baseRow + (g / 3);
                if (col < 0 || col >= size || row < 0 || row >= size) continue;
                if (map->terrainIdAt(col, row) == 1) continue;
                MapPos pos(col * TILE + TILE / 2, row * TILE + TILE / 2, 0);
                UnitFactory::Inst().createUnit(66, map, players[0], pos); // Gold Mine
            }
        }

        // 1 secondary gold patch (4 units) at 18-22 tiles, opposite side
        {
            float angle = syncRandom.nextFloat() * 2.f * M_PI;
            float dist = 18.f + syncRandom.nextFloat() * 4.f;
            int baseCol = static_cast<int>(start.x / TILE + cos(angle) * dist);
            int baseRow = static_cast<int>(start.y / TILE + sin(angle) * dist);
            for (int g = 0; g < 4; g++) {
                int col = baseCol + (g % 2);
                int row = baseRow + (g / 2);
                if (col < 0 || col >= size || row < 0 || row >= size) continue;
                if (map->terrainIdAt(col, row) == 1) continue;
                MapPos pos(col * TILE + TILE / 2, row * TILE + TILE / 2, 0);
                UnitFactory::Inst().createUnit(66, map, players[0], pos);
            }
        }

        // 1 main stone patch (5 units) at 10-14 tiles
        {
            float angle = syncRandom.nextFloat() * 2.f * M_PI;
            float dist = 10.f + syncRandom.nextFloat() * 4.f;
            int baseCol = static_cast<int>(start.x / TILE + cos(angle) * dist);
            int baseRow = static_cast<int>(start.y / TILE + sin(angle) * dist);
            for (int s = 0; s < 5; s++) {
                int col = baseCol + (s % 3) - 1;
                int row = baseRow + (s / 3);
                if (col < 0 || col >= size || row < 0 || row >= size) continue;
                if (map->terrainIdAt(col, row) == 1) continue;
                MapPos pos(col * TILE + TILE / 2, row * TILE + TILE / 2, 0);
                UnitFactory::Inst().createUnit(102, map, players[0], pos); // Stone Mine
            }
        }

        // 1 secondary stone patch (4 units) at 20-24 tiles
        {
            float angle = syncRandom.nextFloat() * 2.f * M_PI;
            float dist = 20.f + syncRandom.nextFloat() * 4.f;
            int baseCol = static_cast<int>(start.x / TILE + cos(angle) * dist);
            int baseRow = static_cast<int>(start.y / TILE + sin(angle) * dist);
            for (int s = 0; s < 4; s++) {
                int col = baseCol + (s % 2);
                int row = baseRow + (s / 2);
                if (col < 0 || col >= size || row < 0 || row >= size) continue;
                if (map->terrainIdAt(col, row) == 1) continue;
                MapPos pos(col * TILE + TILE / 2, row * TILE + TILE / 2, 0);
                UnitFactory::Inst().createUnit(102, map, players[0], pos);
            }
        }

        // 1 berry patch (6 bushes) at 6-8 tiles
        {
            float angle = syncRandom.nextFloat() * 2.f * M_PI;
            float dist = 6.f + syncRandom.nextFloat() * 2.f;
            int baseCol = static_cast<int>(start.x / TILE + cos(angle) * dist);
            int baseRow = static_cast<int>(start.y / TILE + sin(angle) * dist);
            for (int b = 0; b < 6; b++) {
                int col = baseCol + (b % 3);
                int row = baseRow + (b / 3);
                if (col < 0 || col >= size || row < 0 || row >= size) continue;
                if (map->terrainIdAt(col, row) == 1) continue;
                MapPos pos(col * TILE + TILE / 2, row * TILE + TILE / 2, 0);
                UnitFactory::Inst().createUnit(59, map, players[0], pos); // Berry bush
            }
        }

        // 4 extra sheep at 8-12 tiles (existing places 4 nearby)
        for (int s = 0; s < 4; s++) {
            float angle = syncRandom.nextFloat() * 2.f * M_PI;
            float dist = 8.f + syncRandom.nextFloat() * 4.f;
            int col = static_cast<int>(start.x / TILE + cos(angle) * dist);
            int row = static_cast<int>(start.y / TILE + sin(angle) * dist);
            if (col < 0 || col >= size || row < 0 || row >= size) continue;
            if (map->terrainIdAt(col, row) == 1) continue;
            MapPos pos(col * TILE + TILE / 2, row * TILE + TILE / 2, 0);
            UnitFactory::Inst().createUnit(594, map, players[0], pos); // Sheep
        }

        // 3 deer at 12-16 tiles
        for (int d = 0; d < 3; d++) {
            float angle = syncRandom.nextFloat() * 2.f * M_PI;
            float dist = 12.f + syncRandom.nextFloat() * 4.f;
            int col = static_cast<int>(start.x / TILE + cos(angle) * dist);
            int row = static_cast<int>(start.y / TILE + sin(angle) * dist);
            if (col < 0 || col >= size || row < 0 || row >= size) continue;
            if (map->terrainIdAt(col, row) == 1) continue;
            MapPos pos(col * TILE + TILE / 2, row * TILE + TILE / 2, 0);
            UnitFactory::Inst().createUnit(65, map, players[0], pos); // Deer
        }
    }
}
```

- [ ] **Step 2: Call placePlayerResources and reduce random scatter**

In `placeResources()`, call `placePlayerResources()` first, then reduce the random global placement quantities (since players now have guaranteed resources):

```cpp
// Reduce global random patches since per-player resources are guaranteed
int globalGoldPatches = size / 40;   // was size/20
int globalStonePatches = size / 50;  // was size/25
int globalBerryPatches = 0;          // was size/30 (players have their own)
int globalDeer = size / 30;          // was size/15
```

- [ ] **Step 3: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: guarantee gold, stone, berries, deer near each player's TC"
```
