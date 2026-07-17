# Plan: Building Damage Graphics (Fire Overlays + Rubble)

**Goal:** Wire `DamageGraphics` from the `.dat` into `GraphicRender`: check HP%, call `setDamageOverlay()` with the appropriate fire sprite when the building drops below each threshold. On destruction: play `DestructionGraphicID` (the collapse animation) and spawn rubble via `DopplegangerEntity`.

**Status:** Not started

---

## Background

`genie::Unit` already has:
- `std::vector<unit::DamageGraphic> DamageGraphics` — list of overlays with `GraphicID`, `DamagePercent`, and `ApplyMode` (0=overlay, 2=replace).
- `DyingGraphic` (int16_t) — played during death animation.
- `DeadUnitID` (int16_t) — unit to replace with after death.

`GraphicRender` already has:
- `void setDamageOverlay(const int spriteId) noexcept` — sets `m_damageOverlay`.
- `m_damageOverlay` is rendered in `GraphicRender::render()`.

`DopplegangerEntity` is already used for rubble — it spawns from a dying unit (see `Unit.h` line 309).

The gap: nothing calls `setDamageOverlay()` based on current HP. The damage overlay is never shown.

---

## Step 1: Wire damage overlay in `Unit::update` or `Unit::onDamageTaken`

### `src/mechanics/Unit.cpp`

`Unit::onDamageTaken()` is called every time the unit loses HP (private, line 306 in `Unit.h`). This is the right place.

In `Unit::onDamageTaken()`:

```cpp
void Unit::onDamageTaken()
{
    if (!m_data || m_data->DamageGraphics.empty()) {
        return;
    }

    const float healthPct = healthLeft() * 100.f; // 0–100%

    // Find the most severe damage graphic that applies at current HP%
    // DamageGraphics are sorted by DamagePercent ascending (most damaged = lowest HP%)
    int bestGraphicId = -1;
    for (const genie::unit::DamageGraphic &dg : m_data->DamageGraphics) {
        // DamagePercent is the HP% threshold BELOW which this graphic appears
        if (healthPct <= float(dg.DamagePercent)) {
            bestGraphicId = dg.GraphicID;
            // Don't break — take the last (most severe) matching entry
        }
    }

    // setDamageOverlay(-1) clears the overlay when healthy
    GraphicRender *gr = graphicRender(); // see below
    if (gr) {
        gr->setDamageOverlay(bestGraphicId);
    }
}
```

`graphicRender()` — we need access to the unit's `GraphicRender`. Units already hold a `GraphicRender` (it is managed by `Entity` or `Unit`). Check the exact field:

```
grep -n "GraphicRender\|m_graphic" src/mechanics/Entity.h src/mechanics/Unit.cpp
```

The render is accessed via `m_renderer` or similar in `UnitsRenderer`. Since `Unit::onDamageTaken()` is internal, the cleanest approach is to store a pointer to the unit's render object.

**Alternate simpler approach:** Call `setDamageOverlay` from `UnitsRenderer` during the render pass, where it already has access to both the unit's HP and its `GraphicRender`.

### In `src/render/UnitsRenderer.cpp`, in the per-unit update/render loop:

```cpp
// After computing healthPct for the health bar:
const float healthPct = unit->healthLeft() * 100.f;
if (!unit->data()->DamageGraphics.empty() && unit->isAlive()) {
    int overlayId = -1;
    for (const genie::unit::DamageGraphic &dg : unit->data()->DamageGraphics) {
        if (healthPct <= float(dg.DamagePercent)) {
            overlayId = dg.GraphicID;
        }
    }
    unitGraphic->setDamageOverlay(overlayId);
}
```

Where `unitGraphic` is the `GraphicRenderPtr` for this unit.

---

## Step 2: Play destruction graphic on death

`Building` already plays a dying animation via `DyingGraphic` on the entity. To play a destruction graphic explicitly, in `Unit.cpp` in the death path (after `kill()` is called), set the sprite to `DestructionGraphicID` if available.

In `Building.cpp` or `Unit.cpp` in `Unit::kill()`:

```cpp
// Play destruction graphic
if (m_data && m_data->Building.ConstructionGraphicID >= 0) {
    // Building-specific: use the destruction graphic from Building sub-struct
}
// For general units, DyingGraphic handles it already via the existing animation system
```

The dying animation is already handled by `DopplegangerEntity` — it spawns from the original unit with the death/rubble graphic. This existing code already reads `m_deadGraphic` (line 331 in `Unit.h`):

```cpp
int m_deadGraphic = -1;
```

Check `DopplegangerEntity::DopplegangerEntity(const Unit::Ptr &originalUnit)` — it populates `m_deadGraphic` from `data->DeadUnitID`. No changes needed here.

---

## Step 3: Spawn rubble after death animation completes

In `DopplegangerEntity::update(Time time)`:

```cpp
bool DopplegangerEntity::update(Time time) noexcept
{
    // After the dying animation finishes, switch to the rubble graphic
    // (which is the DeadUnitID graphic stored in m_deadGraphic)
    ...
}
```

This is already implemented for basic units. For buildings, the rubble graphic comes from `DeadUnitID`. Verify that `DopplegangerEntity` sets its sprite to `m_deadGraphic` when the death animation ends.

If not already done, in `DopplegangerEntity::onOriginalGone()`:

```cpp
void DopplegangerEntity::onOriginalGone()
{
    // Switch to rubble graphic
    if (m_deadGraphic >= 0) {
        m_graphic->setSprite(m_deadGraphic);
    }
}
```

---

## Step 4: Verify `setDamageOverlay(-1)` clears overlays on healing/resurrection

In `GraphicRender::setDamageOverlay`:

```cpp
void GraphicRender::setDamageOverlay(const int spriteId) noexcept
{
    if (spriteId < 0) {
        m_damageOverlay.reset();
        return;
    }
    if (!m_damageOverlay) {
        m_damageOverlay = std::make_shared<GraphicRender>();
    }
    m_damageOverlay->setSprite(spriteId);
}
```

This already handles `-1` correctly (check the existing implementation in `GraphicRender.cpp`).

---

## Required includes

In `UnitsRenderer.cpp` add:

```cpp
#include <genie/dat/unit/DamageGraphic.h>
```

This is already transitively included via `<genie/dat/Unit.h>`.

---

## Testing

1. Attack a town center until it drops below 50% HP — small fire graphic should appear.
2. Continue attacking below 25% HP — larger fire graphic should appear.
3. Destroy the TC — collapse animation plays, then rubble appears.
4. Heal the TC (repair villager) — fire overlay should disappear as HP rises.
