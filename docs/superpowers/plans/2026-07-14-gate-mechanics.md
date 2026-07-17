# Gate Open/Close Mechanics Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Read existing files before writing any code. Every file path below is exact and verified. Do not guess class names, method signatures, or enum values — they are all specified here.

**Goal:** Make gates auto-open for friendly/allied units and remain impassable walls for enemies, with a UI lock/unlock toggle.
**Architecture:** A new `Gate` subclass of `Building` tracks open/locked state; `UnitFactory` creates it for `genie::Unit::Gate` class units; `ActionMove::isPassable()` checks `GateObstructionClass` and casts to `Gate` to decide passability per player; `Gate::update()` scans its footprint tiles each tick to auto-open/close.
**Tech Stack:** C++, SDL2

---

## Step 1 — Create `src/mechanics/Gate.h` (3 min)

Create `/home/dima/Projects/freeaoe/src/mechanics/Gate.h`:

```cpp
#pragma once
#include "Building.h"

class Gate : public Building {
public:
    using Ptr = std::shared_ptr<Gate>;

    Gate(const genie::Unit &data_, const std::shared_ptr<Player> &player, UnitManager &unitManager);

    bool isLocked = false;
    bool isOpen   = false;

    // Returns true if a unit with the given playerId may pass through this gate.
    // Enemies never pass. Locked gates block everyone including owner.
    bool isPassableFor(int playerId) const noexcept;

    void setOpen(bool open) noexcept;

    bool update(Time time) noexcept override;

    static Ptr fromUnit(const Unit::Ptr &unit) noexcept {
        return std::dynamic_pointer_cast<Gate>(unit);
    }
};
```

---

## Step 2 — Create `src/mechanics/Gate.cpp` (5 min)

Create `/home/dima/Projects/freeaoe/src/mechanics/Gate.cpp`.

Key facts from reading the source:
- `Building(data_, player, unitManager)` — correct base constructor (see `Building.cpp:26`)
- `Unit::playerId()` returns `m_playerId` (see `Unit.h:221`)
- `Player::isAllied(uint8_t playerId)` exists (see `Player.h:189`)
- `renderer().setSprite(int spriteId)` is the correct call (see `GraphicRender.h:91`)
- `data()->StandingGraphic` is `std::pair<int16_t,int16_t>` — `.first` is the primary graphic (see `Unit.h:199`)
- `data()->Building.ConstructionGraphicID` holds the "closed" frame for a gate (use as closed-state sprite; open state is `StandingGraphic.first`)
- `map()->entitiesAt(col, row)` returns `const std::vector<std::weak_ptr<Entity>>&` (see `Map.h:117`)
- `Unit::fromEntity(weak_ptr<Entity>)` gives a `Unit::Ptr`
- Gate footprint: `position()` gives centre; size is `data()->Size.x` × `data()->Size.y` tiles; scan tiles `[tileX - sizeX, tileX + sizeX]` × same for Y

