# Technologies & Effects Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete tech system -- cost/time modifiers, disable tech, recursive dependencies, enable/disable units per civ

**Architecture:**
- `Player::applyResearch(researchId)` -- applies a tech by index into the global `Techs` vector, fires its `EffectID`, then scans all implicit techs (ResearchLocation == -1) whose requirements are now satisfied
- `Player::applyTechEffect(effectId)` -- iterates `EffectCommands` in a genie::Effect and dispatches to `applyTechEffectCommand`
- `Player::applyTechEffectCommand` -- switch on `EffectCommand::Type`; currently logs-but-ignores `TechCostModifier` (101), `DisableTech` (102), `TechTimeModifier` (103)
- `Civilization` stores `m_techs` (map of tech-index to genie::Tech) and `m_researchAvailable` (map of building-ID to tech pointers)
- `Building::enqueueProduceResearch` deducts resource costs from `tech->ResourceCosts`, stores raw `genie::Tech*`, uses `tech->ResearchTime` as production duration
- `Building::finalizeResearch` calls `owner->applyResearch(m_currentProduct->tech->EffectID)` -- **BUG**: passes EffectID instead of tech index
- `Player::setAge()` has a `TODO: need to recurse and research all dependencies` -- only applies one level of tech effects
- `Player::updateAvailableTechs()` filters `civilization.availableTechs()` by requirements but does NOT check disabled techs
- `ActionPanel::addResearchButtons` shows techs from `researchAvailableAt()` filtered by `RequiredTechCount`, but does NOT check disabled techs or already-researched techs

**Tech Stack:** C++20, SDL2

---

## Task 1: Add disabled-techs and tech-modifier storage to Player

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.h`

Add three new private members to `Player`:

```cpp
// After: std::unordered_map<int, genie::Tech> m_currentlyAvailableTechs;

// Techs disabled by civ bonuses (DisableTech effect command)
std::unordered_set<int> m_disabledTechs;

// Tech cost modifiers: key = tech index, value = map of resource-type to delta
std::unordered_map<int, ResourceMap> m_techCostModifiers;

// Tech time modifiers: key = tech index, value = time delta (added to ResearchTime)
std::unordered_map<int, float> m_techTimeModifiers;
```

Add two public query methods:

```cpp
bool isTechDisabled(const int techId) const { return m_disabledTechs.count(techId); }
float techTimeModifier(const int techId) const {
    auto it = m_techTimeModifiers.find(techId);
    return it != m_techTimeModifiers.end() ? it->second : 0.f;
}
const ResourceMap *techCostModifier(const int techId) const {
    auto it = m_techCostModifiers.find(techId);
    return it != m_techCostModifiers.end() ? &it->second : nullptr;
}
```

**Commit:** `feat: add disabled-tech and tech-modifier storage to Player`

---

## Task 2: Implement TechCostModifier effect

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

In `applyTechEffectCommand`, replace the `TechCostModifier` case (lines 157-159):

```cpp
case genie::EffectCommand::TechCostModifier: {
    // TargetUnit = tech index, UnitClassID = resource type, Amount = cost delta
    const int techId = effect.TargetUnit;
    const genie::ResourceType resType = genie::ResourceType(effect.UnitClassID);
    if (effect.AttributeID == 0) {
        // Mode 0: set absolute
        m_techCostModifiers[techId][resType] = effect.Amount;
    } else {
        // Mode 1: add relative
        m_techCostModifiers[techId][resType] += effect.Amount;
    }
    DBG << "Tech cost modifier: tech" << techId << "resource" << int(resType) << "delta" << effect.Amount;
    break;
}
```

**Commit:** `feat: apply TechCostModifier effect commands`

---

## Task 3: Implement TechTimeModifier effect

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

Replace the `TechTimeModifier` case (lines 163-165):

```cpp
case genie::EffectCommand::TechTimeModifier: {
    // TargetUnit = tech index, Amount = time delta
    const int techId = effect.TargetUnit;
    if (effect.AttributeID == 0) {
        m_techTimeModifiers[techId] = effect.Amount;
    } else {
        m_techTimeModifiers[techId] += effect.Amount;
    }
    DBG << "Tech time modifier: tech" << techId << "delta" << effect.Amount;
    break;
}
```

**Commit:** `feat: apply TechTimeModifier effect commands`

---

## Task 4: Implement DisableTech effect

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

Replace the `DisableTech` case (lines 160-162):

```cpp
case genie::EffectCommand::DisableTech: {
    const int techId = effect.TargetUnit;
    m_disabledTechs.insert(techId);
    // Remove from currently available if present
    m_currentlyAvailableTechs.erase(techId);
    DBG << "Disabled tech" << techId;
    break;
}
```

**Commit:** `feat: apply DisableTech effect commands`

---

## Task 5: Filter disabled techs from updateAvailableTechs

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

In `updateAvailableTechs()` (line 476-504), add a disabled-tech check after the `m_activeTechs.count` check. Insert after line 484:

```cpp
    if (m_disabledTechs.count(it->first)) {
        continue;
    }
