# freeaoe Android Touch Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix all touch interaction issues and UI scaling for the Android build of freeaoe.

**Architecture:** All changes in Engine.cpp touch handling, SdlRenderTarget.cpp event conversion, and EventTypes.h. No new files. The approach: (1) add pinch-to-zoom via SDL_MULTIGESTURE, (2) fix click-to-select by removing drag threshold interference, (3) hide cursor on Android, (4) fix long-press for right-click, (5) replace hardcoded 800px UI threshold with dynamic screen height.

**Tech Stack:** C++20, SDL2, Android NDK

**Screen:** OnePlus Ace 5 Ultra — landscape 2800x1272 (logical), game renders at UI overlay resolution (1280x1024 or similar)

---

## File Map

| Action | File | Responsibility |
|--------|------|----------------|
| Modify | `src/render/EventTypes.h` | Add PinchZoom event type |
| Modify | `src/render/SdlRenderTarget.cpp` | Handle SDL_MULTIGESTURE → PinchZoom event |
| Modify | `src/Engine.h` | Add zoom state, remove hardcoded 800 |
| Modify | `src/Engine.cpp` | Pinch zoom, fix tap-to-select, hide cursor, fix long press, dynamic UI threshold |

---

### Task 1: Add pinch-to-zoom via SDL_MULTIGESTURE

**Files:**
- Modify: `src/render/EventTypes.h`
- Modify: `src/render/SdlRenderTarget.cpp`
- Modify: `src/Engine.h`
- Modify: `src/Engine.cpp`

AoE2 original doesn't have zoom, but on a phone screen it's essential. We'll scale the camera viewport — smaller viewport = zoomed in, larger = zoomed out.

- [ ] **Step 1: Add PinchZoom event to EventTypes.h**

Add to the `Event::Type` enum after `TouchEnded`:
```cpp
        PinchZoom,     // two-finger pinch gesture
```

Add a new struct inside `Event`:
```cpp
    struct PinchEvent {
        float dDist = 0; // positive = zoom in (fingers apart), negative = zoom out
    };
```

Add the member:
```cpp
    PinchEvent pinch;
```

- [ ] **Step 2: Handle SDL_MULTIGESTURE in SdlRenderTarget.cpp pollEvent**

After the `SDL_FINGERMOTION` case (around line 1005), add:

```cpp
    case SDL_MULTIGESTURE: {
        if (sdlEvent.mgesture.numFingers == 2) {
            event.type = input::Event::PinchZoom;
            event.pinch.dDist = sdlEvent.mgesture.dDist;
            return true;
        }
        continue;
    }
```

Note: `continue` instead of `break` because if numFingers != 2 we want to poll the next event.

- [ ] **Step 3: Add zoom state to Engine.h**

Add to Engine private members (after `m_touchState`):

```cpp
    float m_zoomLevel = 1.0f;
    static constexpr float ZOOM_MIN = 0.5f;
    static constexpr float ZOOM_MAX = 3.0f;
    static constexpr float PINCH_SENSITIVITY = 8.0f;
```

- [ ] **Step 4: Handle PinchZoom in Engine.cpp handleEvent**

In `Engine::handleEvent`, in the switch statement, add a case:

```cpp
    case input::Event::PinchZoom: {
        float delta = event.pinch.dDist * PINCH_SENSITIVITY;
        m_zoomLevel = std::clamp(m_zoomLevel + delta, ZOOM_MIN, ZOOM_MAX);
        // Adjust camera viewport — smaller viewport = more zoomed in
        Size screenSize = renderTarget_->getSize();
        Size viewportSize(screenSize.width / m_zoomLevel, screenSize.height / m_zoomLevel);
        renderTarget_->camera()->setViewportSize(viewportSize);
        return true;
    }
```

- [ ] **Step 5: Also suppress touch events during pinch**

In `handleTouchEvent`, at the very top, add:

```cpp
    // Ignore touch events while pinching (2+ fingers)
    if (event.touch.finger > 0) {
        return true; // consume but ignore non-primary fingers
    }
```

- [ ] **Step 6: Build and install**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

- [ ] **Step 7: Commit**

```bash
cd /home/dima/Projects/freeaoe && git add src/render/EventTypes.h src/render/SdlRenderTarget.cpp src/Engine.h src/Engine.cpp
git commit -m "feat: pinch-to-zoom on Android via SDL_MULTIGESTURE"
```

---

### Task 2: Fix tap-to-select (unit click not working)

**Files:**
- Modify: `src/Engine.cpp`

Problem: When user taps a unit, the touch handler generates a left click via `handleMousePress` + `handleMouseRelease`. But `handleMousePress` starts rectangle selection (`m_selecting = true`) which interferes. On desktop this works because mouse-up quickly follows mouse-down. On touch, the synthesized press+release happen in the same frame but the selection rect is 0-size.

The real issue: `handleMousePress` at line 621 checks `mousePos.y < 800` — this is hardcoded for 1024px screen. On the phone screen (1272px height) the UI panel is at a different Y. Clicks on units fail because they're below y=800 in screen coords.

