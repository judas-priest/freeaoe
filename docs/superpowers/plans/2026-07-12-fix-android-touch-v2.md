# Fix Android Touch Controls v2

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make touch controls actually work on Android — tap selects, drag scrolls, pinch zooms, long-press commands.

**Architecture:** SDL2 on Android auto-emulates mouse from touch (SDL_MOUSEBUTTONDOWN/UP/MOTION). We rely on this for tap and drag. For pinch we use SDL_MULTIGESTURE. For long-press we track touch duration. The key insight: stop fighting SDL's mouse emulation — embrace it. Remove all `#ifdef ANDROID` special cases from mouse handlers. Instead, only customize: (1) disable edge-scroll, (2) convert drag into camera scroll, (3) add pinch zoom via SDL_RenderSetScale (NOT viewport resize).

**Tech Stack:** C++20, SDL2

**Root causes of current bugs:**
- `m_gameAreaHeight` defaults to 800 but is recalculated to a wrong value because UI overlay doesn't load for HD data
- Pinch zoom calls `setViewportSize()` which moves the camera viewport offset, not the render scale — units shift position instead of zooming
- `handleMouseMove` Android path checks `m_selecting` for drag, but m_selecting requires mousePress to trigger first, and mousePress requires `mousePos.y < m_gameAreaHeight` which fails
- Cursor is reset to null on Android but cursor position tracking is used for hover/selection

---

### Task 1: Remove broken Android special cases, let SDL mouse emulation work

**Files:**
- Modify: `src/Engine.cpp`

The current code has `#ifdef ANDROID` blocks in `handleMouseMove`, `handleMousePress`, `handleMouseRelease`, and `handleEvent` that interfere with SDL's mouse emulation. Remove them all and let the standard mouse handlers work.

- [ ] **Step 1: Remove `#ifdef ANDROID` block from handleMouseMove (around line 446-464)**

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

The standard mouse move handler with edge-scroll will now run on Android too. We'll disable edge-scroll differently in Task 2.

- [ ] **Step 2: Remove `#ifdef ANDROID` block from handleEvent touch case (around line 388-392)**

Change:
```cpp
    case input::Event::TouchBegan:
    case input::Event::TouchMoved:
    case input::Event::TouchEnded:
#ifdef ANDROID
        return true; // On Android, SDL emulates mouse from touch — ignore raw touch
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

We'll fix handleTouchEvent to work on Android in Task 3.

- [ ] **Step 3: Remove `#ifdef ANDROID` block from handleMouseRelease (around line 506-514)**

Change:
```cpp
    if (event.mouseButton.button == input::MouseButton::Left && m_selecting) {
#ifdef ANDROID
        if (m_selectionRect.width < 10 && m_selectionRect.height < 10) {
            state->unitManager()->onLeftClick(mousePos, renderTarget_->camera());
        }
#else
        state->unitManager()->selectUnits(m_selectionRect, renderTarget_->camera());
#endif
```

To:
```cpp
    if (event.mouseButton.button == input::MouseButton::Left && m_selecting) {
        state->unitManager()->selectUnits(m_selectionRect, renderTarget_->camera());
```

- [ ] **Step 4: Fix m_gameAreaHeight — use full screen height on Android**

In `Engine::setup()`, around line 627-632, change:
```cpp
    // Calculate game area height (screen minus UI overlay)
    if (m_uiOverlay && m_uiOverlay->size.isValid()) {
        m_gameAreaHeight = uiSize.height - m_uiOverlay->size.height + m_uiOverlayOffset;
    } else {
        m_gameAreaHeight = uiSize.height * 0.75f;
    }
```

To:
```cpp
#ifdef ANDROID
    m_gameAreaHeight = uiSize.height; // Full screen is game area on Android
#else
    if (m_uiOverlay && m_uiOverlay->size.isValid()) {
        m_gameAreaHeight = uiSize.height - m_uiOverlay->size.height + m_uiOverlayOffset;
    } else {
        m_gameAreaHeight = uiSize.height * 0.75f;
    }
#endif
```

