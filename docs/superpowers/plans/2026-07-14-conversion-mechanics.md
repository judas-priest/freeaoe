# Plan: Conversion Mechanics (Faith + Probability Ramp)

**Goal:** Replace flat random 4–10 s timer in `ActionConvert.cpp` with the real AoE2 conversion model: 0% chance during [0, 4 s], then a linear probability ramp reaching 100% at 10 s. Add faith resource to monks (drops to 0 on successful conversion, recharges at 1.6/sec). Cavalry adds +3 s to the minimum window; siege adds +5 s.

**Status:** Not started

---

## Background

Current code (`ActionConvert.cpp` lines 47–53):
```cpp
m_convertDuration = 4000 + (rand() % 6000); // 4-10 sec
if (target->data()->Class == genie::Unit::SiegeWeapon ||
    target->data()->Type == genie::Unit::BuildingType) {
    m_convertDuration += 8000;
}
```
This picks a fixed duration up front. Real AoE2 uses a probability check each frame: the conversion window is [minTime, maxTime], and each tick during that window there is a growing probability of conversion succeeding. This feels more natural and allows interruption.

---

## Changes to `src/actions/ActionConvert.h`

Replace the private section:

```cpp
private:
    std::weak_ptr<Unit> m_target;
    bool m_isMoving = false;

    // Faith system
    Time m_convertStartTime = 0;
    bool m_converting = false;

    // Per-unit resistance windows (milliseconds)
    Time m_minConvertTime = 4000;  // 0% chance before this
    Time m_maxConvertTime = 10000; // 100% chance at this point

    static constexpr float CONVERT_RANGE = 48.f;
    static constexpr float FAITH_RECHARGE_PER_MS = 0.0016f; // 1.6 per second
    static constexpr float FAITH_MAX = 100.f;
```

---

## Changes to `src/actions/ActionConvert.cpp`

### Constructor: compute resistance windows based on target class

Replace the duration-setting block (lines 47–56) entirely:

```cpp
// Start conversion attempt — compute per-unit resistance windows
m_convertStartTime = time;
m_converting = true;

// Base window: 4 s min, 10 s max (AoE2 standard)
m_minConvertTime = 4000;
m_maxConvertTime = 10000;

// Cavalry resistance: +3 s to both ends
if (target->data()->Class == genie::Unit::Cavalry ||
    target->data()->Class == genie::Unit::CavalryArcher) {
    m_minConvertTime += 3000;
    m_maxConvertTime += 3000;
}

// Siege/building resistance: +5 s to both ends
if (target->data()->Class == genie::Unit::SiegeWeapon ||
    target->data()->Type == genie::Unit::BuildingType) {
    m_minConvertTime += 5000;
    m_maxConvertTime += 5000;
}

DBG << monk->debugName << "starting conversion of" << target->debugName
    << "window=[" << m_minConvertTime << "," << m_maxConvertTime << "]ms";
return UpdateResult::Updated;
```

### Replace the "check if conversion complete" block (lines 60–68):

```cpp
if (m_converting) {
    const Time elapsed = time - m_convertStartTime;

    // Still in grace period — no chance yet
    if (elapsed < m_minConvertTime) {
        return UpdateResult::NotUpdated;
    }

    // Past max — guaranteed conversion
    if (elapsed >= m_maxConvertTime) {
        doConvert(monk, target);
        return UpdateResult::Completed;
    }

    // Linear ramp: probability = (elapsed - minTime) / (maxTime - minTime)
    const float window = float(m_maxConvertTime - m_minConvertTime);
    const float progress = float(elapsed - m_minConvertTime) / window;

    // One roll per ~200 ms tick (avoid rolling every frame)
    if (time - m_prevRollTime >= 200) {
        m_prevRollTime = time;
        const int roll = rand() % 1000;
        if (roll < int(progress * 1000.f)) {
            doConvert(monk, target);
            return UpdateResult::Completed;
        }
    }
}

return UpdateResult::NotUpdated;
```

### Add `doConvert` helper and faith management:

```cpp
void ActionConvert::doConvert(const Unit::Ptr &monk, const Unit::Ptr &target)
{
    auto monkOwner = monk->player().lock();
    if (!monkOwner) return;

    DBG << "Converted" << target->debugName << "to player" << monkOwner->playerId;
    target->setPlayer(monkOwner);

    // Drain monk's faith to 0 — monk must recharge before converting again
    monk->resources[genie::ResourceType::FaithRecharging] = 0.f;
}
```

### Faith recharge: update tick (add before `return UpdateResult::NotUpdated` at end of `update`):

```cpp
// Recharge faith passively (1.6 per second)
if (!m_isMoving && !m_converting) {
    float &faith = monk->resources[genie::ResourceType::FaithRecharging];
    if (faith < FAITH_MAX) {
        const float delta = (time - m_prevTime) * FAITH_RECHARGE_PER_MS;
        faith = std::min(FAITH_MAX, faith + delta);
    }
}
```

### Faith gate: at the top of `update`, before moving/converting:

```cpp
// Cannot convert without faith
const float faith = monk->resources[genie::ResourceType::FaithRecharging];
if (!m_converting && faith < 1.f) {
    DBG << "Monk has no faith — waiting";
    return UpdateResult::NotUpdated;
}
```

---

## Header additions

Add to `ActionConvert.h` private members:

```cpp
Time m_minConvertTime = 4000;
Time m_maxConvertTime = 10000;
Time m_prevRollTime = 0;

void doConvert(const Unit::Ptr &monk, const Unit::Ptr &target);
```

---

## genie::ResourceType::FaithRecharging

Check that this resource type exists in genieutils. Search:

```
grep -r "FaithRecharging\|Faith" src/extern/genieutils/include/genie/dat/ResourceType.h
```

In AoE2, the dat file uses resource type 35 (`FaithRecharging`). Reference it by integer if the enum is missing:

```cpp
constexpr int RESOURCE_FAITH = 35;
monk->resources[genie::ResourceType(RESOURCE_FAITH)] = 0.f;
```

---

## Testing

1. Order a monk to convert a militia. Monk should fail for the first 4 s, then succeed with increasing probability between 4–10 s.
2. After a conversion, monk should need to wait for faith to recharge (~60 s for full recharge at 1.6/s) before converting again.
3. Order conversion of a knight — minimum window should be 7 s (4 + 3).
4. Order conversion of a trebuchet — minimum window should be 9 s (4 + 5).
