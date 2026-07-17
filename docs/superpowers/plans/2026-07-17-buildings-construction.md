# Buildings & Construction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete buildings -- pop cap enforcement, town bell UI buttons, repair cost deduction, production queue limit (5), garrison arrow bonus for towers, building annex positioning

**Architecture:**
- `Building` (src/mechanics/Building.h/cpp) extends `Unit` -- has `m_productionQueue` (unlimited vector of `Product`), `garrisonedUnits` (weak_ptr vector), garrison/ungarrison, production progress timer
- `Player` (src/mechanics/Player.h/cpp) tracks resources via `m_resourcesAvailable` map keyed by `genie::ResourceType`. Population uses `PopulationHeadroom` (housing capacity) and `CurrentPopulation` (used slots). `canAffordUnit()` checks resource costs but the comment says "also checks housing TODO" -- however the actual code only checks `resourcesNeeded()` vs available, which includes `ResourceStorages` entries (housing) but does NOT explicitly block when pop >= cap
- `ActionRepair` (src/actions/ActionRepair.h/cpp) heals 1 HP/sec via `takeDamage(-1)` but never deducts resources from the repairer's player
- `ActionPanel` (src/ui/ActionPanel.cpp) handles button creation in `updateButtons()` but never adds Town Bell / Ungarrison / Abort Town Bell buttons for buildings. The click handlers exist already (lines 856-998)
- Garrison arrows already work in `ActionAttack::missilesUnitCanFire()` (line 273) -- calculates `extraArrows = (max - base) * garrisoned / GarrisonCapacity`. This works for any building with `GarrisonCapacity > 0` including towers. No changes needed here.
- Annexes already created in `UnitFactory::createUnit()` (line 226-251) -- reads `Building.StackUnitID` and `Building.Annexes[]`, creates `Unit::Annex` structs with offsets. The annex units are positioned relative to the parent.

**Tech Stack:** C++20, SDL2

---

## Task 1: Population cap enforcement in `enqueueProduceUnit`

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.cpp`

**What:** Before enqueueing a unit, check that current population + unit's pop cost does not exceed the player's housing capacity. The `canAffordUnit()` call on line 113 is supposed to handle this but the comment admits it's unclear. We need an explicit check.

**Why:** Currently you can train unlimited units even when pop-capped.

**How:** In `Building::enqueueProduceUnit()`, after the `canAffordUnit()` check (line 113), add an explicit population headroom check. The unit's population cost is stored in `data->ResourceStorages` where `Type == genie::ResourceStoreMode::GiveAndTakeResourceType` and the resource is `PopulationHeadroom`.

```cpp
// In Building::enqueueProduceUnit(), after line 113 (canAffordUnit check):

    // Population cap check: current pop + queued pop must not exceed housing
    {
        float popUsed = owner->resourcesUsed(genie::ResourceType::PopulationHeadroom);
        float popCap = owner->resourcesAvailable(genie::ResourceType::PopulationHeadroom);

        // Count population already committed in this building's queue
        float queuedPop = 0.f;
        if (m_currentProduct && m_currentProduct->type == Product::Unit) {
            for (const genie::Unit::ResourceStorage &res : m_currentProduct->unit->ResourceStorages) {
                if (res.Type == -1) continue;
                if (genie::ResourceType(res.Type) == genie::ResourceType::PopulationHeadroom) {
                    queuedPop += res.Amount;
                }
            }
        }
        for (const auto &queued : m_productionQueue) {
            if (queued->type != Product::Unit) continue;
            for (const genie::Unit::ResourceStorage &res : queued->unit->ResourceStorages) {
                if (res.Type == -1) continue;
                if (genie::ResourceType(res.Type) == genie::ResourceType::PopulationHeadroom) {
                    queuedPop += res.Amount;
                }
            }
        }

        // Check this unit's pop cost
        float unitPop = 0.f;
        for (const genie::Unit::ResourceStorage &res : data->ResourceStorages) {
            if (res.Type == -1) continue;
            if (genie::ResourceType(res.Type) == genie::ResourceType::PopulationHeadroom) {
                unitPop += res.Amount;
            }
        }

        if (unitPop > 0 && popUsed + queuedPop + unitPop > popCap) {
            DBG << "Population cap reached:" << popUsed << "+" << queuedPop << "+" << unitPop << ">" << popCap;
            return false;
        }
    }
