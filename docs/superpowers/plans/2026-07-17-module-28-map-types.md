# Module 28: Additional Map Types Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add 10+ additional random map types beyond the current 5 (Arabia, BlackForest, Islands, Arena, Nomad).

**Architecture:** Each map type is a generation function in `RandomMapGenerator` that sets terrain, resources, forests, water, and starting positions. New types follow the existing pattern: terrain fill -> water placement -> forest placement -> resource placement -> player starts.

**Tech Stack:** C++20, existing RandomMapGenerator, Perlin noise

---

## Background

- `RandomMapGenerator.h:16-22` defines MapType enum with 5 types
- Each type has a `generate()` method that fills the map
- Existing helpers: `placeForests()`, `placeResources()`, `noise()` (Perlin), elevation generation
- Map sizes: Tiny(72) to Giant(255) tiles

## New Map Types

| # | Map | Key Feature |
|---|-----|-------------|
| 1 | **Coastal** | Land with water on one side |
| 2 | **Rivers** | Two landmasses separated by a river with shallows |
| 3 | **Baltic** | Large central lake with land around |
| 4 | **Mediterranean** | Narrow sea between two continents |
| 5 | **Highland** | Elevated terrain with lots of hills |
| 6 | **Gold Rush** | Large gold pile in center, fought over |
| 7 | **Fortress** | Each player starts with walls and castle |
| 8 | **Oasis** | Desert with central forest/water oasis |
| 9 | **MegaRandom** | Randomly picks from all other types |
| 10 | **Team Islands** | Teams share an island |

## Key Files

- `src/mechanics/RandomMapGenerator.h` — MapType enum, generate methods
- `src/mechanics/RandomMapGenerator.cpp` — Map generation implementations
- `src/ui/RandomMapSetup.cpp` — Map type selection UI

---

### Task 1: Add map type enum entries

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.h`

- [ ] **Step 1: Extend MapType enum**

```cpp
enum MapType {
    Arabia = 0,
    BlackForest,
    Islands,
    Arena,
    Nomad,
    // New types
    Coastal,
    Rivers,
    Baltic,
    Mediterranean,
    Highland,
    GoldRush,
    Fortress,
    Oasis,
    MegaRandom,
    TeamIslands,
    MapTypeCount
};
```

- [ ] **Step 2: Add map type names for UI**

Ensure `mapTypeName()` or equivalent returns display names for the new types.

- [ ] **Step 3: Build**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/RandomMapGenerator.h
git commit -m "feat: add 10 new map type enum entries"
```

---

### Task 2: Implement Coastal map

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.cpp`

- [ ] **Step 1: Add generateCoastal()**

```cpp
void RandomMapGenerator::generateCoastal()
{
    // Fill with grass
    fillTerrain(TerrainId::Grass);

    // Water on one edge (right side, ~30% of map)
    const int waterStart = m_mapSize * 7 / 10;
    for (int x = waterStart; x < m_mapSize; x++) {
        for (int y = 0; y < m_mapSize; y++) {
            // Noisy coastline
            float nx = noise(x * 0.1f, y * 0.1f) * 4;
            if (x + nx > waterStart) {
                setTerrain(x, y, TerrainId::Water);
            }
        }
    }

    // Beach strip along coast
    addBeachStrip(waterStart);

    // Place forests on land side
    placeForests(0, 0, waterStart, m_mapSize, 0.35f);

    // Resources and players on land only
    placeResources(0, 0, waterStart - 2, m_mapSize);
    placePlayersInArea(0, 0, waterStart - 4, m_mapSize);

    applyElevation();
}
```

- [ ] **Step 2: Route in generate()**

In the `generate()` switch statement:

```cpp
case MapType::Coastal: generateCoastal(); break;
```

- [ ] **Step 3: Build and test**

```bash
cd build && make -j$(nproc)
```

Start a Coastal map, verify water on one side with noisy coastline.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: Coastal map type — land with water on one edge"
```

---

### Task 3: Implement Rivers map

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.cpp`

- [ ] **Step 1: Add generateRivers()**

```cpp
void RandomMapGenerator::generateRivers()
{
    fillTerrain(TerrainId::Grass);

    // River running diagonally across map
    const float riverWidth = 3.0f;
    for (int x = 0; x < m_mapSize; x++) {
        float riverCenter = m_mapSize / 2.0f + noise(x * 0.05f, 0) * 8;
        for (int y = 0; y < m_mapSize; y++) {
            float dist = std::abs(y - riverCenter);
            if (dist < riverWidth + noise(x * 0.1f, y * 0.1f) * 1.5f) {
                setTerrain(x, y, TerrainId::Water);
            }
        }
    }

    // Shallow ford crossings (2-3 points)
    int numFords = 2 + m_rng() % 2;
    for (int i = 0; i < numFords; i++) {
        int fx = m_mapSize * (i + 1) / (numFords + 1);
        float riverCenter = m_mapSize / 2.0f + noise(fx * 0.05f, 0) * 8;
        for (int dy = -2; dy <= 2; dy++) {
            int fy = static_cast<int>(riverCenter) + dy;
            if (fy >= 0 && fy < m_mapSize) {
                setTerrain(fx, fy, TerrainId::ShallowWater);
            }
        }
    }

    placeForests(0, 0, m_mapSize, m_mapSize, 0.30f);
    placeResources();
    placePlayersOnOppositeSides();
    applyElevation();
}
```

- [ ] **Step 2: Build and test**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 3: Commit**

```bash
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: Rivers map type — river with shallow fords"
```

---

### Task 4: Implement remaining map types

Repeat the pattern for each map type. Key terrain generation approaches:

- [ ] **Baltic**: Circle of water in center (`dist_from_center < radius`)
- [ ] **Mediterranean**: Horizontal water strip with noisy edges
- [ ] **Highland**: High elevation everywhere, sparse forests, lots of gold/stone
- [ ] **Gold Rush**: Standard land with 4x gold pile at map center
- [ ] **Fortress**: Standard Arabia + pre-built walls and castle per player
- [ ] **Oasis**: Desert terrain with circle of forest and water at center
- [ ] **MegaRandom**: Pick random MapType at generation time (excluding MegaRandom)
- [ ] **TeamIslands**: Like Islands but players on same team share an island

Each one follows the same pattern:
1. Add `generateXxx()` method
2. Add case in `generate()` switch
3. Build and test
4. Commit

- [ ] **Final commit for all remaining types**

```bash
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: Baltic, Mediterranean, Highland, GoldRush, Fortress, Oasis, MegaRandom, TeamIslands map types"
```

---

### Task 5: Update map selection UI

**Files:**
- Modify: `src/ui/RandomMapSetup.cpp`

- [ ] **Step 1: Add new map types to selection list**

Find where map type names are listed in the UI dropdown/selector and add the new types.

- [ ] **Step 2: Build and test**

```bash
cd build && make -j$(nproc)
```

Verify all new map types appear in the game setup screen.

- [ ] **Step 3: Commit**

```bash
git add src/ui/RandomMapSetup.cpp
git commit -m "feat: show all 15 map types in game setup screen"
```
