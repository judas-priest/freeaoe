# freeaoe Bugfix Batch — July 13 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix all gameplay-breaking bugs preventing comfortable play on both desktop and Android.

**Architecture:** 8 independent bugfix tasks. Each produces a working build. No dependencies between tasks.

**Tech Stack:** C++20, SDL2, Android NDK, genieutils

**Desktop build:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
**Android build:** `cd /home/dima/Projects/freeaoe/android && rm -rf app/.cxx app/build && unset LD_PRELOAD && ./gradlew assembleDebug`
**Desktop run:** `./freeaoe "--game-path=/home/dima/Desktop/Age.of.Empires.2.HD.Edition.v5.8.10-Rutracker/" --single-player`

**References:**
- [OpenAge selection docs](https://simonsan.github.io/openage-webdocs/sphinx/doc/reverse_engineering/game_mechanics/selection.html) — AoE2 uses tolerance radius, not sprite rect
- [OpenAge SLP docs](https://github.com/SFTtech/openage/blob/master/doc/media/slp-files.md) — hotspot is center of sprite
- [AoE2 hitbox bug report](https://forums.ageofempires.com/t/clickable-area-hitbox-for-natural-resources-and-units-is-a-screen-space-rectangle-aabb-should-behave-like-buildings/247241) — AABB vs outline discussion

---

## Bug Summary

| # | Bug | Status | Severity |
|---|-----|--------|----------|
| 1 | Building placement allows overlap with trees/buildings | Done in code, needs test | Critical |
| 2 | Unit hitbox shifted NW — must click northwest of sprite | **TODO** | Critical |
| 3 | Black squares on random maps (elevation/slopes) | Done in code | Critical |
| 4 | Age icon wrong on Android (IV instead of II) | **TODO — needs investigation** | Medium |
| 5 | HTML tags visible in tooltip text | Done in code | Low |
| 6 | Desktop: mouse clicks don't select units (gameAreaHeight=0) | Done in code | Critical |
| 7 | Desktop: SDL logical size mismatch after ScenarioBrowser | Done in code | Medium |
| 8 | Debug mouse coordinates shown on screen | Done in code | Low |

---

### Task 1: Building placement collision (DONE — verify)

**Status:** Code written. `Building::canPlace()` now checks `map->entitiesAt()` for each tile. `width`/`height` use `std::ceil` with minimum 2. Red circle shown when blocked.

**Files:** `src/mechanics/Building.cpp`, `src/render/UnitsRenderer.cpp`

- [x] Step 1: Entity collision check in canPlace
- [x] Step 2: Red placement circle when blocked
- [ ] **Step 3: Verify on desktop** — build house, try place on tree → blocked
- [ ] **Step 4: Commit**

---

### Task 2: Unit hitbox — fix click detection (TODO)

**Problem:** `UnitManager::unitAt()` uses `unit->screenRect()` (sprite bounding box) offset by hotspot. The hotspot-based rect doesn't match rendered position — clicking directly on a sheep/villager misses, must click northwest.

**Root cause:** `GraphicRender::rect()` returns `(-hotspot.x, -hotspot.y, width, height)`. This rect represents the sprite frame in screen space relative to the unit's map position projected to screen. But the sprite hotspot is the "foot" anchor — so the rect extends from the top-left corner of the sprite frame. If the hotspot doesn't match the visual center of the unit, clicking the visual center misses.

**Real AoE2 approach (from OpenAge RE docs):** The game uses a tolerance area around the unit center based on `OutlineSize`, not the sprite bounding box. The tolerance is roughly 1 unit width horizontally and 1 unit height vertically from center.

**Fix:** Replace sprite-rect hit test with OutlineSize-based ellipse test centered on the unit's screen position.

**Files:**
- Modify: `src/mechanics/UnitManager.cpp:1019-1053`

- [ ] **Step 1: Replace unitAt with ellipse-based hit test**

In `src/mechanics/UnitManager.cpp`, replace the `unitAt` method:

```cpp
Unit::Ptr UnitManager::unitAt(const ScreenPos &pos, const CameraPtr &camera, const PlayerAlignment alignment) const
{
    Player::Ptr humanPlayer = m_humanPlayer.lock();

    Unit::Ptr bestUnit;
    double bestDist = 999999;

    std::reverse_iterator<UnitVector::const_iterator> unitIterator;
    for (unitIterator = m_units.rbegin(); unitIterator != m_units.rend(); unitIterator++) {
        Unit::Ptr unit = *unitIterator;
        if (!unit->isVisible) {
            continue;
        }

        switch(alignment) {
        case Allied:
            if (!humanPlayer->isAllied(unit->playerId())) continue;
            break;
        case Enemy:
            if (humanPlayer->isAllied(unit->playerId())) continue;
            break;
        case NoAlignment:
        default:
            break;
        }

        const ScreenPos unitPosition = camera->absoluteScreenPos(unit->position());

        // Use OutlineSize for click radius (matches selection circle)
        double radiusX = unit->data()->OutlineSize.x * Constants::TILE_SIZE_HORIZONTAL / 2.;
        double radiusY = unit->data()->OutlineSize.y * Constants::TILE_SIZE_VERTICAL / 2.;
        // Minimum clickable radius for small units (sheep, relics, etc.)
        radiusX = std::max(radiusX, 15.0);
        radiusY = std::max(radiusY, 15.0);

        // Ellipse hit test: ((dx/rx)^2 + (dy/ry)^2) <= 1.0
        double dx = (pos.x - unitPosition.x) / radiusX;
        double dy = (pos.y - unitPosition.y) / radiusY;
        double dist = dx * dx + dy * dy;

        if (dist <= 1.0 && dist < bestDist) {
            bestDist = dist;
            bestUnit = unit;
        }
    }

    return bestUnit;
}
```

Key changes:
- Uses `OutlineSize` (same data as selection circle) instead of sprite rect
- Ellipse test centered on unit's actual map→screen position
- Finds closest unit when overlapping
- Minimum 15px radius so tiny units are still clickable

- [ ] **Step 2: Test on desktop**

Click directly on sheep → selects. Click on villager body → selects. Click far from unit → nothing selected.

- [ ] **Step 3: Commit**

```bash
git add src/mechanics/UnitManager.cpp
git commit -m "fix: unit click uses OutlineSize ellipse, not sprite rect — fixes NW offset"
```

---

### Task 3: Black squares on random maps (DONE — verify)

**Status:** `RandomMapGenerator::generateTerrain` sets `tile.elevation = 2` (flat) instead of noise.

**Files:** `src/mechanics/RandomMapGenerator.cpp`

- [x] Step 1: Flat elevation
- [ ] **Step 2: Verify on desktop** — random map has no black tiles
- [ ] **Step 3: Commit**

---

### Task 4: Age icon wrong on Android (TODO — needs investigation)

**Problem:** HD Edition dat file has IconID=30/31/32 for age techs (Feudal/Castle/Imperial). On desktop these display correctly. On Android, icon at index 30 shows "IV" instead of "II".

**Hypothesis:** The Technology SLP loaded on Android may have different frame ordering, or the SLP file itself is different.

**Files:**
- Investigate: `src/resource/AssetManager_HD.h` — `getInterfaceSlp(StandardSlpType::Technology)`
- Modify: `src/ui/ActionPanel.cpp` (after investigation)

- [ ] **Step 1: Add debug logging on Android**

```cpp
#ifdef __ANDROID__
        if (tech->Type == 2) {
            __android_log_print(ANDROID_LOG_INFO, "FreeAoE",
                "Age tech: IconID=%d EffectID=%d ResearchTime=%d icons=%zu",
                tech->IconID, tech->EffectID, tech->ResearchTime, m_researchIcons.size());
        }
#endif
```

- [ ] **Step 2: Build Android, run, check logcat**

```bash
adb logcat -s "FreeAoE" | grep "Age tech"
```

- [ ] **Step 3: Compare Technology SLP on Android vs desktop**

Check which SLP file is loaded for `StandardSlpType::Technology`. Dump frame count and first few frame sizes. If different SLP → fix asset path. If same SLP but wrong frame order → map iconId.

- [ ] **Step 4: Apply fix based on findings, commit**

---

### Task 5: HTML tags in tooltips (DONE)

**Files:** `src/Engine.cpp` — `drawUi()` help text section

- [x] Step 1: Strip `<b>`, `</b>`, `<i>`, `</i>`, `\\n`
- [ ] **Step 2: Commit**

---

### Task 6: Desktop gameAreaHeight=0 (DONE)

**Files:** `src/Engine.cpp:1530`

- [x] Step 1: Use `m_uiOverlayOffset` directly with fallback
- [ ] **Step 2: Commit**

---

### Task 7: SDL logical size mismatch (DONE)

**Files:** `src/Engine.cpp:1506`

- [x] Step 1: `SDL_RenderSetLogicalSize(renderer, 0, 0)` on desktop
- [ ] **Step 2: Commit**

---

### Task 8: Debug mouse coordinates (DONE)

**Files:** `src/ui/MouseCursor.cpp`

- [x] Step 1: Removed `m_cursor_pos_text` creation and drawing
- [ ] **Step 2: Commit**

---

## Remaining Work After This Batch

These are NOT bugs but missing features / polish for future plans:

1. **Performance** — PNG terrain loaded per tile from disk, createText() every frame, no sprite batching
2. **Elevation rendering** — slopes produce black tiles (disabled for now, needs filtermap debugging)
3. **Fog of war gaps** — some tiles near units remain unexplored (visibility radius edge case)
4. **AI building overlap** — BasicAI has collision checks but still needs work
5. **Selection circle size** — buildings vs units, already partially fixed