- [ ] **Step 5: Restore mouse cursor on Android (needed for hover/selection internally)**

In `Engine::setup()`, around line 560-562, change:
```cpp
#ifdef ANDROID
    SDL_ShowCursor(SDL_DISABLE);
    m_mouseCursor.reset();
```

To:
```cpp
#ifdef ANDROID
    SDL_ShowCursor(SDL_DISABLE);
    // Keep m_mouseCursor alive — it's needed internally for hover detection
```

- [ ] **Step 6: Commit**

```bash
git add src/Engine.cpp
git commit -m "fix: remove broken Android special cases, let SDL mouse emulation handle taps"
```

---

### Task 2: Camera scroll from touch drag

**Files:**
- Modify: `src/Engine.cpp`

On Android, dragging should scroll the camera instead of edge-scroll or rect-select. We do this in handleTouchEvent (which gets FINGER events alongside the emulated MOUSE events). The touch handler scrolls the camera. The mouse handler handles clicks and selection.

- [ ] **Step 1: Rewrite handleTouchEvent for Android — only camera scroll**

Replace the entire `handleTouchEvent` method:

```cpp
bool Engine::handleTouchEvent(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    switch (event.type) {
    case input::Event::TouchBegan:
        m_touchState.active = true;
        m_touchState.startPos = ScreenPos(event.touch.x, event.touch.y);
        m_touchState.lastPos = m_touchState.startPos;
        m_touchState.startTime = currentTimeMs();
        m_touchState.dragging = false;
        return false; // Don't consume — let SDL mouse emulation also fire
    case input::Event::TouchMoved: {
        ScreenPos pos(event.touch.x, event.touch.y);
        if (!m_touchState.dragging) {
            if (m_touchState.startPos.distanceTo(pos) > TouchState::DRAG_THRESHOLD) {
                m_touchState.dragging = true;
            }
        }
        if (m_touchState.dragging) {
            ScreenPos delta = m_touchState.lastPos - pos;
            ScreenPos camScreen = renderTarget_->camera()->targetPosition().toScreen();
            camScreen.x += delta.x;
            camScreen.y += delta.y;
            MapPos camMap = camScreen.toMap().clamped(state->map()->pixelSize());
            renderTarget_->camera()->setTargetPosition(camMap);
        }
        m_touchState.lastPos = pos;
        return false; // Don't consume
    }
    case input::Event::TouchEnded:
        m_touchState.active = false;
        m_touchState.dragging = false;
        return false;
    default:
        return false;
    }
}
```

Key differences from before:
- Returns `false` (not `true`) — lets SDL mouse emulation events also fire
- Only does camera scroll in drag mode
- No synthesized clicks — SDL mouse emulation handles taps
- No long-press detection here — that's Task 4

- [ ] **Step 2: Disable edge-scroll when touch is active**

In `Engine::updateCamera()` (around line 697), add at the top:

```cpp
bool Engine::updateCamera(const std::shared_ptr<GameState> &state)
{
#ifdef ANDROID
    return false; // Camera controlled by touch drag, not edge-scroll
#endif
    if (m_cameraDeltaX == 0 && m_cameraDeltaY == 0) {
```

- [ ] **Step 3: Build and install**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

- [ ] **Step 4: Commit**

```bash
git add src/Engine.cpp
git commit -m "feat: camera scroll from touch drag, disable edge-scroll on Android"
```

---

### Task 3: Fix pinch zoom — use SDL_RenderSetScale, not viewport resize

**Files:**
- Modify: `src/Engine.cpp`

Current pinch zoom calls `camera()->setViewportSize()` which changes what map area the camera sees but doesn't scale the rendering. Units shift position. Instead, use `SDL_RenderSetScale()` which scales ALL rendering uniformly — sprites, terrain, UI, everything grows/shrinks.

