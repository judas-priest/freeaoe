# Module 31: Hunt/Carcass Food Decay — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dead animal carcasses lose food over time (decay), matching AoE2 behavior where hunted animals rot and eventually have no food left.

**Architecture:** In `ActionGather`, when gathering from a dead animal (hunt target), apply a decay rate to the target's remaining resources based on elapsed time since death. AoE2 decays hunted food at ~0.25 food/second.

**Tech Stack:** C++, genie engine data

---

### Task 1: Track death time on units

**Files:**
- Modify: `src/mechanics/Unit.h` (add m_deathTime field)
- Modify: `src/mechanics/Unit.cpp` (set m_deathTime when unit dies)

- [ ] **Step 1: Add death timestamp to Unit**

In `Unit.h`, add a member variable near other state fields (around line 300):
```cpp
Time m_deathTime = 0;
```

Add a public accessor:
```cpp
Time deathTime() const { return m_deathTime; }
```

- [ ] **Step 2: Set death time when unit dies**

In `Unit.cpp`, find the code that handles unit death (search for `hitpointsLeft() <= 0` or `setDead` or the death/corpse transition). In `Unit::takeDamage()` or wherever the unit transitions to dead state, set:

```cpp
if (hitpointsLeft() <= 0 && m_deathTime == 0) {
    m_deathTime = m_lastUpdateTime;
}
```

If `m_lastUpdateTime` is not available, use the time parameter from the update cycle. The exact location depends on how death is triggered — look for where `DecayingEntity` is created in `UnitFactory.cpp` or where the unit's health reaches zero.

- [ ] **Step 3: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
Expected: Clean compilation.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/Unit.h src/mechanics/Unit.cpp
git commit -m "feat: track unit death timestamp for carcass decay"
```

---

### Task 2: Apply food decay during gathering

**Files:**
- Modify: `src/actions/ActionGather.cpp` (decay food from carcass)

- [ ] **Step 1: Identify gather target resource deduction**

In `ActionGather.cpp`, find where resources are taken from the target (search for `ResourceCapacity` or where the gatherer extracts food). This is the gather loop around line 90+ where `WorkRate` is applied.

- [ ] **Step 2: Add decay calculation before gathering**

At the point where the gatherer takes food from a dead animal target, add decay logic. Insert before the resource extraction:

```cpp
// Decay food from dead animals (hunt carcasses rot at 0.25 food/sec)
if (m_target && m_target->deathTime() > 0) {
    const float decayRate = 0.25f; // food per second
    const float timeSinceDeath = (time - m_target->deathTime()) / 1000.0f;
    const float maxFood = m_target->data()->ResourceCapacity;
    const float decayedFood = std::min(decayRate * timeSinceDeath, maxFood);
    const float currentFood = m_target->resources[genie::ResourceType::FoodStorage];
    if (currentFood > 0) {
        const float effectiveFood = std::max(maxFood - decayedFood, 0.f);
        if (currentFood > effectiveFood) {
            m_target->resources[genie::ResourceType::FoodStorage] = effectiveFood;
        }
    }
    if (m_target->resources[genie::ResourceType::FoodStorage] <= 0) {
        // Carcass fully decayed, abandon gathering
        DBG << "Carcass fully decayed";
        m_target->resources[genie::ResourceType::FoodStorage] = 0;
    }
}
```

The exact resource key depends on how gather targets store food — check if it's `FoodStorage` or another resource type in the genie enum.

- [ ] **Step 3: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
Expected: Clean compilation.

- [ ] **Step 4: Commit**

```bash
git add src/actions/ActionGather.cpp
git commit -m "feat: carcass food decay (0.25 food/sec after death)"
```
