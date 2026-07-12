# freeaoe Android v4 — Zoom, HUD, Touch, UI Fixes

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix zoom (render-to-texture approach), fix HUD click coordinates, add UI overlay scaling, fix minimap position, add Russian language.

**Architecture:** Two key changes from previous attempts:

1. **Touch input: RE-ENABLE SDL mouse emulation** (`SDL_HINT_TOUCH_MOUSE_EVENTS = "1"`, the default). This means every tap auto-generates mouse events → all HUD buttons (ActionPanel, IconButtons, Minimap, top bar menu) work WITHOUT any code changes. Handle FINGER events ONLY for camera drag and pinch zoom — ignore them for clicks (mouse emulation handles that). This is the approach recommended by ptilouk.net SDL Android porting guide.

2. **Zoom via render-to-texture:** Render game world to an off-screen texture at fixed size. Then `SDL_RenderCopy` with `srcrect` selecting a sub-region (zoom in = smaller src = magnified) to the full screen. HUD drawn directly to screen after, at 1:1. Touch coords for game area inverse-transformed from zoom.

UI overlay scaled to fit phone screen height. Minimap repositioned to bottom-right corner.

**Tech Stack:** C++20, SDL2

**Verified facts (from web search):**
- Render-to-texture zoom is the standard SDL2 approach (stackoverflow.com/questions/44079635 — "is this a sensible approach?" answer: yes, works fine)
- `SDL_CreateTexture` with `SDL_TEXTUREACCESS_TARGET` for off-screen render target (wiki.libsdl.org/SDL2/SDL_SetRenderTarget)
- `SDL_RenderSetLogicalSize` auto-transforms MOUSE coords but NOT FINGER coords (github libsdl-org issue #86)
- `SDL_RenderSetScale` transforms drawing coordinates but NOT texture sizes in some cases (reddit.com/r/sdl)
- AoE2 HD does NOT have UI scaling (slant.co review)

**Current state:**
- Screen: 2800×1272 physical, logical 1280×581
- UI overlay: designed for 1280×1024, height 209px (bottom bar only)
- `m_uiOverlayOffset`: positions the overlay image at screen bottom
- Minimap: hardcoded positions for height 1024/768/600, fallback to bottom-right
- ActionPanel: `rect()` computes position from `renderTarget->getSize().height`
- IconButtons (top bar): positioned from right edge at y=5
- Touch: FINGER events with coords converted to logical space

---

### Task 1: Zoom via off-screen render target

**Files:**
- Modify: `src/Engine.h`
- Modify: `src/Engine.cpp`

Remove `SDL_RenderSetScale` approach. Instead: create an off-screen texture the size of the logical screen. Render game world into it. Then `SDL_RenderCopy` it scaled + offset for zoom. Then render HUD directly to screen.

- [ ] **Step 1: Add off-screen texture member to Engine.h**

Add to Engine private members:

```cpp
#ifdef USE_SDL2
    SDL_Texture *m_gameTexture = nullptr; // off-screen render target for game world
#endif
```

- [ ] **Step 2: Create the texture in Engine::setup()**

After `renderTarget_->setSize(uiSize)` (line ~902), add:

```cpp
#ifdef USE_SDL2
    {
        auto *sdlRT = static_cast<SdlRenderTarget*>(renderTarget_.get());
        m_gameTexture = SDL_CreateTexture(sdlRT->renderer(),
            SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
            static_cast<int>(uiSize.width), static_cast<int>(uiSize.height));
    }
#endif
```

- [ ] **Step 3: Replace SDL_RenderSetScale with render-to-texture in main loop**

In Engine::start(), replace the current zoom blocks (lines ~298-330):

```cpp
        if (updated) {
#ifdef USE_SDL2
            auto *sdlRT = static_cast<SdlRenderTarget*>(renderTarget_.get());
            SDL_Renderer *ren = sdlRT->renderer();

            // Render game world to off-screen texture
            SDL_SetRenderTarget(ren, m_gameTexture);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);
#else
            renderWindow_->clear(sf::Color::Green);
#endif
            m_mapRenderer->display();
            drawEntities(state->map());
            state->draw();

            if (m_currentDialog) {
#ifdef USE_SDL2
                m_currentDialog->render(renderTarget_);
#else
                m_currentDialog->render(renderWindow_);
#endif
            }

            if (state->result != GameState::Result::Running) {
                renderTarget_->draw(m_resultOverlay);
            }

#ifdef USE_SDL2
            // Switch back to screen, draw game texture with zoom
            SDL_SetRenderTarget(ren, NULL);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderClear(ren);

            int texW = static_cast<int>(m_baseViewportSize.width);
            int texH = static_cast<int>(m_baseViewportSize.height);

            // Zoom via srcrect: smaller src = zoomed in (magnified)
            // At zoom=1: src = full texture. At zoom=2: src = center half.
            int srcW = static_cast<int>(texW / m_zoomLevel);
            int srcH = static_cast<int>(texH / m_zoomLevel);
            int srcX = (texW - srcW) / 2;
            int srcY = (texH - srcH) / 2;
            SDL_Rect src = {srcX, srcY, srcW, srcH};
            SDL_Rect dst = {0, 0, texW, texH}; // full screen
            SDL_RenderCopy(ren, m_gameTexture, &src, &dst);
#endif
            // HUD renders directly to screen at 1:1 — NOT affected by zoom
            drawUi();

            const int renderTime = Engine::currentTimeMs() - renderStart;
```

- [ ] **Step 4: Adjust touch coordinates for zoom in handleTouchEvent**

Game world is rendered zoomed but HUD is not. Touch on game area needs inverse zoom transform:

In `handleTouchEvent`, the `tx/ty` calculation:

```cpp
    // Touch coords are in logical screen space (1280×logicalH)
    // Game texture shows a sub-region when zoomed: src = center portion of texture
    // Screen pos → game texture pos: inverse of the srcrect mapping
    // screen_pos / texSize * srcSize + srcOffset
    float tx = static_cast<float>(event.touch.x);
    float ty = static_cast<float>(event.touch.y);
    float texW = m_baseViewportSize.width;
    float texH = m_baseViewportSize.height;
    float srcW = texW / m_zoomLevel;
    float srcH = texH / m_zoomLevel;
    float srcX = (texW - srcW) / 2.f;
    float srcY = (texH - srcH) / 2.f;
    float gameTx = srcX + tx / texW * srcW;
    float gameTy = srcY + ty / texH * srcH;
```

Use `gameTx/gameTy` for camera scroll and unit clicks. Use raw `tx/ty` only if checking against HUD elements.

- [ ] **Step 5: Cleanup — destroy texture in Engine destructor**

```cpp
Engine::~Engine()
{
#ifdef USE_SDL2
    if (m_gameTexture) {
        SDL_DestroyTexture(m_gameTexture);
        m_gameTexture = nullptr;
    }
#endif
}
```

- [ ] **Step 6: Build and install**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

- [ ] **Step 7: Commit**

```bash
git add src/Engine.h src/Engine.cpp
git commit -m "feat: zoom via render-to-texture — game zooms, HUD stays fixed"
```

---

### Task 2: Re-enable SDL mouse emulation — fix ALL touch input

**Files:**
- Modify: `src/Engine.cpp`

**KEY INSIGHT from search:** The best practice for SDL2 Android is to KEEP mouse emulation enabled (default). Every tap auto-generates `SDL_MOUSEBUTTONDOWN/UP` → all existing mouse handlers (ActionPanel, IconButtons, Minimap, menu, unit selection via `handleMousePress/handleMouseRelease`) work WITHOUT changes.

Use FINGER events ONLY for:
- Camera drag (single finger drag)
- Pinch zoom (two finger gesture via SDL_MULTIGESTURE)
- Double-tap detection for right-click

- [ ] **Step 1: Remove `SDL_HINT_TOUCH_MOUSE_EVENTS = "0"`**

In Engine::setup(), find and REMOVE:
```cpp
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
```

Default is "1" — touch generates mouse events. This is what we want.

- [ ] **Step 2: Simplify handleTouchEvent — ONLY camera drag and double-tap**

Touch handler should NOT do unit selection or HUD clicks — mouse emulation handles all of that. Touch handler does ONLY:
- Track drag state for camera scroll
- Detect double-tap for right-click (since mouse emulation only generates left clicks)

```cpp
bool Engine::handleTouchEvent(const input::Event &event, const std::shared_ptr<GameState> &state)
{
    float tx = static_cast<float>(event.touch.x);
    float ty = static_cast<float>(event.touch.y);

    switch (event.type) {
    case input::Event::TouchBegan:
        m_touchState.active = true;
        m_touchState.startPos = ScreenPos(tx, ty);
        m_touchState.lastPos = m_touchState.startPos;
        m_touchState.startTime = currentTimeMs();
        m_touchState.dragging = false;
        return false; // Let mouse emulation also handle this tap
    case input::Event::TouchMoved: {
        ScreenPos pos(tx, ty);
        if (!m_touchState.dragging) {
            if (m_touchState.startPos.distanceTo(pos) > TouchState::DRAG_THRESHOLD) {
                m_touchState.dragging = true;
            }
        }
        if (m_touchState.dragging) {
            ScreenPos delta = m_touchState.lastPos - pos;
            ScreenPos camScreen = renderTarget_->camera()->targetPosition().toScreen();
            camScreen.x += delta.x;
            camScreen.y -= delta.y;
            MapPos camMap = camScreen.toMap().clamped(state->map()->pixelSize());
            renderTarget_->camera()->setTargetPosition(camMap);
        }
        m_touchState.lastPos = pos;
        return false; // Let mouse emulation also fire (for edge-scroll disable see updateCamera)
    }
    case input::Event::TouchEnded: {
        // Double-tap detection for right-click
        if (!m_touchState.dragging) {
            ScreenPos pos(tx, ty);
            int64_t now = currentTimeMs();
            if (m_touchState.hasPendingTap
                && (now - m_touchState.pendingTapTime < TouchState::DOUBLE_TAP_MS)
                && m_touchState.pendingTapPos.distanceTo(pos) < TouchState::DOUBLE_TAP_DIST) {
                // Double tap = right click
                state->unitManager()->onRightClick(pos, renderTarget_->camera());
                m_touchState.hasPendingTap = false;
            } else {
                // Single tap — mouse emulation already handled left click
                // Just record for potential double-tap
                m_touchState.hasPendingTap = true;
                m_touchState.pendingTapTime = now;
                m_touchState.pendingTapPos = pos;
            }
        }
        m_touchState.active = false;
        m_touchState.dragging = false;
        return false; // Let mouse emulation handle the click
    }
    default:
        return false;
    }
}
```

Note: returns `false` everywhere — lets SDL mouse emulation events pass through to normal handlers.

- [ ] **Step 3: Remove ALL `#ifdef ANDROID` blocks from handleMouseMove, handleMouseRelease, handleEvent**

Mouse handlers should work identically on Android and desktop — because SDL mouse emulation generates the same events as a real mouse. Remove any Android-specific code from:
- `handleMouseMove` — remove edge-scroll disable (handled in updateCamera)
- `handleMouseRelease` — should be identical to desktop
- `handleEvent` — touch events go to handleTouchEvent, mouse events to mouse handlers, both fire

- [ ] **Step 4: Keep edge-scroll disable in updateCamera**

The `#ifdef ANDROID return false;` in `updateCamera()` stays — on touch devices edge-scroll makes no sense.

- [ ] **Step 5: Remove pending tap execution from main loop**

The "pending tap" logic in the main loop is no longer needed — mouse emulation handles taps immediately. Remove the `if (m_touchState.hasPendingTap)` block from the main loop.

- [ ] **Step 6: Build, install, commit**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
git add src/Engine.cpp && git commit -m "fix: re-enable SDL mouse emulation — all HUD/selection works, touch only for drag/zoom/doubletap"
```

---

### Task 3: Scale UI overlay for phone screen

**Files:**
- Modify: `src/Engine.cpp`

The UI overlay image is designed for 1280×1024 screens. On Android the logical screen is 1280×581. The overlay height is ~209px but it's positioned at `m_uiOverlayOffset` which assumes 1024 height. Need to scale the overlay to fit.

- [ ] **Step 1: Scale UI overlay image on Android**

In `drawUi()`, instead of drawing the overlay at 1:1:

```cpp
void Engine::drawUi()
{
    if (m_selecting) {
        renderTarget_->draw(m_selectionRect, Drawable::Transparent, Drawable::White);
    }

#ifdef ANDROID
    // Scale UI overlay to fit screen width, position at bottom
    if (m_uiOverlay && m_uiOverlay->isValid()) {
        float scaleX = renderTarget_->getSize().width / m_uiOverlay->size.width;
        float scaleY = scaleX; // maintain aspect ratio
        m_uiOverlay->scaleX = scaleX;
        m_uiOverlay->scaleY = scaleY;
        float overlayH = m_uiOverlay->size.height * scaleY;
        renderTarget_->draw(m_uiOverlay, ScreenPos(0, renderTarget_->getSize().height - overlayH));
    }
#else
    renderTarget_->draw(m_uiOverlay, ScreenPos(0, m_uiOverlayOffset));
#endif
```

- [ ] **Step 2: Build, install, commit**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
git add src/Engine.cpp && git commit -m "fix: scale UI overlay to fit phone screen"
```

---

### Task 4: Fix minimap position — bottom right corner

**Files:**
- Modify: `src/ui/Minimap.cpp`

Minimap has hardcoded positions for height 1024/768/600. Android screen height ~581 falls to the else branch: `ScreenRect(size.width - 400, size.height - 200, 400, 200)`. This is close but may overlap with other UI. Make it smaller.

- [ ] **Step 1: Add smaller minimap for short screens**

In `Minimap::updateRect()`, change the else branch:

```cpp
    } else {
        // Small screen (Android) — smaller minimap in bottom right
        int mmW = std::min(250, static_cast<int>(size.width * 0.2f));
        int mmH = mmW / 2;
        m_rect = ScreenRect(size.width - mmW - 5, size.height - mmH - 5, mmW, mmH);
    }
```

- [ ] **Step 2: Build, install, commit**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
git add src/ui/Minimap.cpp && git commit -m "fix: smaller minimap in bottom-right for phone screen"
```

---

### Task 5: Russian language

**Files:**
- Modify: `android/app/src/main/java/org/freeaoe/FreeAoEActivity.java`

Already added `--language=ru` in getArguments. Verify it's there and working. HD Edition stores Russian strings in `resources/ru/strings/key-value-strings-utf8.txt`.

- [ ] **Step 1: Verify getArguments has --language=ru**

File should contain:
```java
return new String[]{"--game-path=" + dataDir.getAbsolutePath(), "--single-player", "--language=ru"};
```

If not, add it.

- [ ] **Step 2: Verify Russian language files exist on device**

```bash
unset LD_PRELOAD
ADB=/home/dima/.local/opt/android-sdk/platform-tools/adb
$ADB shell ls /storage/emulated/0/Android/data/org.freeaoe/files/aoe2data/resources/ru/ 2>/dev/null | head -5
```

If missing, the data push in an earlier step didn't include `ru` directory. Re-push if needed.

- [ ] **Step 3: Commit if changes needed**

```bash
git add android/app/src/main/java/org/freeaoe/FreeAoEActivity.java
git commit -m "fix: ensure Russian language flag is set"
```

---

### Task 6: Verify scroll direction

**Files:**
- Modify: `src/Engine.cpp` (if needed)

Best practice from search: "finger drag = pan map" like dragging paper. Finger RIGHT → map goes RIGHT (camera moves LEFT). In isometric screen coords, Y is inverted relative to map coords.

In Task 2 handleTouchEvent, the scroll code is:
```cpp
ScreenPos delta = m_touchState.lastPos - pos;
camScreen.x += delta.x;
camScreen.y -= delta.y;
```

This means:
- Finger RIGHT → `delta.x < 0` → `camScreen.x decreases` → camera LEFT → map appears to move RIGHT ✓
- Finger DOWN → `delta.y < 0` → `camScreen.y -= negative → increases` → camera DOWN in screen → map UP

In isometric projection `toScreen()` returns `y = z + (mapY - mapX)/2`. Camera screen Y increase = map moves south. So finger DOWN → see more south. This is correct "drag paper" behavior.

**No changes needed if this direction feels natural. Test on device.**

- [ ] **Step 1: Test scrolling — if wrong, swap to `camScreen.y += delta.y`**
- [ ] **Step 2: Commit if changed**

---

## Summary

| Task | What | Key technique |
|------|------|---------------|
| 1 | Zoom via render-to-texture | `SDL_SetRenderTarget` + `SDL_RenderCopy` with scaled dstrect |
| 2 | HUD buttons respond to touch | Synthesize mouse events, try HUD first then game |
| 3 | UI overlay scales to screen | Scale overlay image, position at bottom |
| 4 | Minimap in bottom-right | Proportional size for small screens |
| 5 | Russian language | `--language=ru` arg |
| 6 | Scroll direction | Verify/fix Y axis |
