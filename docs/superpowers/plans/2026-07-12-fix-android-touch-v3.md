# Fix Android Touch Controls v3 (verified)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make touch controls work correctly on Android — tap selects, drag scrolls camera, pinch zooms, long-press gives commands.

**Architecture:** Disable SDL's mouse-from-touch emulation with `SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH=1`. Handle ALL input via `SDL_FINGERDOWN/MOTION/UP` + `SDL_MULTIGESTURE`. Convert normalized touch coords (0-1) to window pixels, then to logical coords. No mouse events on Android at all.

**Tech Stack:** C++20, SDL2

**Verified facts from search:**
- `SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH` = "1" separates mouse and touch (SDL 2.0.4+, stackoverflow.com/questions/34465681)
- `SDL_FINGERDOWN` coords are normalized 0.0-1.0, multiply by WINDOW size (lazyfoo.net/tutorials/SDL/54_touches)
- `SDL_RenderSetLogicalSize` does NOT transform finger coords (only mouse coords)
- `SDL_RenderSetScale` affects positions but NOT texture sizes — wrong for zoom (reddit.com/r/sdl)
- `SDL_MULTIGESTURE` with `mgesture.dDist` threshold ~0.002 works for pinch (gravity-defied-mod)
- For zoom in 2D SDL: adjust camera viewport + scale dstrect at render time, or just use viewport-based zoom via `Camera::setViewportSize`
- Long press: no SDL built-in — track timer manually, standard approach

---

### Task 1: Separate mouse and touch, handle ONLY finger events on Android

**Files:**
- Modify: `src/Engine.cpp`
- Modify: `src/render/SdlRenderTarget.cpp`

- [ ] **Step 1: Set SDL hint in Engine::setup() before window creation**

In `Engine::setup()`, right after `SDL_SetHint(SDL_HINT_ORIENTATIONS, ...)`, add:

```cpp
    // Separate mouse and touch — we handle touch via FINGER events only
    SDL_SetHint(SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH, "1");
```

- [ ] **Step 2: Fix touch coordinate conversion in SdlWindow::pollEvent**

Touch coords are normalized 0-1, must multiply by WINDOW size (not logical size). Then convert to logical coords manually if SDL_RenderSetLogicalSize is active.

Replace the three finger event handlers (SDL_FINGERDOWN, SDL_FINGERUP, SDL_FINGERMOTION) with:

```cpp
    case SDL_FINGERDOWN: {
        event.type = input::Event::TouchBegan;
        int w, h;
        SDL_GetWindowSize(sdlWindow, &w, &h);  // ALWAYS window size, not logical
        event.touch.finger = static_cast<int>(sdlEvent.tfinger.fingerId);
        // Convert normalized (0-1) to window pixels
        int px = static_cast<int>(sdlEvent.tfinger.x * w);
        int py = static_cast<int>(sdlEvent.tfinger.y * h);
        // Convert window pixels to logical coords (if logical size is set)
        int lw, lh;
        SDL_RenderGetLogicalSize(sdlRenderer, &lw, &lh);
        if (lw > 0 && lh > 0) {
            event.touch.x = px * lw / w;
            event.touch.y = py * lh / h;
        } else {
            event.touch.x = px;
            event.touch.y = py;
        }
        return true;
    }
```

Same pattern for SDL_FINGERUP and SDL_FINGERMOTION.

- [ ] **Step 3: Route touch events to handleTouchEvent on Android (remove `return true` ignore)**

In `Engine::handleEvent`, change:
```cpp
    case input::Event::TouchBegan:
    case input::Event::TouchMoved:
    case input::Event::TouchEnded:
#ifdef ANDROID
        return true; // REMOVE THIS
#else
        return handleTouchEvent(event, state);
#endif
```

To:
```cpp
    case input::Event::TouchBegan:
    case input::Event::TouchMoved:
    case input::Event::TouchEnded:
        return handleTouchEvent(event, state);
```

- [ ] **Step 4: Commit**

```bash
git add src/Engine.cpp src/render/SdlRenderTarget.cpp
git commit -m "fix: separate mouse/touch on Android, fix touch coord conversion"
```

---

