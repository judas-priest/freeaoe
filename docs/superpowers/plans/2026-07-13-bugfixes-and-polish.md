# Bugfixes and Polish — 5 Issues

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix crash on Random Map, eliminate rendering lag, adjust button positions, replace double-tap with long-tap for right-click.

**Architecture:** Five independent fixes. Each produces a working build. Order: crash fix first, then input change (affects gameplay), then performance, then UI polish.

**Tech Stack:** C++20, SDL2, Android NDK

**Build:** `cd android && rm -rf app/.cxx app/build && unset LD_PRELOAD && ./gradlew assembleDebug`
**Install:** `unset LD_PRELOAD && adb install -r android/app/build/outputs/apk/debug/app-debug.apk`

---

### Task 1: Fix Random Map crash

**Problem:** Game crashes when starting a random map game.

**Root cause:** `Map::setupBasic(size)` creates tiles with `terrainId=0`, `elevation=0`, `frame=0`. `RandomMapGenerator::generateTerrain()` sets `terrainId` and `elevation`, but **never computes `tile.frame`** (the sub-tile variation index). `tile.frame` is normally set by `Map::updateMapData()` via `terrain->coordinatesToFrame(col, row)`. Without this, the renderer tries to use `frame=0` which may be invalid, crashing in `TerrainSprite::texture()`.

Additionally, `Map::setupBasic(int)` doesn't call `updateMapData()` or set up blend data — tiles have no blends, slopes, or yOffset.

**Files:**
- Modify: `src/mechanics/GameState.cpp:488` — call updateMapData after generate
- Modify: `src/mechanics/Map.h` — ensure updateMapData is public
- Modify: `src/mechanics/Map.cpp` — check updateMapData handles fresh maps

- [ ] **Step 1: Call map update after random map generation**

In `src/mechanics/GameState.cpp`, in `setupRandomMap()`, after `RandomMapGenerator::generate(...)`:

```cpp
    RandomMapGenerator::generate(settings, map_, *m_unitManager, m_players);

    // Compute tile frames, blends, slopes — required for rendering
    map_->updateTileBlends();

    // Center camera on human player's TC
```

Check what method computes tile frames. Search for where `coordinatesToFrame` is called on tiles:

In `Map.cpp`, `create()` (line ~200-250) iterates tiles and sets `tile.frame = terrain->coordinatesToFrame(col, row)`. This happens inside `create()` which loads from scenario data. For random maps, we need to do the same after generation.

Add after `RandomMapGenerator::generate(...)`:
```cpp
    // Compute tile frames for rendering (same as Map::create does for scenarios)
    for (int col = 0; col < map_->columnCount(); col++) {
        for (int row = 0; row < map_->rowCount(); row++) {
            MapTile &tile = map_->getTileAt(col, row);
            auto terrain = AssetManager::Inst()->getTerrain(tile.terrainId);
            if (terrain) {
                tile.frame = terrain->coordinatesToFrame(col, row);
            }
        }
    }
```

Need `#include "resource/AssetManager.h"` in GameState.cpp (check if already included).

- [ ] **Step 2: Build and test**

Test: Launch → Random Map → Arabia → Start → game should load without crash, terrain visible.

- [ ] **Step 3: Commit**

```bash
git commit -m "fix: compute tile frames after random map generation — prevents crash"
```

---

### Task 2: Replace double-tap with long-tap for right-click

**Problem:** Double-tap is used for right-click (move/attack), but in AoE2 double-click should select all units of same type. Long-tap (600ms) is more natural for right-click on mobile.

**Current behavior:**
- Double-tap on ground → `onRightClick` (move/attack) — at `Engine.cpp:1241`
- Long-press with units selected → context menu — at `Engine.cpp:287`

**New behavior:**
- Double-tap on unit → select all of same type on screen (already partially implemented at line 1252)
- Double-tap on ground → do nothing (just single-tap deferred deselect)
- Long-press on ground (with units selected) → `onRightClick` directly (move/attack)
- Long-press on unit → still show context menu for commands

**Files:**
- Modify: `src/Engine.cpp:283-299` — long-press handler
- Modify: `src/Engine.cpp:1234-1244` — double-tap handler

- [ ] **Step 1: Change double-tap to select-all-same-type only**

At `Engine.cpp`, replace the double-tap handler (around line 1234):

```cpp
        if (isDoubleTap) {
            // Double-tap = select all visible units of same type
            Unit::Ptr tappedUnit = state->unitManager()->unitAt(pos, renderTarget_->camera(), NoAlignment);
            if (tappedUnit) {
                // Select all of same type on screen
                int typeId = tappedUnit->data()->ID;
                int ownerId = tappedUnit->playerId();
                Size ss = renderTarget_->getSize();
                ScreenRect fullScreen(ScreenPos(0, 0), ScreenPos(ss.width, ss.height));
                state->unitManager()->selectUnits(fullScreen, renderTarget_->camera());
            }
            m_touchState.phase = TouchState::Phase::Idle;
            m_touchState.tapTime = 0;
            return true;
        }
```

Remove the `onRightClick` call from double-tap entirely.

- [ ] **Step 2: Change long-press to right-click (not context menu)**

At `Engine.cpp`, in the long-press detection (around line 283-299), replace context menu with direct right-click:

```cpp
        if (m_touchState.phase == TouchState::Phase::Pending) {
            int64_t held = currentTimeMs() - m_touchState.startTime;
            if (held >= TouchState::LONG_PRESS_MS) {
                if (!state->unitManager()->selected().isEmpty()) {
                    // Long-press with units selected = right click (move/attack)
                    state->unitManager()->onRightClick(m_touchState.startPos, renderTarget_->camera());
                    m_touchState.phase = TouchState::Phase::Idle;
                    updated = true;
                }
            }
        }
```

