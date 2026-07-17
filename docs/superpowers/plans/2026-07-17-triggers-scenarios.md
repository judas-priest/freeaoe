# Triggers & Scenarios Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete trigger system -- all missing conditions/effects, gate locking, HD effects, bringing coverage from ~80% to 100%.

**Architecture:**
- `ScenarioController` (`src/mechanics/ScenarioController.h` / `.cpp`) owns a `std::vector<Trigger>` loaded from `genie::ScnFile`.
- Each `Trigger` has a list of `Condition` objects (decremented toward 0 = satisfied) and `genie::TriggerEffect` objects.
- Conditions are checked in three places: (1) event handlers (`onUnitDying`, `onUnitCreated`, `onUnitMoved`, `onUnitSelected`, `onUnitDeselected`, `onPlayerDefeated`, `onAttributeChanged`), (2) the polling `update()` loop (Timer, DifficultyLevel, ResearchTechnology, ObjectVisible, etc.), and (3) the "is implemented" switch in `setScenario()` which filters unimplemented triggers.
- Effects are dispatched through `handleTriggerEffect()` with a big switch statement.
- `forEachMatchingUnit()` iterates area + location tiles, filtering by player/type/object ID.
- `Gate` class (`src/mechanics/Gate.h` / `.cpp`) already has `isLocked` field and `isPassableFor()` / `setOpen()` logic. LockGate/UnlockGate just need to set `isLocked` on matching gate units.
- Unit data is stored as `const genie::Unit *m_data` -- stats are immutable. HD stat-change effects need per-unit override storage.

**Tech Stack:** C++20, SDL2

**Gap Analysis:**

Conditions -- all 20 AoE2 conditions are already in the "is implemented" switch (lines 69-89 of ScenarioController.cpp). All have handling in `update()` or event handlers. No missing conditions.

Effects -- missing from switch entirely:
| # | Effect | Status |
|---|--------|--------|
| 6 | `UnlockGate` | Not handled (missing from switch) |
| 7 | `LockGate` | Not handled (missing from switch) |
| 35 | `HD_TeleportObject` | Not handled (missing from switch) |
| 36 | `HD_ChangeUnitStance` | Not handled (missing from switch) |

Effects -- in switch but stubbed (log-only, no real implementation):
| # | Effect | Status |
|---|--------|--------|
| 26 | `ChangeObjectName` | Logs message, does not rename (debugName is const) |
| 28 | `ChangeObjectAttack` | Stub log |
| 30 | `HD_AttackMove` (UserPatch_ChangeSpeed) | Stub log |
| 31 | `HD_ChangeArmor` (UserPatch_ChangeRange) | Stub log |
| 32 | `HD_ChangeRange` (UserPatch_ChangeMeleArmor) | Stub log |
| 33 | `HD_ChangeSpeed` (UserPatch_ChangePiercingArmor) | Stub log |

---

## Task 1: Implement LockGate and UnlockGate trigger effects
**File:** `src/mechanics/ScenarioController.cpp`

Add two cases to `handleTriggerEffect()` before the `ActivateTrigger` case (around line 643), and add them to the "is implemented" switch in `setScenario()` (around line 158). Also add `#include "mechanics/Gate.h"` at the top.

In `handleTriggerEffect()`, insert after the `case genie::TriggerEffect::None:` block (before ActivateTrigger):

```cpp
    case genie::TriggerEffect::LockGate: {
        DBG << "Locking gate" << effect;
        forEachMatchingUnit(effect, [](const Unit::Ptr &unit) {
            Gate::Ptr gate = Gate::fromUnit(unit);
            if (gate) {
                gate->isLocked = true;
                gate->setOpen(false);
                DBG << "Locked gate" << unit->debugName;
            } else {
                WARN << "LockGate effect on non-gate unit" << unit->debugName;
            }
        });
        break;
    }
    case genie::TriggerEffect::UnlockGate: {
        DBG << "Unlocking gate" << effect;
        forEachMatchingUnit(effect, [](const Unit::Ptr &unit) {
            Gate::Ptr gate = Gate::fromUnit(unit);
            if (gate) {
                gate->isLocked = false;
                DBG << "Unlocked gate" << unit->debugName;
            } else {
                WARN << "UnlockGate effect on non-gate unit" << unit->debugName;
            }
        });
        break;
    }
```