```

**Add include:** `#include <genie/dat/ResourceUsage.h>` (already present at line 8).

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)` + run, build houses, verify you cannot train past pop cap.

**Commit:** `feat: population cap blocks unit training when housing full`

---

## Task 2: Production queue limit (5 units)

**File:** `/home/dima/Projects/freeaoe/src/mechanics/Building.cpp`

**What:** Limit the production queue to 5 items (matching AoE2 original). Currently `m_productionQueue` is an unbounded vector.

**How:** Add a check at the top of both `enqueueProduceUnit()` and `enqueueProduceResearch()`.

In `Building::enqueueProduceUnit()`, after the null check (line 102), add:

```cpp
    // AoE2 limits production queue to 5 items
    if (productionQueueLength() >= 5) {
        DBG << "Production queue full";
        return false;
    }
```

In `Building::enqueueProduceResearch()`, after the null check (line 148), add:

```cpp
    // AoE2 limits production queue to 5 items
    if (productionQueueLength() >= 5) {
        DBG << "Production queue full";
        return false;
    }
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)` + run, try to queue 6 units in a barracks -- 6th should be rejected.

**Commit:** `feat: limit production queue to 5 items per building`

---

## Task 3: Repair cost deduction

**Files:** `/home/dima/Projects/freeaoe/src/actions/ActionRepair.h`, `/home/dima/Projects/freeaoe/src/actions/ActionRepair.cpp`

**What:** Repair should cost resources proportional to the building's original cost. In AoE2, repairing costs (total_resource_cost / max_hp) per HP healed, using the same resource types as the original building. The repair rate is 1 HP/sec (already implemented).

**How:** On each repair tick, calculate per-HP cost from the target building's `Creatable.ResourceCosts`, deduct from the repairer's player. If the player can't afford it, stop repairing.

Replace the repair tick block in `ActionRepair::update()`:

```cpp
// ActionRepair.cpp - replace lines 39-44 with:

#include "mechanics/Player.h"
#include <genie/dat/unit/Creatable.h>

// ... in update(), the repair tick block:

    // Repair tick -- heal 1 HP per interval, deducting proportional resource cost
    if (!m_isMoving && time - m_lastRepairTime >= REPAIR_INTERVAL) {
        m_lastRepairTime = time;

        // Calculate per-HP cost from building's original construction cost
        Player::Ptr owner = unit->player().lock();
        if (!owner) return UpdateResult::Completed;

        const float maxHp = target->data()->HitPoints;
        if (maxHp <= 0) return UpdateResult::Completed;

        // Check affordability first
        bool canAfford = true;
        for (const auto &cost : target->data()->Creatable.ResourceCosts) {
            if (!cost.Paid || cost.Amount <= 0) continue;
            const genie::ResourceType type = genie::ResourceType(cost.Type);
            float perHp = static_cast<float>(cost.Amount) / maxHp;
            if (owner->resourcesAvailable(type) < perHp) {
                canAfford = false;
                break;
            }
        }

        if (!canAfford) {
            return UpdateResult::Completed;  // Stop repairing, out of resources
        }

        // Deduct resources
        for (const auto &cost : target->data()->Creatable.ResourceCosts) {
            if (!cost.Paid || cost.Amount <= 0) continue;
            const genie::ResourceType type = genie::ResourceType(cost.Type);
            float perHp = static_cast<float>(cost.Amount) / maxHp;
            owner->removeResource(type, perHp);
        }

        target->takeDamage(-1.f);
        return UpdateResult::Updated;
    }
```

Add includes at top of ActionRepair.cpp:

```cpp
#include "mechanics/Player.h"
#include <genie/dat/unit/Creatable.h>
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)` + run, damage a building, repair it, watch resources decrease.

**Commit:** `feat: repair deducts proportional resource cost per HP healed`

---

## Task 4: Add Town Bell and Ungarrison buttons to building UI

**File:** `/home/dima/Projects/freeaoe/src/ui/ActionPanel.cpp`

**What:** When a building with garrison capacity is selected (TC, Castle, Tower), show Ungarrison button. When a TC is selected, also show Town Bell / Abort Town Bell button. Currently the click handlers exist but no buttons are ever added in `updateButtons()`.

**How:** In `ActionPanel::updateButtons()`, after the `addCreateButtons(unit)` call (line 477), add building-specific buttons:

```cpp
    // Building-specific buttons: ungarrison, town bell
    if (unit->data()->Type >= genie::Unit::BuildingType) {
        Building::Ptr building = Building::fromUnit(unit);

        // Ungarrison button for any garrisonable building
        if (building && unit->data()->GarrisonCapacity > 0 && !building->garrisonedUnits.empty()) {
            InterfaceButton ungarrisonBtn;
            ungarrisonBtn.action = Command::Ungarrison;
            ungarrisonBtn.index = 5;  // bottom-left area
            currentButtons.push_back(ungarrisonBtn);
        }

        // Town Bell for Town Centers
        if (unit->data()->ID == Unit::TownCenter) {
            InterfaceButton bellBtn;
            if (m_bellActive) {
                bellBtn.action = Command::AbortTownBell;
            } else {
                bellBtn.action = Command::RingTownBell;
            }
            bellBtn.index = 6;
            currentButtons.push_back(bellBtn);
        }
    }
