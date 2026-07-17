# Module 18: Map Terrain & Elevation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add elevation variation to random maps and use more terrain types for visual variety.

**Architecture:** Modify `generateTerrain()` to use Perlin noise for elevation (values 1-3, gentle hills). Add terrain variety: desert patches on Arabia, shallows on Islands, snow on Highland (future). Uses existing elevation rendering (already works per MEMORY.md — was re-enabled with gentle hills 1-3).

**Tech Stack:** C++, RandomMapGenerator, Map tile elevation

**Verified APIs:**
- `tile.elevation = 2` currently hardcoded at line 107 of RandomMapGenerator.cpp
- `noise(float x, float y)` exists as static method in RandomMapGenerator (hash-based, not true Perlin)
- Terrain IDs confirmed in code: Grass1=0, Water=1, Beach=2, Leaves=5, Dirt1=6, Forest=10, Dirt3=11, Desert=14
- Elevation valid range in AoE2: 0-7 (0=water level, 2=standard flat)
- `Constants::TILE_SIZE` from `core/Constants.h`

---

### Task 1: Add elevation variation

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.cpp`

**Context:** Currently all tiles forced to `elevation = 2` with comment "slopes cause black tile artifacts". However, per MEMORY.md, elevation was re-enabled with gentle hills (1-3) via Perlin noise with bounds-checking. The random map generator may still be forcing flat. Verify and update.

- [ ] **Step 1: Check if elevation is already re-enabled in generateTerrain**

Read `generateTerrain()` and check if elevation is still hardcoded to 2 or if it uses the Perlin noise hills from the night session.

If still flat, add elevation variation:

```cpp
// Gentle hills using noise — values 1-3 (2 is flat, 1 is valley, 3 is hill)
float elevNoise = noise(col * 0.05f, row * 0.05f); // Low frequency for gentle hills
int elevation = 2; // Default flat
if (elevNoise > 0.6f) {
    elevation = 3; // Hill
} else if (elevNoise < 0.3f) {
    elevation = 1; // Valley
}

// Keep water and beach areas flat
if (terrainId == 1 || terrainId == 2) {
    elevation = 0; // Water level
}

tile.elevation = elevation;
```

- [ ] **Step 2: Ensure elevation bounds (1-7 valid range for AoE2)**

Add bounds check:

```cpp
tile.elevation = std::clamp(elevation, 0, 7);
```

- [ ] **Step 3: Build, test, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: elevation variation on random maps using Perlin noise"
```

---

### Task 2: Add terrain variety

**Files:**
- Modify: `src/mechanics/RandomMapGenerator.cpp`

**Context:** Only 8 terrain IDs used. AoE2 terrain IDs (from genie dat): 0=Grass1, 3=Grass3, 4=Grass2, 5=Leaves, 6=Dirt1, 9=Grass4, 11=Dirt3, 12=Shallows, 14=Desert, 19=Jungle, 24=Road, 26=Snow, 29=IceSea, 35=MediterraneanGrass. Verify IDs against actual dat before using.

- [ ] **Step 1: Add desert patches to Arabia**

In Arabia terrain generation, add:

```cpp
// Occasional desert patches on Arabia
float desertNoise = noise(col * 0.03f + 100.f, row * 0.03f + 100.f);
if (desertNoise > 0.75f && terrainId == 0) {
    terrainId = 14; // Desert
}
```

- [ ] **Step 2: Add shallows on Islands**

For Islands, where water meets land, use shallow terrain (ID 4 or 12, verify):

```cpp
// Shallows at the water/land border on Islands
if (settings.type == MapType::Islands) {
    if (terrainId == 1) { // Water
        // Check if any adjacent tile is land
        bool nearLand = false;
        for (int dx = -1; dx <= 1 && !nearLand; dx++) {
            for (int dy = -1; dy <= 1 && !nearLand; dy++) {
                if (dx == 0 && dy == 0) continue;
                int nc = col + dx, nr = row + dy;
                if (nc < 0 || nc >= size || nr < 0 || nr >= size) continue;
                int adjTerrain = map->terrainIdAt(nc, nr);
                if (adjTerrain != 1 && adjTerrain != 2) {
                    nearLand = true;
                }
            }
        }
        if (nearLand) {
            terrainId = 4; // Shallows (verify ID)
        }
    }
}
```

- [ ] **Step 3: Build, test, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/RandomMapGenerator.cpp
git commit -m "feat: terrain variety — desert patches on Arabia, shallows on Islands"
```