In `setScenario()`, add to the effect "is implemented" switch (around line 158):
```cpp
            case genie::TriggerEffect::LockGate:
            case genie::TriggerEffect::UnlockGate:
```

Add at the top of the file with other includes:
```cpp
#include "mechanics/Gate.h"
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)` -- must compile cleanly.

**Commit message:** `feat: LockGate and UnlockGate trigger effects`

---

## Task 2: Implement HD_TeleportObject trigger effect
**File:** `src/mechanics/ScenarioController.cpp`

Add a case in `handleTriggerEffect()` for `HD_TeleportObject`. This effect instantly moves matching units to `effect.location` (no pathfinding). Insert near the other object-manipulation effects (after StopUnit, around line 892):

```cpp
    case genie::TriggerEffect::HD_TeleportObject: {
        // WARNING: flipped x and y
        MapPos targetPos(effect.location.y * Constants::TILE_SIZE, effect.location.x * Constants::TILE_SIZE);
        DBG << "Teleporting to" << targetPos << effect;
        forEachMatchingUnit(effect, [&targetPos](const Unit::Ptr &unit) {
            DBG << "Teleporting" << unit->debugName;
            unit->setPosition(targetPos);
        });
        break;
    }
```

In `setScenario()`, add to the effect "is implemented" switch:
```cpp
            case genie::TriggerEffect::HD_TeleportObject:
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: HD_TeleportObject trigger effect — instant unit relocation`

---

## Task 3: Implement HD_ChangeUnitStance trigger effect
**File:** `src/mechanics/ScenarioController.cpp`

This is similar to `SetUnitStance` but uses `effect.amount` for the stance value instead of `effect.boundedValue`. Add near the existing `SetUnitStance` case:

```cpp
    case genie::TriggerEffect::HD_ChangeUnitStance: {
        Unit::Stance stance = Unit::Stance::Invalid;
        const int stanceVal = (effect.amount >= 0) ? effect.amount : effect.boundedValue;
        switch(stanceVal) {
        case 0: stance = Unit::Stance::Aggressive; break;
        case 1: stance = Unit::Stance::Defensive; break;
        case 2: stance = Unit::Stance::StandGround; break;
        case 3: stance = Unit::Stance::NoAttack; break;
        default:
            WARN << "HD_ChangeUnitStance: invalid stance" << stanceVal;
            break;
        }
        if (stance != Unit::Stance::Invalid) {
            forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
                unit->stance = stance;
            });
        }
        break;
    }
```

In `setScenario()`, add to the effect "is implemented" switch:
```cpp
            case genie::TriggerEffect::HD_ChangeUnitStance:
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: HD_ChangeUnitStance trigger effect`

---

## Task 4: Add per-unit stat overrides struct
**File:** `src/mechanics/Unit.h`

The HD stat-change effects (`ChangeObjectAttack`, `HD_ChangeArmor`, `HD_ChangeRange`, `HD_ChangeSpeed`) modify individual unit stats at runtime, but `m_data` points to shared immutable `genie::Unit` data. We need per-unit overrides.

Add a struct and member inside the `Unit` class (public section, around line 170):

```cpp
    /// Per-unit stat overrides applied by trigger effects
    struct StatOverrides {
        int attackBonus = 0;     // added to base attack
        int armorMelee = 0;     // added to base melee armor
        int armorPiercing = 0;  // added to base piercing armor
        float rangeBonus = 0.f; // added to base range
        float speedOverride = -1.f; // if >= 0, replaces base speed
    };
    StatOverrides statOverrides;
```

Add accessor methods in the public section:

```cpp
    /// Effective attack (base + override)
    int effectiveAttack() const noexcept {
        int base = 0;
        if (m_data && m_data->Combat.Attacks.size() > 0) {
            base = m_data->Combat.Attacks[0].Amount;
        }
        return base + statOverrides.attackBonus;
    }

    /// Effective melee armor (base + override)
    int effectiveMeleeArmor() const noexcept {
        int base = m_data ? m_data->Combat.DisplayedMeleeArmour : 0;
        return base + statOverrides.armorMelee;
    }

    /// Effective piercing armor (base + override)
    int effectivePiercingArmor() const noexcept {
        int base = m_data ? m_data->Combat.DisplayedPierceArmour : 0;
        return base + statOverrides.armorPiercing;
    }

    /// Effective range (base + override)
    float effectiveRange() const noexcept {
        float base = m_data ? m_data->Combat.MaxRange : 0.f;
        return base + statOverrides.rangeBonus;
    }

    /// Effective speed (override or base)
    float effectiveSpeed() const noexcept {
        if (statOverrides.speedOverride >= 0.f) return statOverrides.speedOverride;
        return m_data ? m_data->Speed : 0.f;
    }
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: per-unit stat overrides struct for trigger effects`

