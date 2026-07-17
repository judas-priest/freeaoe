# Plan: Minimap Modes (Normal / Economic / Diplomatic)

**Goal:** The `Minimap::MinimapMode` enum already exists with `Normal`, `Economic`, and `Diplomatic` values. Currently hardcoded to `Diplomatic` and only Diplomatic logic is implemented. Change `unitColor()` to produce correct colors per mode, and add a toggle button/key to cycle modes.

**Status:** Not started

---

## Current behavior (`Minimap.cpp` lines 151–169)

```cpp
Drawable::Color Minimap::unitColor(const std::shared_ptr<Unit> &unit)
{
    if (m_unitManager->selected().contains(unit)) {
        return Drawable::White;
    }

    switch(m_mode) {
    case MinimapMode::Diplomatic:
        if (unit->playerId() == UnitManager::GaiaID) {
            return Drawable::Color(128, 192, 128); // green for Gaia
        } else if (unit->playerId() == m_unitManager->humanPlayerID()) {
            return Drawable::Blue;
        }
        break;
    case MinimapMode::Economic: //TODO
    case MinimapMode::Normal: //TODO
    default:
        break;
    }

    return Drawable::Red; // enemy
}
```

---

## Step 1: Complete `unitColor()` for all modes

### Diplomatic mode (current — already correct)

- Gaia/nature: green `(128, 192, 128)`
- Human player: blue
- Ally: teal/cyan — check diplomacy state (not yet fully wired)
- Enemy: red

### Normal mode (unit type colors)

Color by unit class:
- Villager: yellow `(255, 220, 0)`
- Military: player color (blue for self, red for enemy)
- Building: same as military but slightly dimmer

### Economic mode

Color by resource type the unit is carrying:
- Wood: green `(0, 192, 0)`
- Food: yellow `(255, 200, 0)`
- Gold: `(255, 215, 0)`
- Stone: `(180, 180, 180)`
- Idle villager: bright white
- Military: same as Diplomatic

### Replacement for `unitColor()`:

```cpp
Drawable::Color Minimap::unitColor(const std::shared_ptr<Unit> &unit)
{
    if (m_unitManager->selected().contains(unit)) {
        return Drawable::White;
    }

    const bool isHuman = (unit->playerId() == m_unitManager->humanPlayerID());
    const bool isGaia  = (unit->playerId() == UnitManager::GaiaID);

    switch (m_mode) {
    case MinimapMode::Diplomatic:
    default:
        if (isGaia) return Drawable::Color(128, 192, 128);
        if (isHuman) return Drawable::Blue;
        // TODO: check ally/neutral when diplomacy map is available
        return Drawable::Red;

    case MinimapMode::Normal:
        if (isGaia) return Drawable::Color(128, 192, 128);
        if (unit->data()->Type == genie::Unit::CivilianType) {
            // Villager: yellow for own, orange for enemy
            return isHuman ? Drawable::Color(255, 220, 0) : Drawable::Color(200, 100, 0);
        }
        // Military / buildings: blue vs red
        return isHuman ? Drawable::Blue : Drawable::Red;

    case MinimapMode::Economic:
        if (isGaia) return Drawable::Color(128, 192, 128);
        if (!isHuman) {
            return Drawable::Red; // enemy stays red in economic mode
        }
        // Own units: color by carried resource
        {
            const ResourceMap &res = unit->resources;
            if (res[genie::ResourceType::WoodStorage]  > 0) return Drawable::Color(0, 192, 0);
            if (res[genie::ResourceType::FoodStorage]  > 0) return Drawable::Color(255, 200, 0);
            if (res[genie::ResourceType::GoldStorage]  > 0) return Drawable::Color(255, 215, 0);
            if (res[genie::ResourceType::StoneStorage] > 0) return Drawable::Color(180, 180, 180);
        }
        // Idle villager: bright white; military: blue
        if (unit->data()->Type == genie::Unit::CivilianType) {
            return Drawable::Color(255, 255, 100); // idle villager — yellowish
        }
        return Drawable::Blue;
    }
}
```

---

## Step 2: Add mode toggle

### `Minimap.h` — add public method

```cpp
void cycleMode();
MinimapMode mode() const { return m_mode; }
```

### `Minimap.cpp` — implement

```cpp
void Minimap::cycleMode()
{
    switch (m_mode) {
    case MinimapMode::Diplomatic: m_mode = MinimapMode::Economic;   break;
    case MinimapMode::Economic:   m_mode = MinimapMode::Normal;     break;
    case MinimapMode::Normal:     m_mode = MinimapMode::Diplomatic; break;
    }
    m_unitsUpdated = true; // force redraw
}
```

### `Engine.cpp` — keyboard toggle

In `handleKeyEvent`, add:

```cpp
case SDLK_F4: // AoE2 uses F4 to toggle minimap mode on some versions
    if (m_minimap) {
        m_minimap->cycleMode();
    }
    return true;
```

### Android: add a small tap area on the minimap

In `Minimap::handleEvent`, detect a tap in the top-left corner of the minimap rect:

```cpp
bool Minimap::handleEvent(input::Event event)
{
    if (event.type == input::Event::MouseButtonReleased) {
        ScreenPos pos(event.mouseButton.x, event.mouseButton.y);
        // Tap in upper-left 24×24 px of minimap rect toggles mode
        ScreenRect modeToggleRect(m_rect.x, m_rect.y, 24, 24);
        if (modeToggleRect.contains(pos)) {
            cycleMode();
            return true;
        }
        // ... existing minimap click-to-camera logic
    }
    // ... existing
}
```

---

## Step 3: Visual mode indicator (optional)

Draw a small label above the minimap showing the current mode:

In `Minimap::draw()`, after drawing the minimap:

```cpp
const char *modeName = "DIP";
switch (m_mode) {
case MinimapMode::Economic:   modeName = "ECO"; break;
case MinimapMode::Normal:     modeName = "NRM"; break;
case MinimapMode::Diplomatic: modeName = "DIP"; break;
}
// Draw tiny text at top-left of minimap
ScreenPos labelPos(m_rect.x + 2, m_rect.y - 12);
// ... use render target's text drawing
```

---

## Required includes in `Minimap.cpp`

Already included:
- `<genie/dat/Unit.h>` — for `genie::Unit::CivilianType`
- `mechanics/Unit.h` — for unit data access
- `core/ResourceMap.h` — for `ResourceMap`

Add if not present:

```cpp
#include <genie/dat/ResourceType.h>
```

---

## Testing

1. Start a game with villagers gathering different resources.
2. Press F4 — minimap should cycle: Diplomatic → Economic → Normal → Diplomatic.
3. In Economic mode: villagers carrying wood should be green, gold should be golden.
4. In Normal mode: villagers yellow, military blue/red.
5. In Diplomatic mode: own units blue, enemy red, Gaia green.