```

Insert this block at line 477, right before the `if (unit->data()->InterfaceKind == genie::Unit::BuildingsInterface)` check.

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)` + run, select a TC -- should see Town Bell button. Garrison units, should see Ungarrison button.

**Commit:** `feat: town bell and ungarrison buttons appear for buildings`

---

## Task 5: Fix annex unit positioning on map

**File:** `/home/dima/Projects/freeaoe/src/mechanics/UnitFactory.cpp`

**What:** Annex units are created with offset coordinates but never actually placed on the map at the correct position. When a building is placed, its annexes should be positioned relative to the parent building's position.

**How:** Check the existing code in `UnitFactory::createUnit()` around line 226. The annexes are created and pushed to `unit->annexes` but their positions are not set. We need to ensure that when `Building::setPosition()` is called, it also updates annex positions.

In `Building::setPosition()` (Building.cpp line 360), add annex repositioning:

```cpp
void Building::setPosition(const MapPos &pos, const bool initial)
{
    std::shared_ptr<Map> map = m_map.lock();
    REQUIRE(map, return);

    Unit::setPosition(map->snapPositionToGrid(pos, clearanceSize()), initial);

    // Reposition annexes relative to this building
    for (auto &annex : annexes) {
        if (annex.unit) {
            MapPos annexPos = position() + annex.offset;
            annex.unit->setPosition(annexPos, initial);
        }
    }
}
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)` + run, build a Town Center or Castle, verify multi-tile footprint renders correctly.

**Commit:** `feat: building annexes positioned correctly relative to parent`

---

## Task 6: AI population check before training

**File:** `/home/dima/Projects/freeaoe/src/ai/BasicAI.cpp`

**What:** The AI already has a population headroom check at line 112-114, but it only checks raw values. It should also account for queued units in production buildings, otherwise the AI will queue past pop cap.

**How:** The AI calls `Building::enqueueProduceUnit()` which now has the pop cap check from Task 1. No AI-specific change is needed -- the building will reject the enqueue if pop is full. However, verify the AI's existing check at line 112 doesn't cause issues.

Read the AI code to confirm:

```cpp
// Line 112-114 of BasicAI.cpp (existing):
    float popCurrent = m_player->resourcesAvailable(genie::ResourceType::CurrentPopulation);
    float popCap = m_player->resourcesAvailable(genie::ResourceType::PopulationHeadroom);
```

This is informational only. The Task 1 fix in `enqueueProduceUnit()` handles enforcement for both human and AI players.

**No code change needed for this task.** Skip to verification.

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)` + run a game with AI, verify AI stops training at pop cap.

---

## Summary of changes

| Task | File(s) | Description |
|------|---------|-------------|
| 1 | `src/mechanics/Building.cpp` | Pop cap blocks training when housing full |
| 2 | `src/mechanics/Building.cpp` | Queue limited to 5 items |
| 3 | `src/actions/ActionRepair.cpp` | Repair deducts resources per HP |
| 4 | `src/ui/ActionPanel.cpp` | Town Bell + Ungarrison buttons appear |
| 5 | `src/mechanics/Building.cpp` | Annexes repositioned with parent |
| 6 | (none -- covered by Task 1) | AI respects pop cap via building check |

**Build command:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Garrison arrows note:** Already implemented in `src/actions/ActionAttack.cpp:268-281`. The formula `extraArrows = (max - base) * garrisoned / GarrisonCapacity` applies to any building (towers, castles, TCs) with `GarrisonCapacity > 0` and `MaxTotalProjectiles > TotalProjectiles`. No changes needed.
