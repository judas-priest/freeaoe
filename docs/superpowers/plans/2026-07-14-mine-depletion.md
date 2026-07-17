# Plan: Mine / Resource Depletion Visual

**Goal:** When a resource (gold mine, stone mine, berry bush, tree) reaches 0, switch its graphic to the unit's `DeadUnitID` so it shows as a depleted/empty resource visually instead of standing there looking full.

**Status:** Not started

---

## Context

`ActionGather.cpp` lines 71–78 already detect when a target resource is empty:

```cpp
if (unit->resources[m_resourceType] >= unit->data()->ResourceCapacity || target->resources[m_resourceType] == 0) {
    if (target->resources[m_resourceType] == 0) {
        DBG << target->debugName << "is empty" << target->resources[m_resourceType];
    }
    return maybeDropOff(unit);
}
```

The `genie::Unit` data struct has `DeadUnitID` (line 246 in `Unit.h`): `int16_t DeadUnitID = -1;`.

For gold mines the `DeadUnitID` is the "depleted mine" unit, which is just a flat rock graphic. For trees it's the stump.

---

## Change: `src/actions/ActionGather.cpp`

In `ActionGather::update`, at line 72 where `target->resources[m_resourceType] == 0` is detected:

**Before (line 72):**
```cpp
if (target->resources[m_resourceType] == 0) {
    DBG << target->debugName << "is empty" << target->resources[m_resourceType];
}
```

**After:**
```cpp
if (target->resources[m_resourceType] == 0) {
    DBG << target->debugName << "is empty" << target->resources[m_resourceType];
    // Switch to depleted graphic if the unit data specifies one
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

That is the complete change — one block inserted into the existing empty-resource branch.

---

## Notes

- `target->setUnitData(deadData)` updates the unit's graphic pointer. `Unit::setUnitData` already calls `updateGraphic()` which picks up the new `StandingGraphic`.
- `Civilization::unitData(id)` is already used in `ActionAttack.cpp` line 219 and elsewhere — safe to call here.
- If `owner` is Gaia (resource node), `target->player()` still returns the Gaia shared_ptr which holds the Gaia civilization. Gaia has the full unit table.
- No new files needed.
- The depleted unit still occupies its tile (it is not removed). Villagers will stop trying to gather from it on the next gather attempt because `resources[m_resourceType] == 0` and `maybeDropOff` is already called.

---

## Testing

1. Task a gold miner on a small gold mine (reduce via trigger or debug console to ~5 gold).
2. Miner should harvest it to 0.
3. On the tick where resources hit 0, the mine graphic should change to the depleted mine (flat grey rock, unit ID ~1797 in standard AoE2).
4. Repeat for trees (should become stump) and stone (should become small stone pile).