- [ ] **Step 1: Replace PinchZoom handler**

In `handleEvent`, replace the PinchZoom case (around line 393-400):

```cpp
    case input::Event::PinchZoom: {
#ifdef USE_SDL2
        m_zoomLevel = std::clamp(m_zoomLevel + event.pinch.dDist * PINCH_SENSITIVITY, ZOOM_MIN, ZOOM_MAX);
        SDL_RenderSetScale(
            static_cast<SdlRenderTarget*>(renderTarget_.get())->renderer(),
            m_zoomLevel, m_zoomLevel
        );
#endif
        return true;
    }
```

This scales everything uniformly — same as mouse wheel zoom in other SDL games.

- [ ] **Step 2: Commit**

```bash
git add src/Engine.cpp
git commit -m "fix: pinch zoom uses SDL_RenderSetScale instead of viewport resize"
```

---

### Task 4: Long press for right-click (commands)

**Files:**
- Modify: `src/Engine.cpp`

SDL mouse emulation only generates left clicks from taps. For right-click (move/attack commands), we need long-press detection. Add a timer check in the main loop.

- [ ] **Step 1: Add long-press check in Engine::start() main loop**

After the event processing block (around line 220, after the `while(pollEvent)` loop), add:

```cpp
#ifdef ANDROID
        // Long press detection: if finger held > threshold without dragging, emit right click
        if (m_touchState.active && !m_touchState.dragging) {
            int64_t held = currentTimeMs() - m_touchState.startTime;
            if (held >= TouchState::LONG_PRESS_MS) {
                input::Event rclick;
                rclick.type = input::Event::MouseButtonPressed;
                rclick.mouseButton.button = input::MouseButton::Right;
                rclick.mouseButton.x = m_touchState.startPos.x;
                rclick.mouseButton.y = m_touchState.startPos.y;
                handleMousePress(rclick, state);
                rclick.type = input::Event::MouseButtonReleased;
                handleMouseRelease(rclick, state);
                m_touchState.active = false; // prevent re-triggering
                updated = true;
            }
        }
#endif
```

- [ ] **Step 2: Commit**

```bash
git add src/Engine.cpp
git commit -m "feat: long press generates right-click for unit commands on Android"
```

---

### Task 5: Fix touch coordinates for SDL_RenderSetLogicalSize

**Files:**
- Modify: `src/render/SdlRenderTarget.cpp`

SDL_FINGERDOWN coordinates are normalized 0.0-1.0 and NOT transformed by SDL_RenderSetLogicalSize. We multiply by window size, but we should multiply by logical size to match the game coordinate system.

- [ ] **Step 1: Use logical size for touch coordinates**

In `SdlWindow::pollEvent()`, for all three finger event cases (SDL_FINGERDOWN, SDL_FINGERUP, SDL_FINGERMOTION), change:

```cpp
        int w, h;
        SDL_RenderGetLogicalSize(sdlRenderer, &w, &h);
        if (w == 0 || h == 0) SDL_GetWindowSize(sdlWindow, &w, &h);
```

This is already done in the current code. Verify it's correct.

- [ ] **Step 2: Commit (if changes needed)**

```bash
git add src/render/SdlRenderTarget.cpp
git commit -m "fix: touch coordinates use logical render size"
```

---

## Summary

| Task | What | Why |
|------|------|-----|
| 1 | Remove all broken `#ifdef ANDROID` special cases | They interfere with SDL mouse emulation which already works |
| 2 | Camera scroll from FINGER drag events | Separate from mouse events — touch drags camera, mouse clicks select |
| 3 | Pinch zoom via SDL_RenderSetScale | Not viewport resize which shifts unit positions |
| 4 | Long press → right click in main loop | SDL only emulates left click from tap |
| 5 | Verify touch coordinates | Must match logical render size |
