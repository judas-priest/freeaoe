# Module 36: Ice/Bridges Terrain — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add ice terrain to map generator for winter-themed maps (Baltic, Highland).

**Architecture:** Ice terrain depends on the .dat file having correct terrain restriction data. In AoE2 HD, terrain IDs include: 26 (Ice), 35 (Snow/Dirt), 32 (Snow/Grass). The terrain passability is data-driven via `TerrainRestriction.PassableBuildableDmgMultiplier`. Ice is walkable by all land units despite being "water" visually.

**BLOCKER:** Need to verify that the HD Edition .dat file used by freeaoe includes ice terrain data. If terrain 26 has no passability entry, land units can't walk on it. Check at runtime by reading `DataManager::Inst().getTerrainRestriction(0).PassableBuildableDmgMultiplier[26]`.

**Tech Stack:** C++, genie .dat data

---

### Task 1: Verify ice terrain data availability

- [ ] **Step 1: Add debug logging in RandomMapGenerator**

At the start of `RandomMapGenerator::generate()`, add:
```cpp
const auto &terrainBlock = DataManager::Inst().terrainBlock();
DBG << "Total terrains:" << terrainBlock.Terrains.size();
if (terrainBlock.Terrains.size() > 26) {
    DBG << "Terrain 26 name:" << terrainBlock.Terrains[26].Name;
}
```

- [ ] **Step 2: Run game and check log**

If terrain 26 exists and is named "ICE" or similar, proceed with implementation.
If not, this module cannot be implemented without modding the .dat file.

### Task 2: Add ice placement to Baltic/Highland maps (if data exists)

- [ ] **Step 1: In generateBaltic(), replace some water with ice near edges**

In `RandomMapGenerator::generateBaltic()`, when placing water tiles near the lake edge:
```cpp
// If close to shore (within 2 tiles of beach), place ice instead of water
if (dist + edgeNoise < lakeRadius - 2) {
    tile.terrainId = 1; // Deep water
} else if (dist + edgeNoise < lakeRadius) {
    tile.terrainId = 26; // Ice (walkable frozen shoreline)
}
```

- [ ] **Step 2: Remove ice from isWaterTerrain check**

In `Map.h:144-147`, terrain ID 26 should NOT be treated as water if it's ice:
```cpp
static bool isWaterTerrain(int terrainId) {
    return terrainId == 1 || terrainId == 2 || terrainId == 3 ||
           terrainId == 4 || terrainId == 22;
    // Removed 26 — ice is walkable
}
```

**Note:** Bridges are not feasible without custom unit/building placement logic (bridges in AoE2 are placed objects, not terrain types). Skipping bridges.