Remove the context menu creation from long-press. Context menu can still be accessed via action panel buttons.

- [ ] **Step 3: Build and test**

Test:
- Select unit → long-press ground → unit moves there
- Double-tap on a unit → all units of same type selected
- Single tap still works for selection

- [ ] **Step 4: Commit**

```bash
git commit -m "fix: long-press = right-click (move/attack), double-tap = select all same type"
```

---

### Task 3: Fix rendering performance / lags

**Problem:** Lag when many units/trees on screen. Three causes:

**Cause A: `createText()` called 3+ times every frame in drawUi():**
- `Engine.cpp:680` — ageText (every frame)
- `Engine.cpp:695` — clockText (every frame)
- `Engine.cpp:706` — scoreText (every frame)

Each `createText()` allocates a new `SdlText` with font lookup. On Android with `TTF_RenderUTF8_Blended`, this is >1ms per call.

**Cause B: hamburger menu icon drawn with 3 `draw(ScreenRect)` calls every frame (minor)**

**Cause C: PNG terrain loaded from disk on first access per tile variant (cold start only)**

**Files:**
- Modify: `src/Engine.h` — add persistent text objects for HUD
- Modify: `src/Engine.cpp` — use persistent text objects, update string only when changed

- [ ] **Step 1: Add persistent HUD text objects to Engine.h**

```cpp
    // Persistent HUD text (avoid createText() every frame)
    Drawable::Text::Ptr m_ageText;
    Drawable::Text::Ptr m_clockText;
    Drawable::Text::Ptr m_scoreText;
```

- [ ] **Step 2: Initialize in Engine::setup(), reuse in drawUi()**

In `Engine::setup()`, after renderTarget_ is created:
```cpp
    m_ageText = renderTarget_->createText(Drawable::Text::Plain);
    m_ageText->pointSize = 13;
    m_ageText->color = Drawable::Color(180, 160, 120, 255);

    m_clockText = renderTarget_->createText(Drawable::Text::Plain);
    m_clockText->pointSize = 12;
    m_clockText->color = Drawable::Color(150, 140, 110, 255);

    m_scoreText = renderTarget_->createText(Drawable::Text::Plain);
    m_scoreText->pointSize = 12;
    m_scoreText->color = Drawable::Color(150, 140, 110, 255);
```

In `drawUi()`, replace `auto ageText = renderTarget_->createText(...)` with:
```cpp
    m_ageText->string = ageNames[ageIdx];
    m_ageText->position = ScreenPos(ss.width - 200, 15);
    renderTarget_->draw(m_ageText);
```

Same for clock and score — reuse `m_clockText` and `m_scoreText`.

- [ ] **Step 3: Also cache resource icon draw calls**

The colored squares for resource icons (lines 571-577) are 4 `draw(ScreenRect)` calls per frame — these are cheap (just SDL_RenderFillRect). Keep as-is.

- [ ] **Step 4: Build and test**

Test: Load campaign with many trees/units → should feel smoother. Profile with fps counter.

- [ ] **Step 5: Commit**

```bash
git commit -m "perf: cache HUD text objects — eliminate 3x createText() per frame"
```

---

### Task 4: Shift action panel buttons left

**Problem:** Action panel buttons are 56px from left edge (one button-width gap). Should be flush-left with small padding.

**Root cause:** `ActionPanel.cpp:235` has `r.x = m_buttonSize` which equals 56 on Android.

**Files:**
- Modify: `src/ui/ActionPanel.cpp:235`

- [ ] **Step 1: Change r.x to small padding**

```cpp
#ifdef ANDROID
    r.x = 8; // Small padding from left edge
#else
    r.x = m_buttonSize;
#endif
```

- [ ] **Step 2: Build and test**

Test: Select unit → action buttons should be flush with left edge.

- [ ] **Step 3: Commit**

```bash
git commit -m "fix: action panel buttons flush-left on Android (was 56px offset)"
```

---

### Task 5: Remove double-tap deferred deselect conflict

**Problem:** After Task 2 changes (long-press = right-click), the deferred deselect at 700ms timeout in the game loop may conflict. When user long-presses (600ms), the deferred deselect timer (700ms) might fire right after and deselect units.

**Fix:** Cancel deferred deselect when long-press triggers:

**Files:**
- Modify: `src/Engine.cpp` — clear tapTime when long-press fires

- [ ] **Step 1: Clear pending tap on long-press**

In the long-press handler (Task 2 code), after `onRightClick`:
```cpp
    m_touchState.tapTime = 0; // Cancel deferred deselect
    m_touchState.phase = TouchState::Phase::Idle;
```

- [ ] **Step 2: Build and test**
- [ ] **Step 3: Commit**

```bash
git commit -m "fix: cancel deferred deselect when long-press right-click fires"
```

---

## Summary

| Task | What | Impact |
|------|------|--------|
| 1 | Fix Random Map crash (missing tile.frame computation) | BLOCKER |
| 2 | Long-press = right-click, double-tap = select all same type | Gameplay |
| 3 | Cache HUD text objects (3x createText per frame eliminated) | Performance |
| 4 | Action panel buttons flush-left | UI polish |
| 5 | Cancel deferred deselect on long-press | Input consistency |

All 5 tasks are independent. Task 2 and 5 should be committed together.