### Task 2: Rewrite handleTouchEvent — tap selects, drag scrolls, long-press commands

**Files:**
- Modify: `src/Engine.cpp`

Single touch handler that does everything:
- **Tap (< 350ms, < 25px movement):** Left click at touch position → selects unit
- **Drag (> 25px movement):** Camera scroll (delta between frames)
- **Long press (> 350ms, < 25px movement):** Right click at touch position → commands unit

- [ ] **Step 1: Rewrite handleTouchEvent**

```cpp
bool Engine::handleTouchEvent(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    switch (event.type) {
    case input::Event::TouchBegan: {
        m_touchState.active = true;
        m_touchState.startPos = ScreenPos(event.touch.x, event.touch.y);
        m_touchState.lastPos = m_touchState.startPos;
        m_touchState.startTime = currentTimeMs();
        m_touchState.dragging = false;

        // Notify UnitManager of cursor position for hover
        state->unitManager()->onMouseMove(renderTarget_->camera()->absoluteMapPos(m_touchState.startPos));
        return true;
    }
    case input::Event::TouchMoved: {
        ScreenPos pos(event.touch.x, event.touch.y);

        if (!m_touchState.dragging) {
            if (m_touchState.startPos.distanceTo(pos) > TouchState::DRAG_THRESHOLD) {
                m_touchState.dragging = true;
            }
        }

        if (m_touchState.dragging) {
            // Camera scroll: move map opposite to finger direction
            ScreenPos delta = m_touchState.lastPos - pos;
            ScreenPos camScreen = renderTarget_->camera()->targetPosition().toScreen();
            camScreen.x += delta.x;
            camScreen.y += delta.y;
            MapPos camMap = camScreen.toMap().clamped(state->map()->pixelSize());
            renderTarget_->camera()->setTargetPosition(camMap);
        }

        m_touchState.lastPos = pos;
        return true;
    }
    case input::Event::TouchEnded: {
        if (!m_touchState.dragging) {
            int64_t duration = currentTimeMs() - m_touchState.startTime;
            ScreenPos pos(event.touch.x, event.touch.y);

            if (duration >= TouchState::LONG_PRESS_MS) {
                // Long press = right click (move/attack command)
                state->unitManager()->onRightClick(pos, renderTarget_->camera());
            } else {
                // Short tap = left click (select unit)
                state->unitManager()->onLeftClick(pos, renderTarget_->camera());
            }
        }
        m_touchState.active = false;
        m_touchState.dragging = false;
        return true;
    }
    default:
        return false;
    }
}
```

Key differences from all previous attempts:
- Calls `UnitManager::onLeftClick` / `onRightClick` DIRECTLY — no synthesized mouse events, no handleMousePress/handleMouseRelease which interfere with m_selecting
- Camera scroll uses `lastPos - pos` delta (finger moves right → map goes left, like dragging a physical map)
- `onMouseMove` called only on TouchBegan for initial hover, not every frame

- [ ] **Step 2: Remove `#ifdef ANDROID` block from handleMouseMove**

Delete the entire block:
```cpp
#ifdef ANDROID
    // On Android, drag = scroll camera (like Google Maps)
    if (m_selecting) {
        ...
    }
    if (mousePos.y < m_gameAreaHeight) {
        state->unitManager()->onMouseMove(renderTarget_->camera()->absoluteMapPos(mousePos));
    }
    return false;
#endif
```

Mouse move is no longer used on Android (we use touch events directly).

- [ ] **Step 3: Remove `#ifdef ANDROID` block from handleMouseRelease**

Remove the `#ifdef ANDROID` block around selectUnits, restore original:
```cpp
    if (event.mouseButton.button == input::MouseButton::Left && m_selecting) {
        state->unitManager()->selectUnits(m_selectionRect, renderTarget_->camera());
        m_selectionRect = ScreenRect();
        m_selecting = false;
        return true;
    }
```

- [ ] **Step 4: Disable edge-scroll on Android**

In `Engine::updateCamera()`, add at top:
```cpp
#ifdef ANDROID
    return false;
#endif
```

- [ ] **Step 5: Set m_gameAreaHeight to full screen on Android**

Already done in current code — verify it says:
```cpp
#ifdef ANDROID
    m_gameAreaHeight = uiSize.height;
#else
```

