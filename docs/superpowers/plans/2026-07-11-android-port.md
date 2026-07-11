# freeaoe Android Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port freeaoe (open-source Age of Empires 2 engine) from SFML to SDL2 so it compiles and runs on Android via NDK.

**Architecture:** The codebase already has a render abstraction layer (`IRenderTarget` / `Drawable::*`). The plan: (1) purge `sf::` types from the abstraction layer and shared headers, replacing with our own types, (2) write `SdlRenderTarget` implementing `IRenderTarget`, (3) replace `sf::Event` with SDL events in Engine, (4) add Android entry point via SDL2's built-in `SDLActivity`. Sound already uses cross-platform miniaudio — no changes needed.

**Tech Stack:** C++20, SDL2 (SDL2_ttf for fonts), Android NDK, CMake, genieutils, miniaudio

**Target Device:** OnePlus Ace 5 Ultra (Dimensity 9400, 16GB RAM, Android 16)

**Environment:**
- Android SDK: `/home/dima/android-sdk` (platform android-36)
- NDK: `/home/dima/.local/opt/android-sdk/ndk/28.2.13676358`
- `unset LD_PRELOAD` before any `adb` command
- AoE2 data files: push to `/sdcard/Download/aoe2data/` or `getExternalFilesDir()`

**Key constraints:**
- SDL2 sources bundled as submodule (not system package) for Android cross-compile
- SDL2 provides `SDLActivity.java` — use it as base class
- `libc++_shared.so` must be bundled in APK
- Minimal APK, no Compose

---

## File Map

| Action | File | Responsibility |
|--------|------|----------------|
| Modify | `src/core/Types.h` | Remove all `sf::` vector/rect types, make self-contained |
| Modify | `src/render/IRenderTarget.h` | Remove all `sf::` from API, pure Drawable types only |
| Modify | `src/render/IRenderTarget.cpp` | Update to match new API |
| Create | `src/render/SdlRenderTarget.h` | SDL2 implementation of IRenderTarget |
| Create | `src/render/SdlRenderTarget.cpp` | SDL2 implementation of IRenderTarget |
| Modify | `src/render/SfmlRenderTarget.h` | Update to match purged IRenderTarget API |
| Modify | `src/render/SfmlRenderTarget.cpp` | Update to match purged IRenderTarget API |
| Modify | `src/resource/Resource.h` | Remove `sf::Image` return, use `Drawable::Image::Ptr` |
| Modify | `src/resource/Resource.cpp` | Implement with Drawable types |
| Modify | `src/resource/Sprite.h` | Replace `sf::Texture` with `Drawable::Image::Ptr` |
| Modify | `src/resource/Sprite.cpp` | Update texture cache to use Drawable types |
| Create | `src/render/EventTypes.h` | Platform-agnostic event types (replace sf::Event) |
| Modify | `src/Engine.h` | Use agnostic event/window types |
| Modify | `src/Engine.cpp` | SDL2 event loop, window creation |
| Modify | `src/ui/UiScreen.h` | Remove sf::RenderWindow, sf::Color, sf::Texture |
| Modify | `src/ui/UiScreen.cpp` | Use Drawable/SDL window |
| Modify | `src/ui/HomeScreen.h` | Remove sf::Texture, sf::Text |
| Modify | `src/ui/HomeScreen.cpp` | Use Drawable types |
| Modify | `src/ui/Dialog.h` | Remove sf::Texture, sf::Event |
| Modify | `src/ui/Dialog.cpp` | Use agnostic types |
| Modify | `src/ui/FileDialog.h` | Remove sf:: types |
| Modify | `src/ui/FileDialog.cpp` | Use agnostic types |
| Modify | `src/ui/HistoryScreen.h` | Remove sf:: types |
| Modify | `src/ui/HistoryScreen.cpp` | Use agnostic types |
| Modify | `src/ui/*.h/cpp` (remaining) | Replace sf::Event with EventTypes |
| Modify | `src/editor/Editor.h` | Remove sf:: types |
| Modify | `src/editor/Editor.cpp` | Use agnostic types |
| Modify | `src/main.cpp` | Conditional SDL_main for Android |
| Modify | `CMakeLists.txt` | Add SDL2 backend option, Android toolchain |
| Create | `android/` | Android project shell (SDLActivity, manifest, gradle) |

---

### Task 1: Purge sf:: from Types.h

**Files:**
- Modify: `src/core/Types.h`

This is the foundation — almost every file includes Types.h. Currently it pulls in `SFML/System/Vector2.hpp` and `SFML/Graphics/Rect.hpp` and uses `sf::Vector2f`, `sf::Vector2i`, `sf::Vector2u`, `sf::FloatRect`, `sf::Uint8`, `sf::Uint32`, `sf::Int32`. All of these have trivial replacements since the codebase already defines `Size`, `ScreenPos`, `MapPos`, `ScreenRect`, `MapRect`.

- [ ] **Step 1: Remove SFML includes and sf:: typedefs**

Replace the SFML-dependent section at the top of `Types.h`:

```cpp
// REMOVE these lines:
// #include <SFML/System/Vector2.hpp>
// #include <SFML/Graphics/Rect.hpp>
// #ifndef SFML_CONFIG_HPP ... #endif
// using sf::Uint32; using sf::Uint8; using sf::Int32; using sf::Vector2u;

// REPLACE with:
#include <cstdint>

using Uint8 = uint8_t;
using Uint32 = uint32_t;
using Int32 = int32_t;
```

- [ ] **Step 2: Remove sf::Vector2f/Vector2u/Vector2i conversions from Size, ScreenPos, ScreenRect**

In `struct Size`, remove:
- Constructor `Size(const sf::Vector2f &sfVector)`
- Constructor `Size(const sf::Vector2u &sfVector)`
- `operator sf::Vector2f()`
- `operator sf::Vector2u()`
- `operator sf::FloatRect()`

In `struct ScreenPos`, remove:
- Constructor `ScreenPos(const sf::Vector2i &v)`
- `operator sf::Vector2f()`
- `operator==(const sf::Vector2f &)` and `operator!=(const sf::Vector2f &)`

In `struct ScreenRect`, remove:
- Constructor `ScreenRect(const sf::FloatRect &r)`

