# Plan: Relic Drop on Monk Death

**Goal:** Instead of calling `relic->kill()` on pickup (which permanently removes the relic), hide the relic and store a `weak_ptr` to it on the monk. When the monk dies, respawn the relic at the monk's death position so another player can retrieve it.

**Status:** Not started

---

## Current behavior

`ActionPickupRelic.cpp` line 44:
```cpp
relic->kill(); // Remove relic from world
```

This permanently destroys the relic entity. In real AoE2, the relic drops at the monk's feet on death and can be picked up by any monk.

---

## Changes to `ActionPickupRelic.h`

Replace `m_relic` (currently a `weak_ptr<Unit>`) with a shared_ptr that keeps the relic alive while carried. Add a flag for whether we are actually carrying it.

```cpp
#pragma once

#include "IAction.h"
#include "mechanics/Unit.h"

class ActionPickupRelic : public IAction
{
public:
    ActionPickupRelic(const Unit::Ptr &monk, const Unit::Ptr &relic);
    ~ActionPickupRelic() override;
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::PickupRelic; }

    // Called by the unit death system to drop the relic
    void dropRelic(const MapPos &deathPosition);

private:
    std::weak_ptr<Unit> m_relic;      // The relic entity (kept alive while carried via m_relicShared)
    std::shared_ptr<Unit> m_relicShared; // Keeps relic alive while monk carries it
    bool m_isMoving = false;
    bool m_pickedUp = false;
    static constexpr float PICKUP_RANGE = 16.f;
};
```

---

## Changes to `ActionPickupRelic.cpp`

### Replace `relic->kill()` in pickup block

**Old (lines 43–44):**
```cpp
m_pickedUp = true;
relic->kill(); // Remove relic from world
```

**New:**
```cpp
m_pickedUp = true;
// Hide the relic but keep it alive — take shared ownership
m_relicShared = relic;
relic->setVisible(false); // hide from map rendering
DBG << "Monk picked up relic (hidden, carried)";
```

`Entity::setVisible(bool)` — if this method does not exist, we need to add a `m_hidden` bool to `Entity` and check it in `UnitsRenderer`. See section below.

### Add destructor

```cpp
ActionPickupRelic::~ActionPickupRelic()
{
    // If monk is dying/dead and relic is still carried, drop it
    if (m_pickedUp && m_relicShared) {
        Unit::Ptr monk = m_unit.lock();
        MapPos dropPos = monk ? monk->position() : MapPos(0, 0);
        dropRelic(dropPos);
    }
}
```

### Add `dropRelic`

```cpp
void ActionPickupRelic::dropRelic(const MapPos &deathPosition)
{
    if (!m_relicShared) return;
    Unit::Ptr relic = m_relicShared;
    m_relicShared.reset(); // release shared ownership
    m_pickedUp = false;

    // Put relic back on the map at death position
    relic->setVisible(true);
    relic->setPosition(deathPosition, false);
    DBG << "Relic dropped at" << deathPosition;

    // Decrement player's relic count
    Unit::Ptr monk = m_unit.lock();
    if (monk) {
        auto owner = monk->player().lock();
        if (owner) {
            float current = owner->resourcesAvailable(genie::ResourceType::RelicsCaptured);
            if (current > 0) {
                owner->setAvailableResource(genie::ResourceType::RelicsCaptured, current - 1.f);
            }
        }
    }
}
```

---

## Adding `setVisible` / `isHidden` to `Entity`

### `src/mechanics/Entity.h`

Add to the public interface:

```cpp
bool isHidden() const noexcept { return m_hidden; }
void setVisible(bool visible) noexcept { m_hidden = !visible; }
```

Add to the protected/private members:

```cpp
bool m_hidden = false;
```

### `src/render/UnitsRenderer.cpp`

In the render loop where units are iterated, skip hidden units:

```cpp
if (unit->isHidden()) continue;
```

This should go at the top of the per-unit render block, just after the existing `isVisible` check.

---

## Wiring monk death to `dropRelic`

The cleanest hook is `Unit::kill()` or the death event path. Since `ActionPickupRelic` already stores a `weak_ptr<Unit>` to the monk via `m_unit`, the destructor fires when the action is removed. But actions are removed when the unit dies (see `UnitActionHandler`). So the destructor approach works correctly:

1. Monk dies → `Unit::kill()` → action queue cleared → `ActionPickupRelic` destructor runs → `dropRelic(monk->position())` called.

We need to call `dropRelic` with the monk's last position before the action is destroyed. To ensure the position is captured, override the destructor pattern:

Actually, the destructor receives `m_unit.lock()` which may return null if the unit is already being destroyed. To be safe, record the position when the monk takes fatal damage.

**Alternate approach:** override `onDamageTaken` event — but that is internal. Instead, intercept in `Unit::kill()`:

In `Unit.cpp`, in `Unit::kill()`, before clearing the action queue, find any `ActionPickupRelic` action and call `dropRelic`:

```cpp
// In Unit::kill():
for (const auto &action : actions.actionQueue()) {
    if (action->type == IAction::Type::PickupRelic) {
        auto *relicAction = static_cast<ActionPickupRelic*>(action.get());
        relicAction->dropRelic(position());
        break;
    }
}
```

This requires adding `IAction::Type::PickupRelic` if not already in the `Type` enum. It is already there (line 80 in `IAction.h`).

Also need to expose the action queue from `UnitActionHandler`. Add:

```cpp
// UnitActionHandler.h
const std::vector<ActionPtr> &actionQueue() const { return m_queue; }
```

---

## Testing

1. Spawn a monk and a relic. Order monk to pick up relic.
2. Kill the monk while it carries the relic.
3. Relic should appear at the monk's death position.
4. Order a second monk to pick up the dropped relic — should work normally.
5. Monk successfully garrisons in a monastery with relic — relic count should increment (existing behavior).