```

The full loop body becomes:

```cpp
for (it = civilization.availableTechs().begin(); it != civilization.availableTechs().end(); it++) {
    if (m_activeTechs.count(it->first)) {
        continue;
    }

    if (m_disabledTechs.count(it->first)) {
        continue;
    }

    bool requirementsSatisfied = false;
    // ... rest unchanged
```

**Commit:** `feat: filter disabled techs from available tech list`

---

## Task 6: Apply cost modifiers when enqueuing research

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.cpp`

In `enqueueProduceResearch` (line 144), the tech index is needed but currently the method only receives a `genie::Tech*` pointer. We need to find the tech index. The Civilization's `m_techs` map key is the index, and `researchAvailableAt` returns pointers into that map. We need to pass the tech index through.

**Step 6a:** Add a `techIndex` field to `Product` struct in Building.h:

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.h`

In the `Product` struct (line 66), add:

```cpp
struct Product {
    enum {
        Unit,
        Research
    } type;

    ResourceMap cost;
    int techIndex = -1; // index into global Techs vector (for cost/time modifiers)

    union {
        const genie::Unit *unit = nullptr;
        const genie::Tech *tech;
    };
};
```

**Step 6b:** Change `enqueueProduceResearch` signature to also accept the tech index:

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.h`

```cpp
bool enqueueProduceResearch(const genie::Tech *data, int techIndex = -1) noexcept;
```

**Step 6c:** Update `enqueueProduceResearch` implementation to apply cost modifiers:

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.cpp`

Replace lines 144-190:

```cpp
bool Building::enqueueProduceResearch(const genie::Tech *data, int techIndex) noexcept
{
    if (!data) {
        WARN << "trying to enqueue null unit";
        return false;
    }

    Player::Ptr owner = player().lock();
    if (!owner) {
        WARN << "building owner went away";
        return false;
    }

    // Build effective costs (base + modifier)
    const ResourceMap *costMod = (techIndex >= 0) ? owner->techCostModifier(techIndex) : nullptr;

    for (const genie::Resource<int16_t, int8_t> &cost : data->ResourceCosts) {
        if (!cost.Paid) {
            continue;
        }

        const genie::ResourceType type = genie::ResourceType(cost.Type);
        float effectiveCost = cost.Amount;
        if (costMod) {
            auto modIt = costMod->find(type);
            if (modIt != costMod->end()) {
                effectiveCost += modIt->second;
            }
        }
        if (effectiveCost < 0) effectiveCost = 0;
        if (owner->resourcesAvailable(type) < effectiveCost) {
            return false;
        }
    }

    std::unique_ptr<Product> product = std::make_unique<Product>();
    product->type = Product::Research;
    product->tech = data;
    product->techIndex = techIndex;

    for (const genie::Resource<int16_t, int8_t> &r : data->ResourceCosts) {
        if (!r.Paid) {
            continue;
        }

        const genie::ResourceType type = genie::ResourceType(r.Type);
        float effectiveCost = r.Amount;
        if (costMod) {
            auto modIt = costMod->find(type);
            if (modIt != costMod->end()) {
                effectiveCost += modIt->second;
            }
        }
        if (effectiveCost < 0) effectiveCost = 0;

        owner->removeResource(type, effectiveCost);
        product->cost[type] = effectiveCost;
    }

    m_productionQueue.push_back(std::move(product));

    if (!m_currentProduct) {
        attemptStartProduction();
    }

    return true;
}
```

**Commit:** `feat: apply tech cost modifiers when enqueuing research`

---

## Task 7: Apply time modifiers during research production

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.cpp`

Wherever `ResearchTime` is used as the production duration, add the time modifier. There are two places:

**7a:** `productionProgress()` (line 233):

```cpp
} else {
    maximum = m_currentProduct->tech->ResearchTime;
    if (m_currentProduct->techIndex >= 0) {
        Player::Ptr owner = player().lock();
        if (owner) {
            maximum += owner->techTimeModifier(m_currentProduct->techIndex);
        }
    }
    if (maximum < 1.f) maximum = 1.f;
}
```

**7b:** `update()` (line 336):

```cpp
} else {
    productionTime = m_currentProduct->tech->ResearchTime;
    if (m_currentProduct->techIndex >= 0) {
        Player::Ptr owner = player().lock();
        if (owner) {
            productionTime += owner->techTimeModifier(m_currentProduct->techIndex);
        }
    }
    if (productionTime < 1.f) productionTime = 1.f;
}
```

**Commit:** `feat: apply tech time modifiers during research production`

---

## Task 8: Fix finalizeResearch passing EffectID instead of tech index

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.cpp`

`finalizeResearch()` at line 494 currently calls:
```cpp
owner->applyResearch(m_currentProduct->tech->EffectID);
```

This is wrong. `applyResearch` expects the tech index (position in the global Techs vector), not the EffectID. Use the stored `techIndex`:

```cpp
void Building::finalizeResearch() noexcept
{
    Player::Ptr owner = player().lock();
    if (!owner) {
        WARN << "building owner went away";
        return;
    }

    if (m_currentProduct->techIndex >= 0) {
        owner->applyResearch(m_currentProduct->techIndex);
    } else {
        // Fallback: apply just the effect directly
        owner->applyTechEffect(m_currentProduct->tech->EffectID);
    }
}
```

**Commit:** `fix: finalizeResearch passes tech index instead of EffectID`

---

## Task 9: Pass tech index from all call sites

Several places call `enqueueProduceResearch` and need to pass the tech index.

**9a: ActionPanel / UnitManager (human player research)**

**File:** `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp`

Find the call to `enqueueProduceResearch` (line 1174). The tech pointer comes from `researchAvailableAt`. We need to find the tech index. The `Civilization::m_techs` map key is the index. Add a method to Civilization to look up the index:

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Civilization.h`

Add public method:

```cpp
int techIndex(const genie::Tech *tech) const {
    for (const auto &[idx, t] : m_techs) {
        if (&t == tech) return idx;
    }
    return -1;
}
```

Then update the UnitManager call site. Search for the `enqueueProduceResearch` call and find how `techData` is obtained. Add the index lookup:

**File:** `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp`

At the call site around line 1174, change:
```cpp
producer->enqueueProduceResearch(techData);
```
to:
```cpp
int techIdx = player->civilization.techIndex(techData);
producer->enqueueProduceResearch(techData, techIdx);
```

**9b: AI call sites**

**File:** `/home/dima/Projects/freeaoe/src/ai/actions/Actions.cpp` (line 376)

Change:
```cpp
building->enqueueProduceResearch(tech);
```
to:
```cpp
building->enqueueProduceResearch(tech, m_player->civilization.techIndex(tech));
```

**File:** `/home/dima/Projects/freeaoe/src/ai/BasicAI.cpp` (lines 284, 307, 558)

At each `enqueueProduceResearch` call, pass the tech index. For example at line 284:
```cpp
building->enqueueProduceResearch(&loom, m_player->civilization.techIndex(&loom));
```

At line 307 (inside the research loop):
```cpp
building->enqueueProduceResearch(&tech, m_player->civilization.techIndex(&tech));
```

At line 558:
```cpp
building->enqueueProduceResearch(&tech, m_player->civilization.techIndex(&tech));
```

**Commit:** `feat: pass tech index through all enqueueProduceResearch call sites`

---

## Task 10: Recursive tech dependencies in setAge

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

Replace `setAge()` (lines 173-198) to recursively apply all implicit tech dependencies, not just one level:

```cpp
void Player::setAge(const Age age)
{
    m_resourcesAvailable[genie::ResourceType::CurrentAge] = age;

    genie::ResourceType effectResourceType;
    switch (age) {
    case DarkAge:
        effectResourceType = genie::ResourceType::DarkAgeTechID;
        break;
    case FeudalAge:
        effectResourceType = genie::ResourceType::FeudalAgeTechID;
        break;
    case CastleAge:
        effectResourceType = genie::ResourceType::CastleAgeTechID;
        break;
    case ImperialAge:
        effectResourceType = genie::ResourceType::ImperialAgeTechID;
        break;
    default:
        WARN << "Invalid age";
        return;
    }

    applyTechEffect(civilization.startingResource(effectResourceType));

    // Recursively apply all implicit techs whose dependencies are now met
    bool anyApplied = true;
    while (anyApplied) {
        anyApplied = false;
        for (const genie::Tech &research : DataManager::Inst().allTechs()) {
            if (research.ResearchLocation != -1) {
                continue;
            }
            if (research.EffectID == -1) {
                continue;
            }
            if (research.Civ != -1 && research.Civ != civilization.id()) {
                continue;
            }
            if (m_activeTechs.count(research.EffectID)) {
                continue;
            }

            bool requirementsSatisfied = false;
            for (const int reqId : research.RequiredTechs) {
                if (reqId == -1) {
                    continue;
                }
                if (m_activeTechs.count(reqId)) {
                    requirementsSatisfied = true;
                } else {
                    requirementsSatisfied = false;
                    break;
                }
            }

            if (!requirementsSatisfied) {
                continue;
            }

            applyTechEffect(research.EffectID);
            anyApplied = true;
        }
    }

    updateAvailableTechs();
}
```

**Commit:** `feat: recursive tech dependency resolution in setAge`

---

## Task 11: Filter disabled and already-researched techs from ActionPanel

**File:** `/home/dima/Projects/freeaoe/src/ui/ActionPanel.cpp`

In `addResearchButtons` (around line 561), after the `RequiredTechCount` check and before creating the button, add:

```cpp
for (const genie::Tech *tech : techs) {
    if (tech->ButtonID > m_buttonOffset + 15) {
        hasNext = true;
        continue;
    }

    // Find tech index for disabled/researched checks
    int techIdx = player->civilization.techIndex(tech);

    // Skip disabled techs
    if (techIdx >= 0 && player->isTechDisabled(techIdx)) {
        continue;
    }

    // Skip already-researched techs
    if (techIdx >= 0 && player->hasResearched(techIdx)) {
        continue;
    }

    // Skip techs whose prerequisites aren't met (e.g. Castle Age before Feudal)
    if (tech->RequiredTechCount > 0) {
        int satisfied = 0;
        for (int16_t reqId : tech->RequiredTechs) {
            if (reqId == -1) continue;
            if (player->hasResearched(reqId)) satisfied++;
        }
        if (satisfied < tech->RequiredTechCount) continue;
    }

    InterfaceButton button;
    // ... rest unchanged
```

**Commit:** `feat: hide disabled and already-researched techs from action panel`

---

## Task 12: Apply cost modifiers in canAffordResearch

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

Update `canAffordResearch` to account for cost modifiers. Change the signature to accept a tech index:

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.h`

```cpp
bool canAffordResearch(const int researchId) const;
```

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Player.cpp`

Replace `canAffordResearch` (lines 200-211):

```cpp
bool Player::canAffordResearch(const int researchId) const
{
    const genie::Tech &research = DataManager::Inst().getTech(researchId);
    const ResourceMap *costMod = techCostModifier(researchId);

    for (const genie::Tech::ResearchResourceCost &cost : research.ResourceCosts) {
        const genie::ResourceType resourceType = genie::ResourceType(cost.Type);
        float effectiveCost = cost.Amount;
        if (costMod) {
            auto modIt = costMod->find(resourceType);
            if (modIt != costMod->end()) {
                effectiveCost += modIt->second;
            }
        }
        if (effectiveCost < 0) effectiveCost = 0;
        if (resourcesAvailable(resourceType) < effectiveCost) {
            return false;
        }
    }

    return true;
}
```

**Commit:** `feat: account for tech cost modifiers in canAffordResearch`

---

## Task 13: Build and smoke test

Run:
```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Fix any compilation errors. Then run the game and:
1. Start a standard game as any civ
2. Research Loom at the Town Center -- verify it completes and costs are deducted
3. Advance to Feudal Age -- verify implicit techs fire (e.g., new units appear at Barracks)
4. Check that civ-disabled techs (e.g., Britons can't research certain techs) don't appear in buildings

**Commit:** (only if fixes are needed) `fix: compilation fixes for tech system`

---

## Summary of files modified

| File | Changes |
|---|---|
| `src/mechanics/Player.h` | Add `m_disabledTechs`, `m_techCostModifiers`, `m_techTimeModifiers` + query methods |
| `src/mechanics/Player.cpp` | Implement TechCostModifier/TechTimeModifier/DisableTech effects; fix canAffordResearch; recursive setAge |
| `src/mechanics/Civilization.h` | Add `techIndex()` lookup method |
| `src/mechanics/Building.h` | Add `techIndex` to Product; update enqueueProduceResearch signature |
| `src/mechanics/Building.cpp` | Apply cost/time modifiers; fix finalizeResearch to use tech index |
| `src/mechanics/UnitManager.cpp` | Pass tech index to enqueueProduceResearch |
| `src/ai/actions/Actions.cpp` | Pass tech index to enqueueProduceResearch |
| `src/ai/BasicAI.cpp` | Pass tech index to enqueueProduceResearch (3 call sites) |
| `src/ui/ActionPanel.cpp` | Filter disabled/researched techs from research buttons |
