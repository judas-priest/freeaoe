# Module 9: Tree Stumps & Gathering Visuals

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When villagers fully chop a tree, show a stump/dead tree instead of instant vanishing. Dead animals lose food over time.

**Architecture:** ActionGather.cpp already handles tree depletion by switching to `DeadUnitID` via `target->setUnitData(deadData)` (ActionGather.cpp:65-75). The `DeadUnitID` is the stump graphic data. This already works — trees switch to their dead unit data. What's missing is: the dead tree should eventually be removed (decay). For animals, add food decay timer after death.

**Tech Stack:** C++20, modify existing files.

---

### Task 1: Verify tree stump behavior

**Files:**
- Read: `src/actions/ActionGather.cpp:65-75`

- [ ] **Step 1: Check if tree stumps already work**

The code at ActionGather.cpp:65-75 already does:
```cpp
if (target->resources[m_resourceType] == 0) {
    const int16_t deadId = target->data()->DeadUnitID;
    if (deadId >= 0) {
        auto owner = target->player().lock();
        if (owner) {
            const genie::Unit &deadData = owner->civilization.unitData(deadId);
            target->setUnitData(deadData);
        }
    }
}
```

This switches the tree to its `DeadUnitID` (stump graphic). **Test first**: chop a tree on device and see if a stump appears. If it does, this task is already done.

- [ ] **Step 2: If stumps don't decay, add timed removal**

If stumps persist forever, add a decay timer. When `setUnitData()` switches to the dead unit data, mark the unit for timed removal:

```cpp
// After setUnitData to dead unit:
target->setUnitData(deadData);
// Start decay timer — unit will be removed after decayTime
target->setDecayTime(60.f); // 60 seconds
```

If `Unit` doesn't have `setDecayTime`, add it:

```cpp
// In Unit.h:
float m_decayTimer = -1.f; // -1 = no decay

void setDecayTime(float seconds) { m_decayTimer = seconds; }

// In Unit::update():
if (m_decayTimer > 0) {
    m_decayTimer -= (time - m_prevTime) / 1000.f;
    if (m_decayTimer <= 0) {
        kill();
    }
}
```

- [ ] **Step 3: Build and test**

Chop a tree — stump should appear and eventually disappear.

- [ ] **Step 4: Commit**

```bash
git add src/actions/ActionGather.cpp src/mechanics/Unit.h src/mechanics/Unit.cpp
git commit -m "$(cat <<'EOF'
feat: tree stumps decay and disappear after 60 seconds
EOF
)"
```

---

### Task 2: Dead animal food decay

**Files:**
- Modify: `src/mechanics/Unit.cpp` — reduce food on dead prey animals over time

- [ ] **Step 1: Find Unit::update() method**

Read Unit.cpp to find the `update()` method. Add food decay logic for dead animals.

- [ ] **Step 2: Add food decay for dead animals**

In `Unit::update()`, after existing logic:

```cpp
// Dead animal food decay: lose 0.1 food/sec
if (isDead() && data()->Class == genie::Unit::PreyAnimal) {
    float currentFood = resources[genie::ResourceType::FoodStorage];
    if (currentFood > 0) {
        float elapsed = (time - m_prevTime) / 1000.f;
        float decay = 0.1f * elapsed;
        resources[genie::ResourceType::FoodStorage] = std::max(0.f, currentFood - decay);
    }
}
```

Note: Check the exact class enum. It might be `genie::Unit::PreyAnimal` or similar — verify in genieutils `Unit.h` UnitClass enum. Deer/boar classes are typically `PreyAnimal = 58` and `DomesticAnimal = 59`.

- [ ] **Step 3: Build and test**

Kill a deer, wait 2 minutes — remaining food should decrease.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/Unit.cpp
git commit -m "$(cat <<'EOF'
feat: dead animal food decay — carcasses lose food over time
EOF
)"
```