```cpp
#include "Gate.h"
#include "Map.h"
#include "Player.h"
#include "core/Constants.h"
#include "core/Logger.h"

Gate::Gate(const genie::Unit &data_, const std::shared_ptr<Player> &player, UnitManager &unitManager)
    : Building(data_, player, unitManager)
{
}

bool Gate::isPassableFor(int queryPlayerId) const noexcept
{
    if (isLocked) return false;
    if (!isOpen)  return false;

    Player::Ptr owner = player().lock();
    if (!owner) return false;

    // Owner's own units always pass when open & unlocked
    if (owner->playerId == queryPlayerId) return true;

    // Allied players pass too
    return owner->isAllied(static_cast<uint8_t>(queryPlayerId));
}

void Gate::setOpen(bool open) noexcept
{
    if (isOpen == open) return;
    isOpen = open;

    // Swap sprite: open = StandingGraphic.first, closed = Building.ConstructionGraphicID
    int targetSpriteId = open
        ? data()->StandingGraphic.first
        : data()->Building.ConstructionGraphicID;

    if (targetSpriteId >= 0) {
        renderer().setSprite(targetSpriteId);
    }
}

bool Gate::update(Time time) noexcept
{
    // Run base Building logic (production queue, etc.)
    bool ret = Building::update(time);

    if (isLocked) {
        setOpen(false);
        return ret;
    }

    MapPtr m = map();
    if (!m) return ret;

    Player::Ptr owner = player().lock();
    if (!owner) return ret;

    const MapPos centre = position();
    const int tileX = static_cast<int>(centre.x / Constants::TILE_SIZE_F);
    const int tileY = static_cast<int>(centre.y / Constants::TILE_SIZE_F);
    const int halfW = std::max(1, static_cast<int>(std::ceil(data()->Size.x)));
    const int halfH = std::max(1, static_cast<int>(std::ceil(data()->Size.y)));

    bool friendlyInFootprint = false;
    for (int dx = -halfW; dx <= halfW && !friendlyInFootprint; ++dx) {
        for (int dy = -halfH; dy <= halfH && !friendlyInFootprint; ++dy) {
            const int col = tileX + dx;
            const int row = tileY + dy;
            if (col < 0 || row < 0 || col >= m->columnCount() || row >= m->rowCount()) continue;

            for (const std::weak_ptr<Entity> &e : m->entitiesAt(col, row)) {
                Unit::Ptr u = Unit::fromEntity(e);
                if (!u || u.get() == this) continue;
                if (u->data()->Speed == 0) continue; // skip buildings/resources

                const int uid = u->playerId();
                if (uid == owner->playerId || owner->isAllied(static_cast<uint8_t>(uid))) {
                    friendlyInFootprint = true;
                    break;
                }
            }
        }
    }

    setOpen(friendlyInFootprint);
    return ret;
}
```

---

## Step 3 — Patch `UnitFactory.cpp` to create `Gate` objects (2 min)

File: `/home/dima/Projects/freeaoe/src/mechanics/UnitFactory.cpp`

At the top, add the include after `Farm.h`:
```cpp
#include "Gate.h"
```

In `UnitFactory::createUnit()` (line ~196), the current code is:
```cpp
    if (ID == Unit::Farm) {
        unit = std::make_shared<Farm>(gunit, owner, unitManager);
    } else if (gunit.Type == genie::Unit::BuildingType) {
        unit = std::make_shared<Building>(gunit, owner, unitManager);
    } else {
```

Replace the `else if` branch:
```cpp
    if (ID == Unit::Farm) {
        unit = std::make_shared<Farm>(gunit, owner, unitManager);
    } else if (gunit.Class == genie::Unit::Gate) {
        unit = std::make_shared<Gate>(gunit, owner, unitManager);
    } else if (gunit.Type == genie::Unit::BuildingType) {
        unit = std::make_shared<Building>(gunit, owner, unitManager);
    } else {
```

---

## Step 4 — Patch `ActionMove::isPassable()` to honour gate state (4 min)

File: `/home/dima/Projects/freeaoe/src/actions/ActionMove.cpp`

Add include near the top (after `mechanics/Unit.h`):
```cpp
#include "mechanics/Gate.h"
```

In `ActionMove::isPassable()` (line ~896), the `BuildingObstruction` case (line ~964–971) currently unconditionally returns `false` when `dx == tileX && dy == tileY`:

```cpp
                case genie::Unit::BuildingObstruction:
                case genie::Unit::MountainObstruction:
                    if (dx == tileX && dy == tileY) {
                        if (useCache) { m_passable[cacheIndex] = false; }
                        return false;
                    }
                    break;
```

Replace with:
```cpp
                case genie::Unit::BuildingObstruction:
                case genie::Unit::MountainObstruction:
                    if (dx == tileX && dy == tileY) {
                        // Gates are passable for their owner/allies when open & unlocked
                        if (otherUnit->data()->ObstructionClass == genie::Unit::GateObstructionClass) {
                            Gate::Ptr gate = Gate::fromUnit(otherUnit);
                            Unit::Ptr mover = m_unit.lock();
                            if (gate && mover && gate->isPassableFor(mover->playerId())) {
                                break; // passable — continue checking other entities
                            }
                        }
                        if (useCache) { m_passable[cacheIndex] = false; }
                        return false;
                    }
                    break;
```