- [ ] **Step 3: Verify it compiles (SFML backend only, expect many errors in other files)**

Run: `cd /home/dima/Projects/freeaoe && mkdir -p build && cd build && cmake .. 2>&1 | head -20`

This WILL fail in downstream files — that's expected. The point is Types.h itself is clean.

- [ ] **Step 4: Commit**

```bash
git add src/core/Types.h
git commit -m "refactor: purge sf:: types from Types.h, use stdint"
```

---

### Task 2: Purge sf:: from IRenderTarget API

**Files:**
- Modify: `src/render/IRenderTarget.h`
- Modify: `src/render/IRenderTarget.cpp`

The author left a TODO: "Remove sf:: from api". We do it now. The IRenderTarget interface currently has draw methods accepting `sf::Image`, `sf::Texture`, `sf::Sprite`, `sf::Drawable`, `sf::BlendMode`, `sf::Color`. We replace them all with `Drawable::` equivalents.

- [ ] **Step 1: Add BlendMode to Drawable namespace in IRenderTarget.h**

Add to the `Drawable` namespace (before `struct Shape`):

```cpp
enum class BlendMode {
    Alpha,
    Add,
    Multiply,
    None
};
```

- [ ] **Step 2: Remove all sf:: forward declarations and sf:: draw methods**

Remove the `namespace sf { ... }` forward declarations at the top.

Remove these virtual methods from `IRenderTarget`:
```cpp
// REMOVE:
virtual void draw(const sf::Image &image, ScreenPos pos) = 0;
virtual void draw(const sf::Texture &texture, ScreenPos pos) = 0;
virtual void draw(const sf::Drawable &shape) = 0;
virtual void draw(const sf::Sprite &sprite) = 0;
virtual void draw(const sf::Sprite &sprite, const sf::BlendMode &blendMode) = 0;
virtual void draw(const std::shared_ptr<IRenderTarget> &renderTarget, const sf::BlendMode &blendMode) = 0;
```

Replace with:
```cpp
virtual void draw(const std::shared_ptr<IRenderTarget> &renderTarget, const Drawable::BlendMode blendMode) = 0;
```

The remaining pure-Drawable draw methods stay as-is.

- [ ] **Step 3: Commit**

```bash
git add src/render/IRenderTarget.h src/render/IRenderTarget.cpp
git commit -m "refactor: purge sf:: from IRenderTarget API"
```

---

### Task 3: Add platform-agnostic event types

**Files:**
- Create: `src/render/EventTypes.h`

Replace `sf::Event`, `sf::Keyboard`, `sf::Mouse` usage throughout the codebase with our own event types. This is a thin layer — just the events that freeaoe actually uses.

- [ ] **Step 1: Create EventTypes.h**

```cpp
#pragma once
#include <cstdint>

namespace input {

enum class MouseButton {
    Left,
    Right,
    Middle
};

enum class Key {
    Unknown,
    Left, Right, Up, Down,
    Space, Return, Escape,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    Delete, BackSpace, Tab,
    LShift, RShift, LControl, RControl, LAlt, RAlt
};

struct Event {
    enum Type {
        Closed,
        KeyPressed,
        KeyReleased,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseWheelScrolled,
        TextEntered,
        TouchBegan,
        TouchMoved,
        TouchEnded,
    } type;

    struct KeyEvent {
        Key code;
        bool shift = false;
        bool control = false;
        bool alt = false;
    };

    struct MouseButtonEvent {
        MouseButton button;
        int x = 0;
        int y = 0;
    };

    struct MouseMoveEvent {
        int x = 0;
        int y = 0;
    };

    struct MouseWheelEvent {
        float delta = 0;
        int x = 0;
        int y = 0;
    };

    struct TextEvent {
        uint32_t unicode = 0;
    };

    struct TouchEvent {
        int finger = 0;
        int x = 0;
        int y = 0;
    };

    KeyEvent key;
    MouseButtonEvent mouseButton;
    MouseMoveEvent mouseMove;
    MouseWheelEvent mouseWheel;
    TextEvent text;
    TouchEvent touch;
};

} // namespace input
```

- [ ] **Step 2: Commit**

```bash
git add src/render/EventTypes.h
git commit -m "feat: add platform-agnostic input event types"
```

---

### Task 4: Update SfmlRenderTarget to match purged IRenderTarget

**Files:**
- Modify: `src/render/SfmlRenderTarget.h`
- Modify: `src/render/SfmlRenderTarget.cpp`

Remove the sf::-param draw methods from the class declaration that were removed from IRenderTarget. Keep the internal SFML usage — SfmlRenderTarget is the SFML backend and should use SFML internally. But its public interface must match the purged IRenderTarget.

- [ ] **Step 1: Update SfmlRenderTarget.h**

Remove override declarations for:
```cpp
void draw(const sf::Image &image, ScreenPos pos) override;
void draw(const sf::Texture &texture, ScreenPos pos) override;
void draw(const sf::Drawable &shape) override;
void draw(const sf::Sprite &sprite) override;
void draw(const sf::Sprite &sprite, const sf::BlendMode &blendMode) override;
void draw(const std::shared_ptr<IRenderTarget> &renderTarget, const sf::BlendMode &blendMode) override;
```

Change the blendMode draw to:
```cpp
void draw(const std::shared_ptr<IRenderTarget> &renderTarget, const Drawable::BlendMode blendMode) override;
```

