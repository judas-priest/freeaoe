# Plan: Edge Panning

**Goal:** In `Engine::updateCamera`, when the mouse is within 20 px of any screen edge, adjust the camera target. Skip on Android (already handled by touch drag). This replaces the current `handleMouseMove`-based delta approach with a direct query in `updateCamera` so that panning works even when the mouse is not moving.

**Status:** Not started

---

## Current behavior

`Engine::handleMouseMove` (around line 1054–1072 in `Engine.cpp`) already sets `m_cameraDeltaX` and `m_cameraDeltaY` when the mouse is near the edges. But these are only updated when a mouse-move event fires. If the user parks the mouse at the edge, no further events fire and panning stops.

`Engine::updateCamera` (line 1715) reads `m_cameraDeltaX/Y` and applies the movement. It is already guarded with `#ifdef ANDROID return false;`.

---

## Change: `Engine::updateCamera` in `Engine.cpp`

Replace the beginning of `updateCamera`:

**Current (lines 1715–1722):**
```cpp
bool Engine::updateCamera(const std::shared_ptr<GameState> &state)
{
#ifdef ANDROID
    return false; // Camera controlled by touch drag
#endif
    if (m_cameraDeltaX == 0 && m_cameraDeltaY == 0) {
        return false;
    }
```

**New:**
```cpp
bool Engine::updateCamera(const std::shared_ptr<GameState> &state)
{
#ifdef ANDROID
    return false; // Camera controlled by touch drag
#endif

    // Query current mouse position directly — works even when mouse is stationary
    {
        int mouseX = 0, mouseY = 0;
        SDL_GetMouseState(&mouseX, &mouseY);
        const Size windowSize = renderTarget_->getSize();
        constexpr int EDGE = 20; // pixels from edge to trigger panning

        if (mouseX < EDGE) {
            m_cameraDeltaX = -1;
        } else if (mouseX > windowSize.width - EDGE) {
            m_cameraDeltaX = 1;
        } else {
            m_cameraDeltaX = 0;
        }

        if (mouseY < EDGE) {
            m_cameraDeltaY = 1;
        } else if (mouseY > windowSize.height - EDGE) {
            m_cameraDeltaY = -1;
        } else {
            m_cameraDeltaY = 0;
        }
    }

    if (m_cameraDeltaX == 0 && m_cameraDeltaY == 0) {
        return false;
    }
```

The rest of `updateCamera` (applying the delta to the camera position) is unchanged.

---

## Remove redundant delta-setting from `handleMouseMove`

The delta was previously set in `handleMouseMove` (lines 1054–1072). Since `updateCamera` now queries mouse state directly, the `handleMouseMove` code is redundant. Remove those lines to avoid double-writes:

**Delete from `handleMouseMove`:**
```cpp
if (mousePos.x < MOUSE_MOVE_EDGE_SIZE) {
    m_cameraDeltaX = -1;
    handled = true;
} else if (mousePos.x > renderTarget_->getSize().width - MOUSE_MOVE_EDGE_SIZE) {
    m_cameraDeltaX = 1;
    handled = true;
} else {
    m_cameraDeltaX = 0;
}

if (mousePos.y < MOUSE_MOVE_EDGE_SIZE) {
    m_cameraDeltaY = 1;
    handled = true;
} else if (mousePos.y > renderTarget_->getSize().height - MOUSE_MOVE_EDGE_SIZE) {
    m_cameraDeltaY = -1;
    handled = true;
} else {
    m_cameraDeltaY = 0;
}
```

Also remove `MOUSE_MOVE_EDGE_SIZE` if it was only used there. Check with grep — if used elsewhere, keep the constant but rename it for clarity:

```cpp
constexpr int EDGE_PAN_SIZE = 20; // in Engine.h or Engine.cpp anonymous namespace
```

---

## Required includes in `Engine.cpp`

Add if not already present:

```cpp
#include <SDL2/SDL.h>
```

`SDL_GetMouseState` is declared in `<SDL2/SDL_mouse.h>` which is pulled in by `<SDL2/SDL.h>`.

---

## Why this is better

Previously the mouse had to move to trigger panning. Now panning is continuous as long as the mouse is parked within 20 px of the edge. This matches the behavior of real AoE2 and most RTSes.

The check runs once per game loop iteration (in `updateCamera`, which is called from `updateUi`). On a 60 fps loop that is 60 `SDL_GetMouseState` calls per second — extremely cheap.

---

## Android guard

The `#ifdef ANDROID return false;` at the top of `updateCamera` ensures that `SDL_GetMouseState` is never called on Android (where it may not be meaningful for touch input). Touch panning is handled separately by the touch state machine.

---

## Testing

1. Desktop build: move mouse to right edge of screen — camera should pan right continuously without needing to move the mouse.
2. Move mouse to top edge — camera should pan up.
3. Move mouse to center — panning stops.
4. Android build: no regression, touch drag still controls camera.
