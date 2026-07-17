# Module 20: Tribute Tax & Data-Driven Rates Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make tribute tax read from the `TributeInefficiency` resource (modified by Coinage/Banking techs) instead of hardcoded 25%. Also make garrison heal rate and relic gold rate read from data instead of hardcodes.

**Architecture:** Replace hardcoded constants with reads from `Player::resourcesAvailable()` for the relevant resource types. The tech effect system already modifies these resources when techs are researched — we just need to read them.

**Tech Stack:** C++, GameState, Building, genie::ResourceType

**Verified APIs:**
- `genie::ResourceType::TributeInefficiency = 46` (confirmed in ResourceType.h)
- `genie::ResourceType::TCRelicGoldProductionRate = 191` (NOT `RelicGoldProductionRate`)
- `data()->Building.GarrisonHealRate` exists (genie/dat/unit/Building.h:119, `float GarrisonHealRate = 0`)
- `Player::resourcesAvailable(genie::ResourceType)` returns float
- `GameState::player(size_t id)` returns `shared_ptr<Player>`

---

### Task 1: Fix tribute tax to use TributeInefficiency resource

**Files:**
- Modify: `src/mechanics/GameState.cpp` (CommandType::Tribute handler, ~line 609)

**Context:** Current code:
```cpp
float received = amount * 0.75f; // 25% tribute tax
```

`genie::ResourceType::TributeInefficiency` (resource 46) defaults to 0.25 (25% tax). Coinage tech sets it to 0.15, Banking sets it to 0.0. The tech effect system modifies this resource via `ResourceModifier` commands. We just need to read it.

- [ ] **Step 1: Replace hardcoded tax with resource read**

In the `CommandType::Tribute` handler in `GameState::executeCommands()`:

```cpp
case CommandType::Tribute: {
    Player::Ptr sender = player(cmd.playerId);
    Player::Ptr receiver = player(cmd.targetPlayerId);
    if (!sender || !receiver) break;

    auto resType = static_cast<genie::ResourceType>(cmd.resourceType);
    float amount = static_cast<float>(cmd.amount);

    if (sender->resourcesAvailable(resType) >= amount) {
        sender->removeResource(resType, amount);

        // Tax rate from TributeInefficiency resource (default 0.25, reduced by Coinage/Banking)
        float taxRate = sender->resourcesAvailable(genie::ResourceType::TributeInefficiency);
        if (taxRate < 0.f) taxRate = 0.f;
        if (taxRate > 1.f) taxRate = 1.f;
        float received = amount * (1.f - taxRate);

        receiver->addResource(resType, received);
    }
    break;
}
```

- [ ] **Step 2: Verify TributeInefficiency default value**

Check that `TributeInefficiency` (resource 46) is initialized to 0.25 in the civ starting resources. If not set by the dat, add a default:

In `Player::Player()`, after loading starting resources:

```cpp
// Ensure TributeInefficiency has a default (0.25 = 25% tax)
if (m_resourcesAvailable.find(genie::ResourceType::TributeInefficiency) == m_resourcesAvailable.end()) {
    m_resourcesAvailable[genie::ResourceType::TributeInefficiency] = 0.25f;
}
```

Or check if `resourcesAvailable()` returns 0 for unset resources — in that case, 0 tax would be wrong. The default must be 0.25.

- [ ] **Step 3: Build, test, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/GameState.cpp src/mechanics/Player.cpp
git commit -m "fix: tribute tax reads TributeInefficiency resource (Coinage/Banking techs work)"
```

---

### Task 2: Fix garrison heal rate to read from data

**Files:**
- Modify: `src/mechanics/Building.cpp` (garrison healing section, ~line 364)

**Context:** Current code uses hardcoded rates:
```cpp
// TC/Towers heal at 0.1 HP/sec, Castle at 0.2 HP/sec
```

The dat stores `Building.GarrisonHealRate` per building type, and the tech effect system can modify it via `applyUnitAttributeModifier` (attribute `GarrisonHealRate`).

- [ ] **Step 1: Replace hardcoded heal rate with data read**

```cpp
// Garrison healing — rate from building data
float healRate = data()->Building.GarrisonHealRate;
if (healRate <= 0.f) {
    healRate = 0.1f; // Fallback if dat has no value
}
float healAmount = healRate * deltaTime / 1000.f;

for (auto &weakUnit : garrisonedUnits) {
    Unit::Ptr unit = weakUnit.lock();
    if (!unit || unit->isDead()) continue;
    if (unit->healthLeft() < unit->data()->HitPoints) {
        unit->takeDamage(-healAmount); // Negative = heal
    }
}
```

Verify `data()->Building.GarrisonHealRate` exists in the genie unit struct — check `genieutils/include/genie/dat/Unit.h` for the Building sub-struct field name.

- [ ] **Step 2: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/Building.cpp
git commit -m "fix: garrison heal rate reads from building data instead of hardcode"
```

---

### Task 3: Fix relic gold rate to read from data

**Files:**
- Modify: `src/mechanics/Building.cpp` (monastery relic gold section, ~line 381)

**Context:** Current code:
```cpp
float goldPerMs = 0.5f * relicCount; // Hardcoded 0.5 gold/sec
```

`genie::ResourceType::TCRelicGoldProductionRate` (resource 191) stores the per-relic gold rate. It defaults to 0.5 but can be modified by civ bonuses (e.g. Aztecs in AoE2 get more relic gold).

- [ ] **Step 1: Replace hardcoded relic gold rate**

```cpp
// Relic gold — rate from player resource (default 0.5, modifiable by civ bonus)
float goldPerRelic = owner->resourcesAvailable(genie::ResourceType::TCRelicGoldProductionRate);
if (goldPerRelic <= 0.f) {
    goldPerRelic = 0.5f; // Fallback default
}
float gold = goldPerRelic * relicCount * deltaTime / 1000.f;
```

- [ ] **Step 2: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/mechanics/Building.cpp
git commit -m "fix: relic gold rate reads from player resource (civ bonuses work)"
```