Move the removed sf:: draw methods to private non-virtual helpers (they're still used internally by the SFML backend):
```cpp
private:
    void drawSfSprite(const sf::Sprite &sprite);
    void drawSfSprite(const sf::Sprite &sprite, const sf::BlendMode &blendMode);
    void drawSfTexture(const sf::Texture &texture, ScreenPos pos);
```

- [ ] **Step 2: Update SfmlRenderTarget.cpp**

Rename the removed public methods to the private helper names. Update `draw(shared_ptr<IRenderTarget>, Drawable::BlendMode)` to convert `Drawable::BlendMode` to `sf::BlendMode` internally:

```cpp
static sf::BlendMode toSfBlendMode(Drawable::BlendMode mode) {
    switch (mode) {
    case Drawable::BlendMode::Add: return sf::BlendAdd;
    case Drawable::BlendMode::Multiply: return sf::BlendMultiply;
    case Drawable::BlendMode::None: return sf::BlendNone;
    case Drawable::BlendMode::Alpha:
    default: return sf::BlendAlpha;
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add src/render/SfmlRenderTarget.h src/render/SfmlRenderTarget.cpp
git commit -m "refactor: update SfmlRenderTarget to match purged IRenderTarget API"
```

---

### Task 5: Migrate Resource and Sprite from sf::Image/sf::Texture to Drawable types

**Files:**
- Modify: `src/resource/Resource.h`
- Modify: `src/resource/Resource.cpp`
- Modify: `src/resource/Sprite.h`
- Modify: `src/resource/Sprite.cpp`
- Modify: `src/resource/TerrainSprite.h`
- Modify: `src/resource/TerrainSprite.cpp`

`Resource::convertFrameToImage` returns `sf::Image` — change to return raw pixel data or `Drawable::Image::Ptr`.
`Sprite` caches `sf::Texture` in `m_cache` and has a `texture()` method returning `const sf::Texture&` — change to `Drawable::Image::Ptr`.

- [ ] **Step 1: Change Resource::convertFrameToImage**

In `Resource.h`, change:
```cpp
// Was: static sf::Image convertFrameToImage(...)
// Now:
struct RawImage {
    std::vector<uint8_t> pixels; // RGBA
    int width = 0;
    int height = 0;
};
static RawImage convertFrameToImage(const genie::SlpFramePtr &frame);
static RawImage convertFrameToImage(const genie::SlpFramePtr &frame, const genie::PalFile &palette, const int playerColor = -1);
```

Remove `#include <SFML/Graphics/Image.hpp>`.

- [ ] **Step 2: Update Resource.cpp**

The existing implementation creates an `sf::Image`, sets pixels, returns it. Change to fill `RawImage.pixels` (RGBA byte array) with the same pixel logic. The pixel conversion code from genieutils SlpFrame is the same — just write into a `std::vector<uint8_t>` instead of `sf::Image`.

- [ ] **Step 3: Change Sprite to use Drawable::Image::Ptr**

In `Sprite.h`:
```cpp
// Remove: static const sf::Texture nullImage;
// Remove: #include <SFML/Graphics/Texture.hpp>
// Change: std::unordered_map<SpriteState, sf::Texture> m_cache;
//     To: std::unordered_map<SpriteState, Drawable::Image::Ptr> m_cache;
// Change: const sf::Texture &texture(...);
//     To: Drawable::Image::Ptr texture(IRenderTarget &rt, ...);
// Change: static sf::Image slpFrameToImage(...);
//     To: static Resource::RawImage slpFrameToImage(...);
```

- [ ] **Step 4: Update Sprite.cpp**

Update `texture()` to use `IRenderTarget::createImage()` to create `Drawable::Image::Ptr` from raw pixels, and cache that. Update `slpFrameToImage` to return `Resource::RawImage`.

- [ ] **Step 5: Update all callers of Sprite::texture() and Resource::convertFrameToImage()**

These are in:
- `src/render/GraphicRender.cpp` — uses `sprite.texture()` to get sf::Texture, creates sf::Sprite, draws. Change to use `Drawable::Image::Ptr` and `renderTarget->draw(image, pos)`.
- `src/render/UnitsRenderer.cpp` — same pattern.
- `src/ui/HomeScreen.cpp` — uses `Resource::convertFrameToImage` for button textures.
- `src/ui/UiScreen.cpp` — uses it for backgrounds.
- `src/ui/Dialog.cpp` — menu background.
- `src/mechanics/Farm.cpp` — farm terrain overlay.

Each caller changes from `sf::Sprite + sf::Texture` pattern to `Drawable::Image::Ptr + renderTarget->draw(image, pos)`.

- [ ] **Step 6: Compile test**

Run: `cd build && cmake .. && make -j$(nproc) 2>&1 | tail -30`

- [ ] **Step 7: Commit**

```bash
git add src/resource/Resource.h src/resource/Resource.cpp src/resource/Sprite.h src/resource/Sprite.cpp src/resource/TerrainSprite.h src/resource/TerrainSprite.cpp src/render/GraphicRender.cpp src/render/UnitsRenderer.cpp src/ui/HomeScreen.h src/ui/HomeScreen.cpp src/ui/UiScreen.h src/ui/UiScreen.cpp src/ui/Dialog.h src/ui/Dialog.cpp src/mechanics/Farm.cpp
git commit -m "refactor: migrate Resource/Sprite from sf::Image/Texture to Drawable types"
```

---

### Task 6: Migrate UI layer from sf::Event to input::Event

**Files:**
- Modify: `src/Engine.h`
- Modify: `src/Engine.cpp`
- Modify: `src/ui/UiScreen.h`
- Modify: `src/ui/UiScreen.cpp`
- Modify: `src/ui/HomeScreen.h`
- Modify: `src/ui/HomeScreen.cpp`
- Modify: `src/ui/Dialog.h`
- Modify: `src/ui/Dialog.cpp`
- Modify: `src/ui/FileDialog.h`
- Modify: `src/ui/FileDialog.cpp`
- Modify: `src/ui/HistoryScreen.h`
- Modify: `src/ui/HistoryScreen.cpp`
- Modify: `src/ui/ActionPanel.h`
- Modify: `src/ui/ActionPanel.cpp`
- Modify: `src/ui/Minimap.h`
- Modify: `src/ui/Minimap.cpp`
- Modify: `src/ui/UnitInfoPanel.h`
- Modify: `src/ui/UnitInfoPanel.cpp`
- Modify: `src/editor/Editor.h`
- Modify: `src/editor/Editor.cpp`

All `sf::Event` parameters become `input::Event`. All `sf::Keyboard::Key` become `input::Key`. All `sf::Mouse::Button` become `input::MouseButton`.

- [ ] **Step 1: Replace sf::Event in Engine.h**

```cpp
// Remove: #include <SFML/Window/Event.hpp> etc.
// Add: #include "render/EventTypes.h"

// Change all method signatures:
// bool handleEvent(const sf::Event &event, ...) → bool handleEvent(const input::Event &event, ...)
// bool handleKeyEvent(const sf::Event &event, ...) → bool handleKeyEvent(const input::Event &event, ...)
// etc.

// Remove: std::shared_ptr<sf::RenderWindow> renderWindow_;
// Add: std::unique_ptr<Drawable::Window> m_window;
```

- [ ] **Step 2: Replace sf::Event in Engine.cpp**

The main loop currently uses `sf::RenderWindow::pollEvent(sf::Event)`. This will be replaced with SDL in Task 8. For now, keep the SFML backend working by adding an `sfEventToInput()` conversion function:

```cpp
static input::Event sfEventToInput(const sf::Event &sfEvent) {
    input::Event event;
    switch (sfEvent.type) {
    case sf::Event::Closed:
        event.type = input::Event::Closed; break;
    case sf::Event::KeyPressed:
        event.type = input::Event::KeyPressed;
        event.key.code = sfKeyToInput(sfEvent.key.code);
        event.key.shift = sfEvent.key.shift;
        event.key.control = sfEvent.key.control;
        event.key.alt = sfEvent.key.alt;
        break;
    case sf::Event::MouseButtonPressed:
        event.type = input::Event::MouseButtonPressed;
        event.mouseButton.x = sfEvent.mouseButton.x;
        event.mouseButton.y = sfEvent.mouseButton.y;
        event.mouseButton.button = (sfEvent.mouseButton.button == sf::Mouse::Right)
            ? input::MouseButton::Right : input::MouseButton::Left;
        break;
    case sf::Event::MouseButtonReleased:
        event.type = input::Event::MouseButtonReleased;
        event.mouseButton.x = sfEvent.mouseButton.x;
        event.mouseButton.y = sfEvent.mouseButton.y;
        event.mouseButton.button = (sfEvent.mouseButton.button == sf::Mouse::Right)
            ? input::MouseButton::Right : input::MouseButton::Left;
        break;
    case sf::Event::MouseMoved:
        event.type = input::Event::MouseMoved;
        event.mouseMove.x = sfEvent.mouseMove.x;
        event.mouseMove.y = sfEvent.mouseMove.y;
        break;
    default:
        event.type = input::Event::Closed; // ignored
        break;
    }
    return event;
}
```

Update the event loop to convert then dispatch.

- [ ] **Step 3: Replace sf::Event in all UI files**

Mechanical replacement in each file:
- `#include <SFML/Window/Event.hpp>` → `#include "render/EventTypes.h"`
- `const sf::Event &event` → `const input::Event &event`
- `sf::Event::MouseButtonPressed` → `input::Event::MouseButtonPressed`
- `sf::Mouse::Button::Left` → `input::MouseButton::Left`
- `sf::Mouse::Button::Right` → `input::MouseButton::Right`
- `event.mouseButton.button == sf::Mouse::Left` → `event.mouseButton.button == input::MouseButton::Left`
- `sf::Keyboard::*` → `input::Key::*`
- `event.key.code` stays the same (same field name)

- [ ] **Step 4: Replace sf::RenderWindow in UiScreen**

`UiScreen` holds `std::shared_ptr<sf::RenderWindow> m_renderWindow` and uses it to create a render target and run event loops. Change to `Drawable::Window*` or keep the shared_ptr to window. The `UiScreen::run()` method has its own event loop — convert it too.

Replace `sf::Color` members in UiScreen.h with `Drawable::Color`:
```cpp
// sf::Color m_textFillColor; → Drawable::Color m_textFillColor;
// etc for all sf::Color members
```

Replace `sf::Texture m_background` with `Drawable::Image::Ptr m_background`.

- [ ] **Step 5: Replace sf::Texture/sf::Text in HomeScreen.h**

In the `Button` struct:
```cpp
// sf::Texture texture; → Drawable::Image::Ptr texture;
// sf::Texture hoverTexture; → Drawable::Image::Ptr hoverTexture;
// sf::Texture selectedTexture; → Drawable::Image::Ptr selectedTexture;
// sf::Text text; → Drawable::Text::Ptr text;
// sf::Text m_description; → Drawable::Text::Ptr m_description;
```

- [ ] **Step 6: Update Dialog.h**

```cpp
// sf::Texture background; → Drawable::Image::Ptr background;
// void render(std::shared_ptr<sf::RenderWindow> &) → void render(IRenderTarget *rt)
// Choice handleEvent(const sf::Event &) → Choice handleEvent(const input::Event &)
```

- [ ] **Step 7: Compile test and fix remaining sf:: references**

Run: `cd build && make -j$(nproc) 2>&1 | grep "sf::" | head -30`

Fix any remaining references. Common patterns:
- `sf::sleep(sf::milliseconds(N))` → platform sleep (SDL_Delay or std::this_thread::sleep_for)
- `sf::Clock` → `std::chrono::steady_clock`

- [ ] **Step 8: Commit**

```bash
git add src/render/EventTypes.h src/Engine.h src/Engine.cpp src/ui/*.h src/ui/*.cpp src/editor/*.h src/editor/*.cpp
git commit -m "refactor: migrate all UI from sf::Event to input::Event, purge sf::Texture/Text from UI"
```

---

### Task 7: Replace sf::Clock with std::chrono

**Files:**
- Modify: `src/Engine.h`
- Modify: `src/Engine.cpp`
- Modify: `src/actions/ActionMove.cpp`
- Modify: `src/server/GameServer.cpp`
- Modify: `src/server/GameServer.h`
- Modify: `src/mechanics/IState.h`
- Modify: `src/mechanics/GameState.cpp`

`Engine::GameClock` is a `static const sf::Clock` used everywhere via `GameClock.getElapsedTime().asMilliseconds()`.

- [ ] **Step 1: Replace with std::chrono**

In `Engine.h`:
```cpp
// Remove: static const sf::Clock GameClock;
// Add:
#include <chrono>
static inline int64_t currentTimeMs() {
    static const auto start = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();
}
```

- [ ] **Step 2: Replace all GameClock.getElapsedTime().asMilliseconds() calls**

Search and replace across the codebase:
```
GameClock.getElapsedTime().asMilliseconds()  →  Engine::currentTimeMs()
```

Also replace `sf::sleep(sf::milliseconds(N))` with:
```cpp
#include <thread>
std::this_thread::sleep_for(std::chrono::milliseconds(N));
```

- [ ] **Step 3: Compile test**

Run: `cd build && make -j$(nproc) 2>&1 | tail -20`

- [ ] **Step 4: Commit**

```bash
git add src/Engine.h src/Engine.cpp src/actions/ActionMove.cpp src/server/GameServer.cpp src/server/GameServer.h src/mechanics/IState.h src/mechanics/GameState.cpp
git commit -m "refactor: replace sf::Clock with std::chrono"
```

---

### Task 8: Write SdlRenderTarget backend

**Files:**
- Create: `src/render/SdlRenderTarget.h`
- Create: `src/render/SdlRenderTarget.cpp`

Implement `IRenderTarget` using SDL2 + SDL2_ttf.

- [ ] **Step 1: Create SdlRenderTarget.h**

```cpp
#pragma once
#include "IRenderTarget.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <string>

struct SdlImage : public Drawable::Image {
    SDL_Texture *sdlTexture = nullptr;
    SDL_Renderer *renderer = nullptr; // non-owning, for re-creation

    bool isValid() const override { return sdlTexture != nullptr; }
    ~SdlImage() override;
};

struct SdlText : public Drawable::Text {
    TTF_Font *font = nullptr; // non-owning
    SDL_Texture *cachedTexture = nullptr;
    SDL_Renderer *renderer = nullptr;
    std::string lastString;
    float lastPointSize = 0;
    int cachedWidth = 0, cachedHeight = 0;

    float lineSpacing() override;
    Size size() override;
    ~SdlText() override;
};

class SdlRenderTarget : public IRenderTarget {
public:
    static TTF_Font *plainFont();
    static TTF_Font *uiFont();
    static TTF_Font *stylishFont();

    // For window-backed targets
    SdlRenderTarget(SDL_Renderer *renderer, int w, int h);
    // For texture-backed targets (off-screen)
    SdlRenderTarget(SDL_Renderer *renderer, const Size &size);
    ~SdlRenderTarget() override;

    Size getSize() const override;
    void setSize(const Size size) const override;

    void draw(const ScreenRect &rect, const Drawable::Color &fillColor,
              const Drawable::Color &outlineColor = Drawable::Transparent,
              const float outlineSize = 1.) override;
    void draw(const Drawable::Rect &rect) override;
    void draw(const Drawable::Circle &circle) override;
    void draw(const Drawable::Image::Ptr &image, const ScreenPos &position) override;
    void draw(const std::shared_ptr<IRenderTarget> &renderTarget,
              const ScreenPos &pos = ScreenPos(0, 0)) override;
    void draw(const std::shared_ptr<IRenderTarget> &renderTarget,
              const Drawable::BlendMode blendMode) override;
    void draw(const Drawable::Text::Ptr &text) override;

    void display() override;
    void clear(const Drawable::Color &color = Drawable::Color(0, 0, 0, 255)) override;

    Drawable::Image::Ptr createImage(const Size &size, const uint8_t *pixels) const override;
    Drawable::Image::Ptr loadImage(const uint8_t *data, const size_t dataSize) const override;
    std::shared_ptr<IRenderTarget> createTextureTarget(const Size &size) const override;
    Drawable::Text::Ptr createText(const Drawable::Text::Style style = Drawable::Text::Plain) const override;

    SDL_Renderer *renderer() const { return m_renderer; }
    SDL_Texture *targetTexture() const { return m_targetTexture; }

private:
    SDL_Renderer *m_renderer; // non-owning
    SDL_Texture *m_targetTexture = nullptr; // owned, for off-screen targets
    mutable int m_width, m_height;
};

struct SdlWindow : public Drawable::Window {
    SdlWindow(const Size size, const std::string &title);
    ~SdlWindow() override;

    SDL_Window *sdlWindow = nullptr;
    SDL_Renderer *sdlRenderer = nullptr;

    void display() override;
    bool isOpen() const override;
    void close() override;

    bool pollEvent(input::Event &event);

private:
    bool m_open = true;
};
```

- [ ] **Step 2: Implement SdlRenderTarget.cpp**

Key implementation points:
- `createImage`: create `SDL_Texture` from RGBA pixels via `SDL_CreateTexture` + `SDL_UpdateTexture`
- `loadImage`: use `SDL_RWFromConstMem` + `IMG_LoadTexture_RW` (or manual BMP/PNG decode)
- `draw(Image)`: `SDL_RenderCopy` with position
- `draw(Rect)`: `SDL_RenderDrawRect` / `SDL_RenderFillRect`
- `draw(Circle)`: midpoint circle algorithm (SDL has no built-in circle)
- `draw(Text)`: `TTF_RenderUTF8_Blended` → `SDL_CreateTextureFromSurface` → `SDL_RenderCopy`
- `clear`: `SDL_SetRenderDrawColor` + `SDL_RenderClear`
- `display`: `SDL_RenderPresent` (only for window targets)
- Fonts loaded from embedded resources (same .h data files) via `SDL_RWFromConstMem` + `TTF_OpenFontRW`
- Off-screen targets use `SDL_CreateTexture` with `SDL_TEXTUREACCESS_TARGET` and `SDL_SetRenderTarget`

For `SdlWindow::pollEvent`: convert `SDL_Event` to `input::Event`:
```cpp
bool SdlWindow::pollEvent(input::Event &event) {
    SDL_Event sdlEvent;
    while (SDL_PollEvent(&sdlEvent)) {
        switch (sdlEvent.type) {
        case SDL_QUIT:
            event.type = input::Event::Closed;
            return true;
        case SDL_KEYDOWN:
            event.type = input::Event::KeyPressed;
            event.key.code = sdlKeyToInput(sdlEvent.key.keysym.sym);
            event.key.shift = sdlEvent.key.keysym.mod & KMOD_SHIFT;
            event.key.control = sdlEvent.key.keysym.mod & KMOD_CTRL;
            event.key.alt = sdlEvent.key.keysym.mod & KMOD_ALT;
            return true;
        case SDL_MOUSEBUTTONDOWN:
            event.type = input::Event::MouseButtonPressed;
            event.mouseButton.x = sdlEvent.button.x;
            event.mouseButton.y = sdlEvent.button.y;
            event.mouseButton.button = (sdlEvent.button.button == SDL_BUTTON_RIGHT)
                ? input::MouseButton::Right : input::MouseButton::Left;
            return true;
        case SDL_MOUSEBUTTONUP:
            event.type = input::Event::MouseButtonReleased;
            event.mouseButton.x = sdlEvent.button.x;
            event.mouseButton.y = sdlEvent.button.y;
            event.mouseButton.button = (sdlEvent.button.button == SDL_BUTTON_RIGHT)
                ? input::MouseButton::Right : input::MouseButton::Left;
            return true;
        case SDL_MOUSEMOTION:
            event.type = input::Event::MouseMoved;
            event.mouseMove.x = sdlEvent.motion.x;
            event.mouseMove.y = sdlEvent.motion.y;
            return true;
        case SDL_FINGERDOWN:
            event.type = input::Event::TouchBegan;
            event.touch.finger = sdlEvent.tfinger.fingerId;
            event.touch.x = sdlEvent.tfinger.x * m_width;
            event.touch.y = sdlEvent.tfinger.y * m_height;
            return true;
        case SDL_FINGERUP:
            event.type = input::Event::TouchEnded;
            event.touch.finger = sdlEvent.tfinger.fingerId;
            event.touch.x = sdlEvent.tfinger.x * m_width;
            event.touch.y = sdlEvent.tfinger.y * m_height;
            return true;
        case SDL_FINGERMOTION:
            event.type = input::Event::TouchMoved;
            event.touch.finger = sdlEvent.tfinger.fingerId;
            event.touch.x = sdlEvent.tfinger.x * m_width;
            event.touch.y = sdlEvent.tfinger.y * m_height;
            return true;
        default:
            continue;
        }
    }
    return false;
}
```

- [ ] **Step 3: Compile test (SDL2 backend)**

Run: `cd build && cmake .. -DUSE_SDL2=ON && make -j$(nproc) 2>&1 | tail -30`

- [ ] **Step 4: Commit**

```bash
git add src/render/SdlRenderTarget.h src/render/SdlRenderTarget.cpp
git commit -m "feat: add SDL2 render backend (SdlRenderTarget)"
```

---

### Task 9: Add touch-to-mouse input translation for Android

**Files:**
- Modify: `src/Engine.cpp`

On Android, touch events need to map to mouse events. The mapping is simple:
- Single tap = left click
- Long press (>500ms) = right click
- Drag with one finger = scroll map (when not on a unit)
- Drag to select units (rectangle selection)

- [ ] **Step 1: Add touch state tracking to Engine.h**

```cpp
// Add to Engine private members:
struct TouchState {
    bool active = false;
    ScreenPos startPos;
    int64_t startTime = 0;
    bool moved = false;
    static constexpr float MOVE_THRESHOLD = 10.f;
    static constexpr int64_t LONG_PRESS_MS = 500;
} m_touchState;
```

- [ ] **Step 2: Add handleTouchEvent to Engine.cpp**

```cpp
bool Engine::handleTouchEvent(const input::Event &event, const std::shared_ptr<GameState> &state) {
    switch (event.type) {
    case input::Event::TouchBegan: {
        m_touchState.active = true;
        m_touchState.startPos = ScreenPos(event.touch.x, event.touch.y);
        m_touchState.startTime = currentTimeMs();
        m_touchState.moved = false;
        return true;
    }
    case input::Event::TouchMoved: {
        ScreenPos pos(event.touch.x, event.touch.y);
        float dist = m_touchState.startPos.distanceTo(pos);
        if (dist > TouchState::MOVE_THRESHOLD) {
            m_touchState.moved = true;
        }
        // Scroll camera by drag delta
        if (m_touchState.moved) {
            input::Event moveEvent;
            moveEvent.type = input::Event::MouseMoved;
            moveEvent.mouseMove.x = event.touch.x;
            moveEvent.mouseMove.y = event.touch.y;
            handleMouseMove(moveEvent, state);

            // Camera scroll: move camera opposite to finger direction
            ScreenPos delta = m_touchState.startPos - pos;
            ScreenPos camScreen = renderTarget_->camera()->targetPosition().toScreen();
            camScreen.x += delta.x;
            camScreen.y -= delta.y;
            MapPos camMap = camScreen.toMap().clamped(state->map()->pixelSize());
            renderTarget_->camera()->setTargetPosition(camMap);
            m_touchState.startPos = pos;
        }
        return true;
    }
    case input::Event::TouchEnded: {
        if (!m_touchState.moved) {
            int64_t duration = currentTimeMs() - m_touchState.startTime;
            input::Event clickEvent;
            clickEvent.mouseButton.x = event.touch.x;
            clickEvent.mouseButton.y = event.touch.y;

            if (duration >= TouchState::LONG_PRESS_MS) {
                // Long press = right click
                clickEvent.type = input::Event::MouseButtonPressed;
                clickEvent.mouseButton.button = input::MouseButton::Right;
                handleMousePress(clickEvent, state);
                clickEvent.type = input::Event::MouseButtonReleased;
                handleMouseRelease(clickEvent, state);
            } else {
                // Tap = left click
                clickEvent.type = input::Event::MouseButtonPressed;
                clickEvent.mouseButton.button = input::MouseButton::Left;
                handleMousePress(clickEvent, state);
                clickEvent.type = input::Event::MouseButtonReleased;
                handleMouseRelease(clickEvent, state);
            }
        }
        m_touchState.active = false;
        return true;
    }
    default:
        return false;
    }
}
```

- [ ] **Step 3: Wire touch events in the main handleEvent**

In `Engine::handleEvent()`, add cases for touch events:
```cpp
case input::Event::TouchBegan:
case input::Event::TouchMoved:
case input::Event::TouchEnded:
    return handleTouchEvent(event, state);
```

- [ ] **Step 4: Commit**

```bash
git add src/Engine.h src/Engine.cpp
git commit -m "feat: add touch-to-mouse input translation for Android"
```

---

### Task 10: Update CMakeLists.txt with SDL2 backend option

**Files:**
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add USE_SDL2 option**

```cmake
option(USE_SDL2 "Use SDL2 backend instead of SFML" OFF)

if(ANDROID)
    set(USE_SDL2 ON CACHE BOOL "Force SDL2 on Android" FORCE)
    add_definitions(-DANDROID)
endif()

if(USE_SDL2)
    # On Android or when sources are bundled, build SDL2 from submodule
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/extern/SDL2/CMakeLists.txt)
        message(STATUS "Using bundled SDL2")
        set(SDL_SHARED ON CACHE BOOL "")
        set(SDL_STATIC OFF CACHE BOOL "")
        add_subdirectory(src/extern/SDL2)

        set(SDL_TTF_SHARED ON CACHE BOOL "")
        set(SDL_TTF_STATIC OFF CACHE BOOL "")
        add_subdirectory(src/extern/SDL2_ttf)

        set(SDL2_LIBS SDL2 SDL2_ttf)
    else()
        # Desktop: use system SDL2
        find_package(SDL2 REQUIRED)
        find_package(SDL2_ttf REQUIRED)
        set(SDL2_LIBS SDL2::SDL2 SDL2_ttf::SDL2_ttf)
    endif()

    set(RENDER_SRC
        src/render/Camera.cpp src/render/Camera.h
        src/render/GraphicRender.cpp src/render/GraphicRender.h
        src/render/IRenderer.cpp src/render/IRenderer.h
        src/render/IRenderTarget.cpp src/render/IRenderTarget.h
        src/render/MapRenderer.cpp src/render/MapRenderer.h
        src/render/SdlRenderTarget.cpp src/render/SdlRenderTarget.h
        src/render/UnitsRenderer.cpp src/render/UnitsRenderer.h
        src/render/EventTypes.h
    )

    set(ALL_LIBRARIES ${SDL2_LIBS} genieutils ${EXTRA_LIBS})
    add_definitions(-DUSE_SDL2)

    # On Android, build as shared library (loaded by SDLActivity)
    if(ANDROID)
        add_library(freeaoe SHARED src/main.cpp $<TARGET_OBJECTS:freeaoe_common>)
    endif()
else()
    find_package(SFML COMPONENTS system window graphics REQUIRED)

    set(RENDER_SRC
        src/render/Camera.cpp src/render/Camera.h
        src/render/GraphicRender.cpp src/render/GraphicRender.h
        src/render/IRenderer.cpp src/render/IRenderer.h
        src/render/IRenderTarget.cpp src/render/IRenderTarget.h
        src/render/MapRenderer.cpp src/render/MapRenderer.h
        src/render/SfmlRenderTarget.cpp src/render/SfmlRenderTarget.h
        src/render/UnitsRenderer.cpp src/render/UnitsRenderer.h
        src/render/EventTypes.h
    )

    set(ALL_LIBRARIES sfml-system sfml-window sfml-graphics genieutils ${EXTRA_LIBS})
endif()
```

- [ ] **Step 3: Compile test with SFML backend (regression check)**

Run: `cd build && cmake .. -DUSE_SDL2=OFF && make -j$(nproc) 2>&1 | tail -10`

- [ ] **Step 4: Compile test with SDL2 backend**

Run: `cd build && cmake .. -DUSE_SDL2=ON && make -j$(nproc) 2>&1 | tail -10`

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt
git commit -m "build: add USE_SDL2 cmake option, dual backend support"
```

---

### Task 11: Add #ifdef USE_SDL2 guards in Engine.cpp and main.cpp

**Files:**
- Modify: `src/Engine.cpp`
- Modify: `src/main.cpp`

The Engine main loop needs to use either SfmlWindow or SdlWindow.

- [ ] **Step 1: Conditional includes in Engine.cpp**

```cpp
#ifdef USE_SDL2
#include "render/SdlRenderTarget.h"
#else
#include "render/SfmlRenderTarget.h"
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
// ... other SFML includes
#endif
```

- [ ] **Step 2: Conditional window creation in Engine::setup()**

```cpp
#ifdef USE_SDL2
    auto sdlWindow = std::make_unique<SdlWindow>(Size(1280, 1024), "freeaoe");
    renderTarget_ = std::make_shared<SdlRenderTarget>(sdlWindow->sdlRenderer, 1280, 1024);
    m_window = std::move(sdlWindow);
#else
    renderWindow_ = std::make_unique<sf::RenderWindow>(...);
    renderTarget_ = std::make_shared<SfmlRenderTarget>(*renderWindow_);
#endif
```

- [ ] **Step 3: Conditional event loop in Engine::start()**

```cpp
#ifdef USE_SDL2
    auto *sdlWin = static_cast<SdlWindow*>(m_window.get());
    input::Event event;
    while (sdlWin->pollEvent(event)) {
        handleEvent(event, state);
        updated = true;
    }
#else
    sf::Event sfEvent;
    while (renderWindow_->pollEvent(sfEvent)) {
        input::Event event = sfEventToInput(sfEvent);
        handleEvent(event, state);
        updated = true;
    }
#endif
```

- [ ] **Step 4: Conditional main entry point**

In `main.cpp`, for Android SDL provides its own main via `SDL_main`:
```cpp
#ifdef USE_SDL2
#include <SDL2/SDL.h>
// SDL_main is #defined to map main() on Android
#endif
```

- [ ] **Step 5: Compile test both backends**

Run SFML: `cd build && cmake .. -DUSE_SDL2=OFF && make -j$(nproc) 2>&1 | tail -5`
Run SDL2: `cd build && cmake .. -DUSE_SDL2=ON && make -j$(nproc) 2>&1 | tail -5`

- [ ] **Step 6: Run SDL2 build on desktop**

Run: `cd build && ./freeaoe`

Verify window opens, assets load, map renders, mouse clicks work.

- [ ] **Step 7: Commit**

```bash
git add src/Engine.cpp src/main.cpp
git commit -m "feat: dual SFML/SDL2 backend in Engine, conditional compilation"
```

---

### Task 12: Create Android project shell

**Files:**
- Create: `android/app/src/main/AndroidManifest.xml`
- Create: `android/app/src/main/java/org/freeaoe/FreeAoEActivity.java`
- Create: `android/app/build.gradle.kts`
- Create: `android/build.gradle.kts`
- Create: `android/settings.gradle.kts`
- Create: `android/gradle.properties`
- Create: `android/local.properties`

SDL2 must be added as a git submodule for Android cross-compilation (system SDL2 won't work for NDK):

- [ ] **Step 0: Add SDL2 and SDL2_ttf as submodules**

```bash
cd /home/dima/Projects/freeaoe
unset LD_PRELOAD
git submodule add https://github.com/libsdl-org/SDL.git src/extern/SDL2
git submodule add https://github.com/libsdl-org/SDL_ttf.git src/extern/SDL2_ttf
cd src/extern/SDL2 && git checkout release-2.30.x && cd ../../..
cd src/extern/SDL2_ttf && git checkout release-2.22.x && cd ../../..
```

- [ ] **Step 1: Create AndroidManifest.xml**

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="org.freeaoe">
    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE"/>
    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE"/>
    <application
        android:label="FreeAoE"
        android:hasCode="true"
        android:hardwareAccelerated="true">
        <activity
            android:name=".FreeAoEActivity"
            android:configChanges="orientation|screenSize|screenLayout|keyboardHidden"
            android:screenOrientation="sensorLandscape"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN"/>
                <category android:name="android.intent.category.LAUNCHER"/>
            </intent-filter>
            <meta-data android:name="SDL_ENV.SDL_ACCELEROMETER_AS_JOYSTICK" android:value="0"/>
        </activity>
    </application>
</manifest>
```

- [ ] **Step 2: Create FreeAoEActivity.java**

```java
package org.freeaoe;

import org.libsdl.app.SDLActivity;
import android.os.Environment;
import java.io.File;

public class FreeAoEActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[]{
            "c++_shared",
            "SDL2",
            "SDL2_ttf",
            "freeaoe"
        };
    }

    @Override
    protected String[] getArguments() {
        // Try external files dir first, then /sdcard/Download/aoe2data
        File dataDir = new File(getExternalFilesDir(null), "aoe2data");
        if (!dataDir.exists()) {
            dataDir = new File(Environment.getExternalStorageDirectory(),
                "Download/aoe2data");
        }
        return new String[]{"--game-path", dataDir.getAbsolutePath()};
    }
}
```

Note: `"c++_shared"` is listed first because `libc++_shared.so` must be loaded before other native libs.

- [ ] **Step 3: Create build.gradle.kts files**

`android/app/build.gradle.kts`:
```kotlin
plugins {
    id("com.android.application")
}

android {
    namespace = "org.freeaoe"
    compileSdk = 36
    ndkVersion = "28.2.13676358"

    defaultConfig {
        applicationId = "org.freeaoe"
        minSdk = 24
        targetSdk = 36
        versionCode = 1
        versionName = "0.1"

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DUSE_SDL2=ON",
                    "-DANDROID_STL=c++_shared"
                )
                cppFlags += "-std=c++20"
            }
        }
        ndk {
            abiFilters += "arm64-v8a"
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}
```

`android/build.gradle.kts`:
```kotlin
plugins {
    id("com.android.application") version "8.7.0" apply false
}
```

`android/settings.gradle.kts`:
```kotlin
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolution {
    repositories {
        google()
        mavenCentral()
    }
}
include(":app")
```

`android/gradle.properties`:
```properties
android.useAndroidX=true
org.gradle.jvmargs=-Xmx2048m
```

`android/local.properties`:
```properties
sdk.dir=/home/dima/android-sdk
ndk.dir=/home/dima/.local/opt/android-sdk/ndk/28.2.13676358
```

- [ ] **Step 4: Copy SDLActivity.java from SDL2 sources**

SDL2 provides its Java helper classes that must be in the project:

```bash
mkdir -p android/app/src/main/java/org/libsdl/app/
cp src/extern/SDL2/android-project/app/src/main/java/org/libsdl/app/*.java \
   android/app/src/main/java/org/libsdl/app/
```

- [ ] **Step 5: Commit**

```bash
git add android/ src/extern/SDL2 src/extern/SDL2_ttf
git commit -m "feat: add Android project shell (SDLActivity + gradle + CMake + SDL2 submodules)"
```

---

### Task 13: Desktop SDL2 integration test

**Files:** None new — this is a verification task.

- [ ] **Step 1: Install SDL2 dev packages if not present**

Run: `sudo pacman -S sdl2 sdl2_ttf` (CachyOS/Arch)

- [ ] **Step 2: Build with SDL2 backend**

```bash
cd /home/dima/Projects/freeaoe
rm -rf build && mkdir build && cd build
cmake .. -DUSE_SDL2=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
make -j$(nproc)
```

- [ ] **Step 3: Run with AoE2 data**

```bash
./freeaoe --game-path /path/to/aoe2/data
```

Verify:
- Window opens at correct resolution
- Start screen renders (loading image)
- Home screen buttons work with mouse clicks
- Can start a game and see the map
- Units render with correct sprites
- Left click selects, right click commands
- Camera scrolls at screen edges
- Minimap renders and is clickable
- Sound plays (miniaudio — should work unchanged)

- [ ] **Step 4: Document any rendering issues for follow-up**

If circle rendering (unit selection ellipses) looks wrong, the midpoint circle algorithm may need tuning. Note issues but don't block on them.

---

## Summary

| Task | Description | Est. Complexity |
|------|-------------|-----------------|
| 1 | Purge sf:: from Types.h | Small |
| 2 | Purge sf:: from IRenderTarget API | Small |
| 3 | Create EventTypes.h | Small |
| 4 | Update SfmlRenderTarget to match | Medium |
| 5 | Migrate Resource/Sprite to Drawable | Large |
| 6 | Migrate UI layer to input::Event | Large |
| 7 | Replace sf::Clock with std::chrono | Small |
| 8 | Write SdlRenderTarget backend | Large |
| 9 | Touch-to-mouse translation | Medium |
| 10 | CMakeLists dual backend | Medium |
| 11 | Engine.cpp #ifdef guards | Medium |
| 12 | Android project shell | Small |
| 13 | Desktop SDL2 integration test | Verification |

Tasks 1-7 are the refactor phase (make codebase backend-agnostic).
Tasks 8-12 are the SDL2/Android phase (add new backend).
Task 13 is verification.

The SFML backend remains functional throughout — each commit should leave the SFML build working.