- [ ] **Step 6: Commit**

```bash
git add src/Engine.cpp
git commit -m "feat: rewrite touch handler — direct UnitManager calls, no synthesized mouse events"
```

---

### Task 3: Pinch zoom via camera viewport

**Files:**
- Modify: `src/Engine.cpp`

Use `Camera::setViewportSize` for zoom. This changes what portion of the map is visible — smaller viewport = zoomed in (fewer tiles but bigger on screen), larger viewport = zoomed out. This is how AoE2's camera naturally works. Don't use `SDL_RenderSetScale` (doesn't scale textures).

- [ ] **Step 1: Store base viewport size**

In `Engine.h`, add member:
```cpp
    Size m_baseViewportSize;
```

In `Engine::setup()`, after `renderTarget_->setSize(uiSize)`, save:
```cpp
    m_baseViewportSize = uiSize;
```

- [ ] **Step 2: Fix PinchZoom handler**

Replace current handler:
```cpp
    case input::Event::PinchZoom: {
        m_zoomLevel = std::clamp(m_zoomLevel + event.pinch.dDist * PINCH_SENSITIVITY, ZOOM_MIN, ZOOM_MAX);
        Size viewportSize(m_baseViewportSize.width / m_zoomLevel, m_baseViewportSize.height / m_zoomLevel);
        renderTarget_->camera()->setViewportSize(viewportSize);
        return true;
    }
```

This works because Camera::setViewportSize changes the visible area. When viewport is half the screen size, everything appears 2x bigger (zoomed in). When viewport is double, everything appears half size (zoomed out).

- [ ] **Step 3: Commit**

```bash
git add src/Engine.h src/Engine.cpp
git commit -m "feat: pinch zoom via camera viewport resize"
```

---

### Task 4: Hide cursor, keep MouseCursor for internal use

**Files:**
- Modify: `src/Engine.cpp`

Current code resets m_mouseCursor to null on Android which breaks hover detection. Keep the object alive but hide the SDL system cursor.

- [ ] **Step 1: Don't reset m_mouseCursor**

In `Engine::setup()`, change:
```cpp
#ifdef ANDROID
    SDL_ShowCursor(SDL_DISABLE);
    m_mouseCursor.reset();
```

To:
```cpp
#ifdef ANDROID
    SDL_ShowCursor(SDL_DISABLE);
    // Don't reset m_mouseCursor — needed for internal hover/cursor type tracking
```

- [ ] **Step 2: Skip rendering cursor on Android but keep update**

In `drawUi()`, change:
```cpp
    if (m_mouseCursor) m_mouseCursor->render();
```

To:
```cpp
#ifndef ANDROID
    if (m_mouseCursor) m_mouseCursor->render();
#endif
```

- [ ] **Step 3: Commit**

```bash
git add src/Engine.cpp
git commit -m "fix: keep MouseCursor alive on Android for hover detection, hide rendering"
```

---

### Task 5: Build, install, verify

- [ ] **Step 1: Build**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
```

- [ ] **Step 2: Install**

```bash
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

User launches app manually.

- [ ] **Step 3: Verify checklist**

- Tap on unit → unit gets selected (blue highlight)
- Drag finger → camera scrolls (map moves opposite to finger, like dragging paper)
- Pinch two fingers apart → zoom in (things get bigger)
- Pinch two fingers together → zoom out (things get smaller)
- Select unit, then long-press on ground → unit moves there
- No white cursor arrow visible
- UI buttons (top bar) respond to tap
- Minimap responds to tap

---

## Summary

| Task | What | Key insight |
|------|------|-------------|
| 1 | Separate mouse/touch, fix coords | `SDL_HINT_ANDROID_SEPARATE_MOUSE_AND_TOUCH=1`, coords × window size then ÷ to logical |
| 2 | Rewrite touch handler | Call UnitManager directly, no synthesized mouse events |
| 3 | Pinch zoom via viewport | `Camera::setViewportSize` — smaller viewport = zoomed in |
| 4 | Cursor: hide render, keep object | MouseCursor needed for hover detection internally |
| 5 | Build + install + verify | Manual testing checklist |
