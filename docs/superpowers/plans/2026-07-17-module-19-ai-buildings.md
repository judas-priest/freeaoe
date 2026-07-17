# Module 19: AI Buildings (Castle, Blacksmith, University) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** AI builds Blacksmith, University, Castle, and a second Town Center. This enables key techs (attack/armor upgrades, ballistics, chemistry) and unique units from Castle.

**Architecture:** Add `buildBlacksmith()`, `buildUniversity()`, `buildCastle()`, `buildExtraTownCenter()` methods to BasicAI. Each follows the existing `buildStructure()` pattern. Called from `update()` at appropriate ages. Building IDs from AoE2 dat.

**Tech Stack:** C++, BasicAI, genie unit IDs

**Verified APIs:**
- `buildStructure(int buildingId, int woodCost)` — wood-only buildings
- `buildStructureWithCost(int buildingId, int woodCost, int stoneCost)` — multi-resource buildings
- `countBuildingsOfType(int id)` exists
- Building IDs: Blacksmith=103, University=209, Castle=82, TC=109
- Blacksmith cost: 150W. University cost: 200W. Castle cost: 650S. TC cost: 275W + 100S.

---

### Task 1: AI builds Blacksmith

**Files:**
- Modify: `src/ai/BasicAI.h` (add method)
- Modify: `src/ai/BasicAI.cpp` (implement + call from update)

**Context:** Blacksmith ID = 103. Available from Feudal Age. Costs 150 wood. Required for all attack/armor upgrades. Currently `researchTechs()` tries to research Blacksmith techs but never builds the Blacksmith building.

- [ ] **Step 1: Add method declaration**

In `BasicAI.h` private section:

```cpp
void buildBlacksmith();
void buildUniversity();
void buildCastle();
void buildExtraTownCenter();
```

- [ ] **Step 2: Implement buildBlacksmith()**

```cpp
void BasicAI::buildBlacksmith()
{
    // Build in Feudal Age, one is enough
    Player::Ptr player = m_player->player();
    if (player->currentAge() < Player::Age::FeudalAge) {
        return;
    }
    if (countBuildingsOfType(103) > 0) {
        return;
    }
    if (player->resourcesAvailable(genie::ResourceType::WoodStorage) < 150) {
        return;
    }
    buildStructure(103, 150);
}
```

- [ ] **Step 3: Call from update()**

In `BasicAI::update()`, add after `buildDefenses()`:

```cpp
buildBlacksmith();
```

- [ ] **Step 4: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/ai/BasicAI.h src/ai/BasicAI.cpp
git commit -m "feat: AI builds Blacksmith in Feudal Age for attack/armor upgrades"
```

---

### Task 2: AI builds University

**Files:**
- Modify: `src/ai/BasicAI.cpp`

**Context:** University ID = 209. Available from Castle Age. Costs 200 wood. Enables Ballistics, Chemistry, Architecture, Murder Holes, etc.

- [ ] **Step 1: Implement buildUniversity()**

```cpp
void BasicAI::buildUniversity()
{
    Player::Ptr player = m_player->player();
    if (player->currentAge() < Player::Age::CastleAge) {
        return;
    }
    if (countBuildingsOfType(209) > 0) {
        return;
    }
    if (player->resourcesAvailable(genie::ResourceType::WoodStorage) < 200) {
        return;
    }
    buildStructure(209, 200);
}
```

- [ ] **Step 2: Add University techs to researchTechs()**

Extend `priorityTechs[]` array with University techs:

```cpp
// University techs (Castle Age+)
{93, 209},  // Ballistics
{51, 209},  // Architecture
{380, 209}, // Heated Shot
```

Or simply add these tech IDs to the existing priority list — `researchTechs()` already finds the building by `tech.ResearchLocation`.

- [ ] **Step 3: Call from update(), build, commit**

```cpp
buildUniversity();
```

```bash
cd build && make -j$(nproc)
git add src/ai/BasicAI.cpp
git commit -m "feat: AI builds University and researches Ballistics/Architecture"
```

---

### Task 3: AI builds Castle

**Files:**
- Modify: `src/ai/BasicAI.cpp`

**Context:** Castle ID = 82. Available from Castle Age. Costs 650 stone. AI needs this for unique units and as a defensive structure. Only build if AI has excess stone.

- [ ] **Step 1: Implement buildCastle()**

```cpp
void BasicAI::buildCastle()
{
    Player::Ptr player = m_player->player();
    if (player->currentAge() < Player::Age::CastleAge) {
        return;
    }
    // Max 2 castles
    if (countBuildingsOfType(82) >= 2) {
        return;
    }
    if (player->resourcesAvailable(genie::ResourceType::StoneStorage) < 650) {
        return;
    }
    buildStructureWithCost(82, 0, 650); // Castle costs 650 stone, 0 wood
}
```

- [ ] **Step 2: Train unique units from Castle**

In `trainMilitary()`, add a block for Castle units. The unique unit ID depends on civilization. Get it from the dat:

```cpp
// Train unique unit from Castle if it exists
if (countBuildingsOfType(82) > 0 && m_params.researchTechs) {
    // Find any trainable unit at Castle (building 82)
    const auto &creatableAtCastle = player->civilization.creatableUnits(82);
    for (int unitId : creatableAtCastle) {
        // Skip Petards (ID 440) and Trebuchets (ID 331) — focus on unique unit
        if (unitId == 440 || unitId == 331) continue;
        trainUnit(82, unitId);
        break; // Train one type
    }
}
```

Adapt to match the actual `creatableUnits()` API.

- [ ] **Step 3: Call from update(), build, commit**

```cpp
buildCastle();
```

```bash
cd build && make -j$(nproc)
git add src/ai/BasicAI.cpp
git commit -m "feat: AI builds Castle and trains unique units"
```

---

### Task 4: AI builds a second Town Center

**Files:**
- Modify: `src/ai/BasicAI.cpp`

**Context:** Second TC provides extra pop space (5), faster villager production, and garrison safety. Available from Castle Age (requires researching the "Town Center" tech or just building if age permits). Cost: 275W + 100S.

- [ ] **Step 1: Implement buildExtraTownCenter()**

```cpp
void BasicAI::buildExtraTownCenter()
{
    Player::Ptr player = m_player->player();
    if (player->currentAge() < Player::Age::CastleAge) {
        return;
    }
    // Max 3 TCs total
    if (countBuildingsOfType(109) >= 3) {
        return;
    }
    if (player->resourcesAvailable(genie::ResourceType::WoodStorage) < 275 ||
        player->resourcesAvailable(genie::ResourceType::StoneStorage) < 100) {
        return;
    }

    // Boom strategy prioritizes extra TCs
    if (m_strategy != Strategy::Boom && countBuildingsOfType(109) >= 2) {
        return;
    }

    buildStructureWithCost(109, 275, 100); // TC costs 275 wood + 100 stone
}

- [ ] **Step 2: Call from update(), build, commit**

```cpp
buildExtraTownCenter();
```

```bash
cd build && make -j$(nproc)
git add src/ai/BasicAI.cpp
git commit -m "feat: AI builds extra Town Centers in Castle Age (Boom strategy)"
```