---

## Task 5: Implement ChangeObjectAttack trigger effect
**File:** `src/mechanics/ScenarioController.cpp`

Replace the stub for `ChangeObjectAttack` (around line 948) with a real implementation:

```cpp
    case genie::TriggerEffect::ChangeObjectAttack: {
        DBG << "ChangeObjectAttack" << effect;
        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            unit->statOverrides.attackBonus += effect.amount;
            DBG << "Changed attack of" << unit->debugName << "by" << effect.amount;
        });
        break;
    }
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: ChangeObjectAttack trigger effect modifies unit attack`

---

## Task 6: Implement HD_ChangeArmor trigger effect
**File:** `src/mechanics/ScenarioController.cpp`

Replace the stub for `HD_ChangeArmor` with a real implementation. In AoE2 HD, this effect uses `effect.amount` for the armor class (0=melee, 1=piercing) in the low bits and the armor value:

```cpp
    case genie::TriggerEffect::HD_ChangeArmor: {
        DBG << "HD_ChangeArmor" << effect;
        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            // amount encodes the armor change; boundedValue selects class (0=melee, 1=pierce)
            if (effect.boundedValue == 1) {
                unit->statOverrides.armorPiercing += effect.amount;
            } else {
                unit->statOverrides.armorMelee += effect.amount;
            }
            DBG << "Changed armor of" << unit->debugName << "class" << effect.boundedValue << "by" << effect.amount;
        });
        break;
    }
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: HD_ChangeArmor trigger effect modifies unit armor`

---

## Task 7: Implement HD_ChangeRange trigger effect
**File:** `src/mechanics/ScenarioController.cpp`

Replace the stub for `HD_ChangeRange`:

```cpp
    case genie::TriggerEffect::HD_ChangeRange: {
        DBG << "HD_ChangeRange" << effect;
        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            unit->statOverrides.rangeBonus += float(effect.amount);
            DBG << "Changed range of" << unit->debugName << "by" << effect.amount;
        });
        break;
    }
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: HD_ChangeRange trigger effect modifies unit range`

---

## Task 8: Implement HD_ChangeSpeed and HD_AttackMove trigger effects
**File:** `src/mechanics/ScenarioController.cpp`

Replace the stubs for `HD_AttackMove` and `HD_ChangeSpeed`. Note: these share enum values with UserPatch effects. `HD_AttackMove` (=`UserPatch_ChangeSpeed`, value 30) changes speed. `HD_ChangeSpeed` (=`UserPatch_ChangePiercingArmor`, value 33) also changes speed. We handle both as speed changes:

```cpp
    case genie::TriggerEffect::HD_AttackMove: {
        // HD_AttackMove = UserPatch_ChangeSpeed = SWGB_SnapView (value 30)
        // In AoE2 HD context this is attack-move, but we treat as speed change for UserPatch compat
        DBG << "HD_AttackMove/ChangeSpeed" << effect;
        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            unit->statOverrides.speedOverride = float(effect.amount) / 100.f;
            DBG << "Changed speed of" << unit->debugName << "to" << unit->statOverrides.speedOverride;
        });
        break;
    }
    case genie::TriggerEffect::HD_ChangeSpeed: {
        DBG << "HD_ChangeSpeed" << effect;
        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            float baseSpeed = unit->data() ? unit->data()->Speed : 0.f;
            unit->statOverrides.speedOverride = baseSpeed + float(effect.amount) / 100.f;
            DBG << "Changed speed of" << unit->debugName << "to" << unit->statOverrides.speedOverride;
        });
        break;
    }
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: HD_ChangeSpeed and HD_AttackMove trigger effects`

---

## Task 9: Wire effectiveSpeed() into movement system
**Files:** `src/actions/ActionMove.cpp`

Find where unit speed is read (likely `m_data->Speed` or `data()->Speed`) in `ActionMove` and replace with `effectiveSpeed()`. Search for speed usage:

Look for patterns like `unit->data()->Speed` or `m_unit->data()->Speed` in `ActionMove.cpp` and replace with `unit->effectiveSpeed()` / `m_unit->effectiveSpeed()`.

