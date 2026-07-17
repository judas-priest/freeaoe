# Module 12: AI Farm Economy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** AI builds farms when natural food sources are depleted, builds a Mill, and assigns villagers to farm. Without this, AI starves after berries/deer run out.

**Architecture:** Add `buildFarms()` and `buildMill()` methods to BasicAI. `buildMill()` triggers in Dark Age near berries. `buildFarms()` triggers when idle food villagers exist and no natural food nearby. Uses existing `buildStructure()` and `assignTask()` patterns from BasicAI.

**Tech Stack:** C++, BasicAI, UnitManager, genie unit IDs

**Verified APIs:**
- `buildStructure(int buildingId, int woodCost)` — takes ID + wood cost only, finds position internally near TC via spiral search
- `buildStructureWithCost(int buildingId, int woodCost, int stoneCost)` — variant for multi-resource buildings
- `countBuildingsOfType(int id)` and `countUnitsOfType(int id)` exist
- `Player::resourcesAvailable(genie::ResourceType)` for resource checks
- `Player::currentAge()` returns `Player::Age` enum
- `m_player->player()` returns `Player::Ptr`
- Unit IDs: Farm=50, Mill=68, Berry Bush (Gaia)=59
- AI update pattern: methods called from `BasicAI::update()` in sequence

---

### Task 1: Add buildMill() to BasicAI

**Files:**
- Modify: `src/ai/BasicAI.h` (add method declarations)
- Modify: `src/ai/BasicAI.cpp` (add implementations + call from update)

**Context:** BasicAI already has `buildDropOffSites()` (line 123 in update) which builds Lumber Camp (ID 562) and Mining Camp (ID 584). Mill (ID 68) is missing. Mill is a food drop-off point and should be built near berry bushes. `buildStructure(unitId, resourceCost)` is the existing pattern — it finds a position near TC using spiral search and creates a Build command.

- [ ] **Step 1: Add method declarations to BasicAI.h**

Add in the private section:

```cpp
void buildMill();
void buildFarms();
```

- [ ] **Step 2: Implement buildMill() in BasicAI.cpp**

Add after `buildDropOffSites()`:

```cpp
void BasicAI::buildMill()
{
    // Only build one Mill
    if (countBuildingsOfType(68) > 0) {
        return;
    }

    // Need at least 100 wood
    Player::Ptr player = m_player->player();
    if (player->resourcesAvailable(genie::ResourceType::WoodStorage) < 100) {
        return;
    }

    // Find nearest berry bush to TC
    MapPos tcPos;
    bool hasTc = false;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (unit && unit->playerId() == player->playerId() && unit->data()->ID == 109) {
            tcPos = unit->position();
            hasTc = true;
            break;
        }
    }
    if (!hasTc) {
        return;
    }

    // Find nearest berry bush (ID 59)
    MapPos bestBerry;
    float bestDist = std::numeric_limits<float>::max();
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != 0) { // Gaia
            continue;
        }
        if (unit->data()->ID != 59) { // Berry bush
            continue;
        }
        float dist = unit->position().distance(tcPos);
        if (dist < bestDist) {
            bestDist = dist;
            bestBerry = unit->position();
        }
    }

    if (bestDist < std::numeric_limits<float>::max()) {
        // Build mill (100 wood) — buildStructure finds position near TC via spiral search
        buildStructure(68, 100);
    }
}
```

- [ ] **Step 3: Implement buildFarms() in BasicAI.cpp**

```cpp
void BasicAI::buildFarms()
{
    Player::Ptr player = m_player->player();

    // Need Mill or TC as food drop-off
    bool hasDropOff = countBuildingsOfType(68) > 0 || countBuildingsOfType(109) > 0;
    if (!hasDropOff) {
        return;
    }

    // Count existing farms
    int farmCount = countBuildingsOfType(50);

    // Count food gatherers (villagers currently gathering food)
    int foodVillagers = 0;
    int idleFoodVillagers = 0;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != player->playerId()) {
            continue;
        }
        if (unit->data()->ID != 83 && unit->data()->ID != 293) { // Male/female villager
            continue;
        }
        // Check if idle (no current action or action completed)
        if (unit->actions.currentAction() == nullptr) {
            idleFoodVillagers++;
        }
    }

    // Build farms when:
    // 1. We have idle villagers AND
    // 2. Natural food is scarce (few berry/deer/sheep left near TC) AND
    // 3. We have wood (60 per farm)
    // Simple heuristic: build farms once we have 6+ villagers and some are idle
    int totalVillagers = countUnitsOfType(83) + countUnitsOfType(293);
    if (totalVillagers < 6) {
        return;
    }

    // Target: roughly 40% of villagers on farms
    int targetFarms = std::max(4, totalVillagers * 2 / 5);
    if (farmCount >= targetFarms) {
        return;
    }

    if (player->resourcesAvailable(genie::ResourceType::WoodStorage) < 60) {
        return;
    }

    // Build farm near TC
    buildStructure(50, 60);
}
```

- [ ] **Step 4: Call both methods from update()**

In `BasicAI::update()`, add after `buildDropOffSites()` (line 123):

```cpp
buildMill();
buildFarms();
```

- [ ] **Step 5: Build and test**

```bash
cd build && make -j$(nproc)
```

Expected: compiles. Run a random map game vs AI, observe AI building farms after ~10 minutes when berries run out.

- [ ] **Step 6: Commit**

```bash
git add src/ai/BasicAI.h src/ai/BasicAI.cpp
git commit -m "feat: AI builds Mill near berries and Farms when natural food depleted"
```

---

### Task 2: Assign villagers to farms

**Files:**
- Modify: `src/ai/BasicAI.cpp` (modify `assignIdleVillagers`)

**Context:** `assignIdleVillagers()` currently finds the nearest Gaia gatherable resource and sends idle villagers there. It doesn't know about farms. Farms are player-owned (not Gaia) and use a different gather task (ActionType::GatherRebuild or similar). The villager needs to be assigned to gather from the farm unit.

- [ ] **Step 1: Extend assignIdleVillagers to handle farms**

In `assignIdleVillagers()`, after the existing logic that looks for Gaia resources, add a fallback:

```cpp
// If no natural food found, look for own farms
if (!foundTarget) {
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != player->playerId()) {
            continue;
        }
        if (unit->data()->ID != 50) { // Farm
            continue;
        }
        // Check farm has food left
        if (unit->resources[genie::ResourceType::FoodStorage] <= 0) {
            continue;
        }
        // Check farm not already occupied (another villager gathering from it)
        bool occupied = false;
        for (const Unit::Ptr &other : m_unitManager->units()) {
            if (!other || other == idleVillager) continue;
            if (other->playerId() != player->playerId()) continue;
            // Check if other villager is targeting this farm
            if (other->actions.currentAction() &&
                other->actions.currentAction()->targetUnit() == unit) {
                occupied = true;
                break;
            }
        }
        if (!occupied) {
            // Assign villager to gather from this farm
            m_unitManager->assignTask(idleVillager, unit);
            foundTarget = true;
            break;
        }
    }
}
```

Adapt to match the actual `assignIdleVillagers()` code structure — the key pattern is: find an unoccupied farm, call the same task-assignment that right-clicking on a resource would trigger.

- [ ] **Step 2: Build and test**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 3: Commit**

```bash
git add src/ai/BasicAI.cpp
git commit -m "feat: AI assigns idle villagers to farms when no natural food available"
```