Note: `GateObstructionClass` is defined as `5` in `genie::Unit::ObstructionClasses` (see `Unit.h:551`). The field is `otherUnit->data()->ObstructionClass`.

---

## Step 5 — Add `LockGate`/`UnlockGate` commands to `ActionPanel` (5 min)

File: `/home/dima/Projects/freeaoe/src/ui/ActionPanel.h`

In the `Command` enum (line ~166), after `Garrison = 240` add:
```cpp
        LockGate   = 241,
        UnlockGate = 242,
```

File: `/home/dima/Projects/freeaoe/src/ui/ActionPanel.cpp`

1. Add include after `Building.h`:
```cpp
#include "mechanics/Gate.h"
```

2. In `helpTextId()` (line ~262), add entries to the static map:
```cpp
        { Command::LockGate,   41060 },   // "Lock Gate" — use a free SLP string ID
        { Command::UnlockGate, 41061 },
```

3. In `updateButtons()`, after the `SiegeWeapon` class check (line ~583), add:
```cpp
    // Gate lock/unlock button
    if (unit->data()->Class == genie::Unit::Gate) {
        Gate::Ptr gate = Gate::fromUnit(unit);
        if (gate) {
            InterfaceButton gateBtn;
            gateBtn.action = gate->isLocked ? Command::UnlockGate : Command::LockGate;
            gateBtn.index  = 5;
            currentButtons.push_back(gateBtn);
        }
    }
```

4. In the command `switch` (near line ~836, after `Pack`/`Unpack` case), add:
```cpp
        case Command::LockGate:
        case Command::UnlockGate: {
            const bool newLocked = (button.action == Command::LockGate);
            for (const Unit::Ptr &u : m_selectedUnits) {
                Gate::Ptr gate = Gate::fromUnit(u);
                if (gate) {
                    gate->isLocked = newLocked;
                    if (newLocked) gate->setOpen(false);
                }
            }
            m_buttonsDirty = true;
            break;
        }
```

---

## Step 6 — Register `Gate.cpp` in build system (2 min)

File: `/home/dima/Projects/freeaoe/CMakeLists.txt`

After line 306 (`src/mechanics/Building.cpp`), add:
```
    src/mechanics/Gate.cpp
```

The android build uses the same root `CMakeLists.txt` (no separate android CMakeLists for source files — the `.cxx/` directory is generated). No additional change needed.

---

## Step 7 — Verify & build (3 min)

Desktop build (quick compile check):
```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc) 2>&1 | grep -E "error:|warning:" | head -30
```

Android build:
```bash
cd /home/dima/Projects/freeaoe/android && rm -rf app/.cxx app/build && unset LD_PRELOAD && ./gradlew assembleDebug
```

Install and test:
```bash
unset LD_PRELOAD && adb install -r android/app/build/outputs/apk/debug/app-debug.apk
adb logcat -s "FreeAoE" | grep -i gate
```

Expected log lines on approaching a gate:
- `Gate::update — friendly in footprint, opening`
- `Gate::setOpen(true) — switching sprite`

If sprite does not change, verify `data()->Building.ConstructionGraphicID` is non-negative for unit ID 487 by adding a temporary `DBG` log in `Gate::setOpen`.

---

## Known edge cases / follow-up

- **Cache invalidation**: `ActionMove` caches passability per `(x, y)`. Gates change state at runtime, so `resetCache()` must be called when `setOpen()` changes state. Add `if (open != isOpen) { /* notify unit managers to reset move caches */ }` — or simply clear `m_passableDirty = true` via a signal. Simplest short-term fix: call `resetCache()` on all active `ActionMove` instances via `UnitManager::units()` loop in `Gate::setOpen()`.
- **Garrisoned units**: garrisoned units have `Speed == 0` but live inside the gate; the footprint scan skips them correctly via the `Speed == 0` guard.
- **Enemy units inside gate footprint**: if an enemy somehow enters the tile (e.g. was there when gate closed), the gate should not reopen. The `isAllied` check handles this.
- **Multiplayer / AI**: AI units have non-human `playerId`; `Player::isAllied()` already handles this correctly.