Similarly check `src/mechanics/Missile.cpp` and any other files that read `->Speed` for unit movement.

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: wire effectiveSpeed into movement for trigger stat overrides`

---

## Task 10: Wire effectiveRange() into combat system
**Files:** `src/actions/ActionAttack.cpp` (or wherever range checks happen)

Search for `MaxRange` usage in attack/combat code and replace with `effectiveRange()` where appropriate. Key locations:
- Range checks in attack action (whether target is in range)
- Any line-of-sight or attack-range comparisons

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: wire effectiveRange into combat for trigger stat overrides`

---

## Task 11: Wire effectiveAttack() and armor into damage calculation
**Files:** `src/mechanics/Unit.cpp` or wherever damage calculation happens

Search for `DisplayedMeleeArmour`, `DisplayedPierceArmour`, and attack amount reads in the damage formula. Replace with the effective* accessors.

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: wire effectiveAttack and armor into damage calc for trigger overrides`

---

## Task 12: Implement ChangeObjectName properly
**File:** `src/mechanics/Unit.h`, `src/mechanics/ScenarioController.cpp`

Make `debugName` mutable (change from `const std::string` to `std::string` in `src/Entity.h` if that is where it lives, or add a `displayName` override field).

In `Unit.h`, add:
```cpp
    std::string nameOverride; // set by ChangeObjectName trigger effect
```

In `ScenarioController.cpp`, replace the stub:
```cpp
    case genie::TriggerEffect::ChangeObjectName:
        DBG << "ChangeObjectName:" << effect.message;
        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            unit->nameOverride = effect.message;
            DBG << "Renamed" << unit->debugName << "to" << effect.message;
        });
        break;
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `feat: ChangeObjectName trigger effect stores name override`

---

## Task 13: Clean up PlayerDefeated condition handling
**File:** `src/mechanics/ScenarioController.cpp`

The `onPlayerDefeated` handler (line 1135) only checks if the human player lost. It does not decrement `amountRequired` on `PlayerDefeated` conditions for AI players. Fix it:

In `onPlayerDefeated()`, add condition scanning:
```cpp
void ScenarioController::onPlayerDefeated(Player *player)
{
    if (!m_gameState) return;

    Player::Ptr human = m_gameState->humanPlayer();
    if (!human) return;

    if (player->playerId == human->playerId) {
        m_gameState->result = GameState::Result::Lost;
    }

    // Satisfy PlayerDefeated conditions for this player
    for (Trigger &trigger : m_triggers) {
        if (!trigger.enabled) continue;
        for (Condition &condition : trigger.conditions) {
            if (condition.data.type == genie::TriggerCondition::PlayerDefeated &&
                condition.data.sourcePlayer == player->playerId) {
                condition.amountRequired = 0;
            }
        }
    }
}
```

**Test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

**Commit message:** `fix: PlayerDefeated condition now triggers for AI players`

---

## Summary

| Task | Description | Files |
|------|-------------|-------|
| 1 | LockGate / UnlockGate effects | `ScenarioController.cpp` |
| 2 | HD_TeleportObject effect | `ScenarioController.cpp` |
| 3 | HD_ChangeUnitStance effect | `ScenarioController.cpp` |
| 4 | Per-unit stat overrides struct | `Unit.h` |
| 5 | ChangeObjectAttack effect | `ScenarioController.cpp` |
| 6 | HD_ChangeArmor effect | `ScenarioController.cpp` |
| 7 | HD_ChangeRange effect | `ScenarioController.cpp` |
| 8 | HD_ChangeSpeed / HD_AttackMove effects | `ScenarioController.cpp` |
| 9 | Wire effectiveSpeed() into movement | `ActionMove.cpp` + others |
| 10 | Wire effectiveRange() into combat | `ActionAttack.cpp` + others |
| 11 | Wire effectiveAttack/armor into damage | `Unit.cpp` + others |
| 12 | ChangeObjectName properly | `Unit.h`, `ScenarioController.cpp` |
| 13 | Fix PlayerDefeated condition for AI | `ScenarioController.cpp` |

All 20 AoE2 conditions are already implemented. After these 13 tasks, all 36 effect types defined in the genie enum will be handled (the remaining SWGB-only effects like `SWGBCC_InputOff`/`SWGBCC_InputOn` are not relevant to AoE2).