- [ ] **Step 1: Replace hardcoded 800 with dynamic game area height**

In `Engine.h`, add a private member:

```cpp
    float m_gameAreaHeight = 800.f; // Y threshold: above = game, below = UI panel
```

- [ ] **Step 2: Calculate m_gameAreaHeight in Engine::setup()**

After `renderTarget_->setSize(uiSize)` (around line 868), add:

```cpp
    // Game area is screen height minus UI overlay height at bottom
    if (m_uiOverlay && m_uiOverlay->size.isValid()) {
        m_gameAreaHeight = uiSize.height - m_uiOverlay->size.height + m_uiOverlayOffset;
    } else {
        m_gameAreaHeight = uiSize.height * 0.75f; // fallback: 75% of screen is game area
    }
```

- [ ] **Step 3: Replace all `800` references with `m_gameAreaHeight`**

In `Engine.cpp`, find and replace these lines:

Line ~567: `if (mousePos.y < 800)` → `if (mousePos.y < m_gameAreaHeight)`
Line ~598: `if (mousePos.y < 800)` → `if (mousePos.y < m_gameAreaHeight)`
Line ~621: `if (mousePos.y < 800 && ...` → `if (mousePos.y < m_gameAreaHeight && ...`
Line ~713: `if (mousePos.y < 800 && ...` → `if (mousePos.y < m_gameAreaHeight && ...`

Use search: there are exactly 4 occurrences of `< 800` in Engine.cpp (excluding the `case 800:` in the UI overlay switch which is unrelated).

- [ ] **Step 4: Fix tap selection — don't start rect selection on touch tap**

