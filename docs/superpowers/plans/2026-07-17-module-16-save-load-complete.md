# Module 16: Save/Load Completeness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend save/load to preserve: researched techs, diplomacy stances, market prices, garrison state, building construction progress.

**Architecture:** Bump `SaveGame::VERSION` to 3. Add new sections after the existing unit data block. Load code checks version before reading new sections.

**Tech Stack:** C++, SaveGame, Player, Building, binary I/O

**Verified APIs:**
- SaveGame uses `std::ofstream`/`std::ifstream` with free helper functions:
  - `writeU32(file, val)`, `writeI32(file, val)`, `writeFloat(file, val)`
  - `readU32(file)`, `readI32(file)`, `readFloat(file)`
- `Player::hasResearched(int researchId)` exists, but no `researchedTechs()` getter — need to add one
- `Player::m_researchedTechs` is `std::unordered_set<int>` (private)
- `Player::diplomaticStanceTo(uint8_t playerId)` returns `DiplomaticStance`
- `Player::setDiplomaticStance()` — verify exact signature
- `Player::marketPrices` has `int basePrice[3]`
- `Player::applyResearch(int researchId)` re-applies a tech
- `Building` is accessed via `dynamic_cast<Building*>(unit.get())`

---

### Task 1: Add researchedTechs() getter to Player

**Files:**
- Modify: `src/mechanics/Player.h` (add public getter)

- [ ] **Step 1: Add getter**

In `Player.h`, in the public section near `hasResearched()`:

```cpp
const std::unordered_set<int> &researchedTechs() const { return m_researchedTechs; }
```

- [ ] **Step 2: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/Player.h
git commit -m "feat: expose researchedTechs() getter for save/load"
```

---

### Task 2: Save/load researched techs (bump to VERSION 3)

**Files:**
- Modify: `src/mechanics/SaveGame.cpp`

- [ ] **Step 1: Bump VERSION**

Change:
```cpp
static constexpr uint32_t VERSION = 3;
```

- [ ] **Step 2: Save researched techs after units block**

In `SaveGame::save()`, after the unit-writing loop ends, add:

```cpp
// === Researched Techs Section (v3+) ===
const auto &allPlayers = state->players();
writeU32(file, allPlayers.size() - 1); // exclude Gaia
for (size_t p = 1; p < allPlayers.size(); p++) {
    const auto &player = allPlayers[p];
    if (!player) {
        writeU32(file, 0);
        continue;
    }
    const auto &techs = player->researchedTechs();
    writeU32(file, techs.size());
    for (int techId : techs) {
        writeI32(file, techId);
    }
}
```

- [ ] **Step 3: Load researched techs**

In `SaveGame::load()`, after loading units, add:

```cpp
if (version >= 3) {
    // === Researched Techs Section ===
    uint32_t playerCountForTechs = readU32(file);
    for (uint32_t p = 0; p < playerCountForTechs; p++) {
        auto player = state->player(p + 1);
        uint32_t techCount = readU32(file);
        for (uint32_t t = 0; t < techCount; t++) {
            int32_t techId = readI32(file);
            if (player) {
                player->applyResearch(techId);
            }
        }
    }
}
```

- [ ] **Step 4: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/SaveGame.cpp
git commit -m "feat: save/load researched technologies (save format v3)"
```

---

### Task 3: Save/load diplomacy stances

**Files:**
- Modify: `src/mechanics/SaveGame.cpp`

- [ ] **Step 1: Save diplomacy after techs block**

```cpp
// === Diplomacy Section (v3+) ===
for (size_t p = 1; p < allPlayers.size(); p++) {
    const auto &player = allPlayers[p];
    for (size_t other = 1; other < allPlayers.size(); other++) {
        if (other == p || !player) {
            writeI32(file, 0); // Neutral placeholder
            continue;
        }
        int stance = static_cast<int>(player->diplomaticStanceTo(other));
        writeI32(file, stance);
    }
}
```

- [ ] **Step 2: Load diplomacy (inside version >= 3 block)**

```cpp
// === Diplomacy Section ===
for (size_t p = 1; p < allPlayers.size(); p++) {
    auto player = state->player(p);
    for (size_t other = 1; other < allPlayers.size(); other++) {
        int32_t stance = readI32(file);
        if (player && other != p) {
            player->setDiplomaticStance(other,
                static_cast<Player::DiplomaticStance>(stance));
        }
    }
}
```

Verify `setDiplomaticStance` signature by reading `Player.h` — it may take `(uint8_t playerId, DiplomaticStance)` or `(int, DiplomaticStance)`.

- [ ] **Step 3: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/SaveGame.cpp
git commit -m "feat: save/load diplomacy stances"
```

---

### Task 4: Save/load market prices

**Files:**
- Modify: `src/mechanics/SaveGame.cpp`

- [ ] **Step 1: Save market prices after diplomacy**

```cpp
// === Market Prices Section (v3+) ===
for (size_t p = 1; p < allPlayers.size(); p++) {
    const auto &player = allPlayers[p];
    if (!player) {
        writeI32(file, 100); writeI32(file, 100); writeI32(file, 100);
        continue;
    }
    writeI32(file, player->marketPrices.basePrice[0]);
    writeI32(file, player->marketPrices.basePrice[1]);
    writeI32(file, player->marketPrices.basePrice[2]);
}
```

- [ ] **Step 2: Load market prices**

```cpp
// === Market Prices Section ===
for (size_t p = 1; p < allPlayers.size(); p++) {
    auto player = state->player(p);
    int32_t food = readI32(file);
    int32_t wood = readI32(file);
    int32_t stone = readI32(file);
    if (player) {
        player->marketPrices.basePrice[0] = food;
        player->marketPrices.basePrice[1] = wood;
        player->marketPrices.basePrice[2] = stone;
    }
}
```

- [ ] **Step 3: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/SaveGame.cpp
git commit -m "feat: save/load market prices"
```

---

### Task 5: Save/load building construction progress

**Files:**
- Modify: `src/mechanics/SaveGame.cpp`

**Context:** Currently `setCreationProgress(1.f)` is called on all loaded buildings (marks them complete). We need to save and restore the actual progress value.

- [ ] **Step 1: Save construction progress per unit**

In the unit save loop, after writing `angle` (`writeFloat(file, unit->angle())`), add:

```cpp
// Write construction progress (1.0 = complete, 0.0-0.99 = under construction)
Building *building = dynamic_cast<Building *>(unit.get());
if (building) {
    writeFloat(file, building->constructionProgress());
} else {
    writeFloat(file, 1.0f);
}
```

- [ ] **Step 2: Load construction progress**

In the unit load loop, after restoring HP, add:

```cpp
if (version >= 3) {
    float progress = readFloat(file);
    Building *building = dynamic_cast<Building *>(unit.get());
    if (building) {
        building->setCreationProgress(progress);
    }
} else {
    // v2 behavior: all buildings marked complete
    unit->setCreationProgress(1.f);
}
```

Remove the existing `setCreationProgress(1.f)` call that unconditionally marks all buildings complete.

- [ ] **Step 3: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/SaveGame.cpp
git commit -m "feat: save/load building construction progress"
```