In `handleMousePress`, the `m_selecting = true` logic should be skipped when the click came from a touch tap (it's a point-click, not a drag-select). Add a guard:

```cpp
    if (mousePos.y < m_gameAreaHeight && event.mouseButton.button == input::MouseButton::Left) {
        if (state->unitManager()->onLeftClick(ScreenPos(event.mouseButton.x, event.mouseButton.y), renderTarget_->camera())) {
            return true;
        }

#ifndef ANDROID
        // Rectangle selection only on desktop (touch uses tap-to-select)
        m_selectionStart = mousePos;
        m_selectionCurr = mousePos + ScreenPos(1, 1);
        m_selecting = true;
#endif
    }
```

- [ ] **Step 5: Build and install**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

- [ ] **Step 6: Commit**

```bash
git add src/Engine.h src/Engine.cpp
git commit -m "fix: tap-to-select units, dynamic game area threshold replacing hardcoded 800px"
```

---

### Task 3: Hide mouse cursor on Android

**Files:**
- Modify: `src/Engine.cpp`

The white arrow cursor is visible on the screenshot. On Android there's no mouse — hide it.

- [ ] **Step 1: Always hide cursor on Android**

In `Engine::setup()`, find the cursor block (around line 778):

```cpp
    m_mouseCursor = std::make_unique<MouseCursor>(renderTarget_);
#ifdef USE_SDL2
    if (m_mouseCursor->isValid()) {
        SDL_ShowCursor(SDL_DISABLE);
    }
```

Change to:
```cpp
    m_mouseCursor = std::make_unique<MouseCursor>(renderTarget_);
#ifdef ANDROID
    SDL_ShowCursor(SDL_DISABLE);
    // Don't render custom cursor on Android either
    m_mouseCursor.reset();
#elif defined(USE_SDL2)
    if (m_mouseCursor->isValid()) {
        SDL_ShowCursor(SDL_DISABLE);
    }
```

- [ ] **Step 2: Guard cursor render and update calls**

In `Engine::drawUi()`, the cursor is rendered at the end:
```cpp
    m_mouseCursor->render();
```

Wrap it:
```cpp
    if (m_mouseCursor) {
        m_mouseCursor->render();
    }
```

In `Engine::updateUi()`, find:
```cpp
    updated = m_mouseCursor->update(state->unitManager()) || updated;
```

Wrap it:
```cpp
    if (m_mouseCursor) {
        updated = m_mouseCursor->update(state->unitManager()) || updated;
    }
```

In `Engine::start()`, find:
```cpp
    updated = m_mouseCursor->setPosition(mousePos) || updated;
```

Wrap it:
```cpp
    if (m_mouseCursor) {
        updated = m_mouseCursor->setPosition(mousePos) || updated;
    }
```

- [ ] **Step 3: Build and install**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

- [ ] **Step 4: Commit**

```bash
git add src/Engine.cpp
git commit -m "fix: hide mouse cursor on Android"
```

---

### Task 4: Fix long-press right-click

**Files:**
- Modify: `src/Engine.cpp`

Problem: User can't make the horse move via long press. The issue is that `handleTouchEvent` generates right-click at the touch START position, but the unit manager expects a right-click at the current position. Also, the 400ms threshold might be too short — user's finger moves slightly during the hold, triggering drag mode instead.

- [ ] **Step 1: Increase drag threshold and decrease long-press time**

In `Engine.h`, the `TouchState` struct:

Change:
```cpp
        static constexpr float DRAG_THRESHOLD = 15.f;
        static constexpr int64_t LONG_PRESS_MS = 400;
```
To:
```cpp
        static constexpr float DRAG_THRESHOLD = 25.f;  // 25px — more forgiving on high-DPI
        static constexpr int64_t LONG_PRESS_MS = 350;   // 350ms — faster right-click
```

- [ ] **Step 2: Use lastPos instead of startPos for right-click target**

In `handleTouchEvent`, in the `TouchEnded` case, change:

```cpp
            clickEvent.mouseButton.x = event.touch.x;
            clickEvent.mouseButton.y = event.touch.y;
```

This is already correct — it uses the finger-up position. But the issue is that the unit must first be **selected** (left click) before right-click does anything. So the flow is: tap unit (selects it) → long press ground (sends unit there). If this doesn't work, the problem is in unit selection (Task 2), not here.

- [ ] **Step 3: Generate mouseMove before right-click so UnitManager knows the target**

In `handleTouchEvent`, `TouchEnded` case, before generating the click events, add a mouseMove:

```cpp
        if (!m_touchState.dragging) {
            int64_t duration = currentTimeMs() - m_touchState.startTime;

            // Generate mouseMove first so UnitManager knows cursor position
            input::Event moveEvent;
            moveEvent.type = input::Event::MouseMoved;
            moveEvent.mouseMove.x = event.touch.x;
            moveEvent.mouseMove.y = event.touch.y;
            handleMouseMove(moveEvent, state);

            input::Event clickEvent;
            clickEvent.mouseButton.x = event.touch.x;
            clickEvent.mouseButton.y = event.touch.y;
```

- [ ] **Step 4: Build and install**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

- [ ] **Step 5: Commit**

```bash
git add src/Engine.h src/Engine.cpp
git commit -m "fix: long-press right-click — increased drag threshold, mouseMove before click"
```

---

### Task 5: Scale UI and game rendering for phone screen

**Files:**
- Modify: `src/Engine.cpp`

Problem from screenshot: the game renders at the original 1280x1024 resolution into a 2800x1272 screen. The UI overlay (designed for 1280x1024) is tiny in the corner. Need to use SDL's logical rendering to scale the game to fill the screen while maintaining aspect ratio.

- [ ] **Step 1: Set SDL render logical size**

In `Engine::setup()`, after the window and renderTarget are created (after the `#ifdef ANDROID` block around line 757), add:

```cpp
#ifdef ANDROID
    // Set logical render size — SDL will scale and letterbox to fill screen
    // Use 1280x720 (16:9) as base resolution for landscape phones
    SDL_RenderSetLogicalSize(
        static_cast<SdlRenderTarget*>(renderTarget_.get())->renderer(),
        1280, 720
    );
#endif
```

This makes SDL automatically scale all rendering from 1280x720 logical to the actual screen size. The game thinks it's rendering at 1280x720 but SDL stretches it.

Note: this requires `SdlRenderTarget::renderer()` to be accessible. Check that it exists — it's declared in SdlRenderTarget.h as `SDL_Renderer *renderer() const { return m_renderer; }`.

- [ ] **Step 2: Also set the render target size to match logical size**

After SDL_RenderSetLogicalSize, the renderTarget size should match:

```cpp
#ifdef ANDROID
    SDL_RenderSetLogicalSize(
        static_cast<SdlRenderTarget*>(renderTarget_.get())->renderer(),
        1280, 720
    );
    renderTarget_->setSize(Size(1280, 720));
#endif
```

And remove/skip the later `SDL_SetWindowSize` and `renderTarget_->setSize(uiSize)` call for Android since we already set it. The `#ifdef ANDROID` block around line 862 should set `uiSize = Size(1280, 720)` instead of querying window size:

Replace:
```cpp
#ifdef ANDROID
    // On Android, keep fullscreen size, don't resize to UI overlay
    {
        int sw, sh;
        SDL_GetWindowSize(m_sdlWindow->sdlWindow, &sw, &sh);
        uiSize = Size(sw, sh);
    }
```

With:
```cpp
#ifdef ANDROID
    // Use logical render size
    uiSize = Size(1280, 720);
```

- [ ] **Step 3: Build and install**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

- [ ] **Step 4: Commit**

```bash
git add src/Engine.cpp
git commit -m "feat: SDL logical render scaling — game renders at 1280x720 scaled to phone screen"
```

---

## Summary

| Task | Description | Complexity |
|------|-------------|------------|
| 1 | Pinch-to-zoom via SDL_MULTIGESTURE | Medium |
| 2 | Fix tap-to-select (dynamic Y threshold, disable rect-select on Android) | Small |
| 3 | Hide mouse cursor on Android | Small |
| 4 | Fix long-press right-click (threshold + mouseMove) | Small |
| 5 | Scale UI/game to phone screen via SDL logical rendering | Medium |

All 5 tasks modify only Engine.cpp/Engine.h + EventTypes.h + SdlRenderTarget.cpp. No new files.
