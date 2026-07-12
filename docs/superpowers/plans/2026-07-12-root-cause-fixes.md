# Root Cause Fixes — Menus, Double-Tap, Russian Language

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix three Android-port bugs: game menus don't open (buttons respond but nothing appears), double-tap is unreliable, Russian text is invisible.

**Architecture:** Three independent fixes:
1. Menu: `TextButton::render()` is an empty stub under SDL2 — dialog opens but buttons are invisible. Implement SDL2 rendering using existing `SdlRenderTarget::draw()` APIs.
2. Double-tap: Dual event system (touch + synthesized mouse) causes duplicate `selectUnits`/`onRightClick` calls and interferes with double-tap detection. Fix: disable `SDL_HINT_TOUCH_MOUSE_EVENTS`, rely solely on touch handler which already has complete logic.
3. Russian: All three bundled fonts (Alegreya-Bold.latin, BerryRotunda, CharisSILEur) lack Cyrillic glyphs — text renders as blank. String loading order is also wrong (EN overwrites RU).

**Tech Stack:** C++20, SDL2, SDL_ttf, Android NDK

**Verified against:**
- [SDL Wiki: SDL_RenderSetLogicalSize](https://wiki.libsdl.org/SDL2/SDL_RenderSetLogicalSize) — mouse events ARE auto-scaled to logical coords (no coordinate mismatch)
- [SDL Wiki: SDL_HINT_TOUCH_MOUSE_EVENTS](https://wiki.libsdl.org/SDL2/SDL_HINT_TOUCH_MOUSE_EVENTS) — setting to "0" disables touch→mouse synthesis
- [Android ViewConfiguration](https://developer.android.com/reference/android/view/ViewConfiguration) — standard touch slop is 8dp (~22px physical)
- [Android GestureDetector](https://developer.android.com/reference/android/view/GestureDetector.OnDoubleTapListener) — standard double-tap timeout is 300ms
- [SDL_ttf#193](https://github.com/libsdl-org/SDL_ttf/issues/193) — TTF_RenderUTF8_Blended is >1s slow on Android; must cache textures
- [Charis SIL](https://software.sil.org/charis/) — v7.000 (June 2025), renamed to "Charis", full Cyrillic, OFL license
- [Alegreya Google Fonts](https://fonts.google.com/specimen/Alegreya?subset=cyrillic-ext) — full Cyrillic+CyrillicExt support confirmed

---

### Task 1: Implement TextButton::render() for SDL2

The SFML version draws a filled rectangle background, 3-pixel beveled border, and centered text. We replicate this using `SdlRenderTarget::draw(ScreenRect, fillColor)` for rectangles and `SdlRenderTarget::draw(Drawable::Text::Ptr)` for text. The text rendering already caches SDL textures (`SdlText::cachedTexture`), so slow `TTF_RenderUTF8_Blended` only runs once per string change.

**Files:**
- Modify: `src/ui/TextButton.h`
- Modify: `src/ui/TextButton.cpp`
- Modify: `src/ui/Dialog.cpp`

- [ ] **Step 1: Add render target and text members to TextButton for SDL2**

In `src/ui/TextButton.h`, replace the struct:

```cpp
struct TextButton
{
    std::string text;
    ScreenRect rect;
    bool pressed = false;

    TextButton();

    void render(UiScreen *screen);

#ifdef USE_SDL2
    void setRenderTarget(const std::shared_ptr<IRenderTarget> &rt);
private:
    std::shared_ptr<IRenderTarget> m_renderTarget;
    Drawable::Text::Ptr m_text;
#else
private:
    static void drawLine(const ScreenPos &from, const ScreenPos &to, const sf::Color &color, UiScreen *screen);
    sf::Text m_text;
#endif
};
```

- [ ] **Step 2: Implement TextButton for SDL2**

Replace the SDL2 stub in `src/ui/TextButton.cpp` (lines 23-32):

```cpp
#ifdef USE_SDL2

#include "render/IRenderTarget.h"

TextButton::TextButton()
{
}

void TextButton::setRenderTarget(const std::shared_ptr<IRenderTarget> &rt)
{
    m_renderTarget = rt;
    m_text = rt->createText(Drawable::Text::Plain);
    m_text->pointSize = 17;
}

void TextButton::render(UiScreen *screen)
{
    if (!m_renderTarget || !m_text) {
        return;
    }

    // Background
    Drawable::Color bgColor(0, 0, 0, static_cast<uint8_t>(screen->m_buttonOpacity * 255));
    m_renderTarget->draw(rect, bgColor);

    // Bevel colors — swap when pressed (same logic as SFML version)
    Drawable::Color inner1 = screen->m_bevelColor1a;
    Drawable::Color middle1 = screen->m_bevelColor1b;
    Drawable::Color outer1 = screen->m_bevelColor1c;
    Drawable::Color inner2 = screen->m_bevelColor2a;
    Drawable::Color middle2 = screen->m_bevelColor2b;
    Drawable::Color outer2 = screen->m_bevelColor2c;
    if (pressed) {
        std::swap(inner1, inner2);
        std::swap(middle1, middle2);
        std::swap(outer1, outer2);
    }

    // Top border (3 lines)
    m_renderTarget->draw(ScreenRect(rect.x - 2, rect.y - 2, rect.width + 4, 1), inner1);
    m_renderTarget->draw(ScreenRect(rect.x - 1, rect.y - 1, rect.width + 2, 1), middle1);
    m_renderTarget->draw(ScreenRect(rect.x,     rect.y,     rect.width,     1), outer1);

    // Bottom border
    m_renderTarget->draw(ScreenRect(rect.x - 2, rect.y + rect.height + 1, rect.width + 4, 1), inner2);
    m_renderTarget->draw(ScreenRect(rect.x - 1, rect.y + rect.height,     rect.width + 2, 1), middle2);
    m_renderTarget->draw(ScreenRect(rect.x,     rect.y + rect.height - 1, rect.width,     1), outer2);

    // Left border
    m_renderTarget->draw(ScreenRect(rect.x - 2, rect.y - 2, 1, rect.height + 4), inner2);
    m_renderTarget->draw(ScreenRect(rect.x - 1, rect.y - 1, 1, rect.height + 2), middle2);
    m_renderTarget->draw(ScreenRect(rect.x,     rect.y,     1, rect.height),      outer2);

    // Right border
    m_renderTarget->draw(ScreenRect(rect.x + rect.width - 1, rect.y,     1, rect.height),     outer1);
    m_renderTarget->draw(ScreenRect(rect.x + rect.width,     rect.y - 1, 1, rect.height + 2), middle1);
    m_renderTarget->draw(ScreenRect(rect.x + rect.width + 1, rect.y - 2, 1, rect.height + 4), inner1);

    // Text — uses cached SDL texture (re-renders only when string changes)
    m_text->string = text;
    m_text->color = screen->m_textFillColor;
    m_text->outlineColor = screen->m_textOutlineColor;

    Size textSize = m_text->size();
    float tx = rect.x + (rect.width - textSize.width) / 2.f;
    float ty = rect.y + (rect.height - textSize.height) / 2.f;
    if (pressed) {
        tx += screen->m_pressOffset;
        ty += screen->m_pressOffset;
    }
    m_text->position = ScreenPos(tx, ty);

    m_renderTarget->draw(m_text);
}

#else // SFML
```

- [ ] **Step 3: Pass render target to buttons in Dialog::render**

Replace the SDL2 `Dialog::render` method in `src/ui/Dialog.cpp` (lines 26-34):

```cpp
#ifdef USE_SDL2
void Dialog::render(const std::shared_ptr<IRenderTarget> &renderTarget)
{
    Size windowSize = renderTarget->getSize();
    Size textureSize(295, 300);
    const ScreenPos windowCenter(windowSize.width / 2, windowSize.height / 2);
    ScreenPos position(windowCenter.x - textureSize.width/2, windowCenter.y - textureSize.height/2);

    // Semi-transparent overlay behind dialog
    renderTarget->draw(ScreenRect(0, 0, windowSize.width, windowSize.height),
                       Drawable::Color(0, 0, 0, 128));

    if (background) {
        renderTarget->draw(background, position);
    }

    const int buttonWidth = textureSize.width - 80;
    const int buttonHeight = 30;
    const int buttonMargin = 10;
    const int allButtonsHeight = ChoicesCount * (buttonHeight + buttonMargin);

    const int x = windowCenter.x - buttonWidth / 2.f;
    int y = windowCenter.y - allButtonsHeight / 2.f;

    for (int i=0; i<ChoicesCount; i++) {
        m_buttons[i].setRenderTarget(renderTarget);
        m_buttons[i].rect.x = x;
        m_buttons[i].rect.y = y;
        m_buttons[i].rect.height = buttonHeight;
        m_buttons[i].rect.width = buttonWidth;
        y += buttonHeight + buttonMargin;
        m_buttons[i].render(m_screen);
    }
}
#else
```

Note: `setRenderTarget` is idempotent — calling it every frame is fine because it only creates the text object once and reuses it. The `Drawable::Text::Ptr` is a shared_ptr, so assignment is a no-op after first call. If profiling shows overhead, cache a bool.

- [ ] **Step 4: Add touch event handling to Dialog::handleEvent**

`Dialog::handleEvent` only handles `MouseButtonPressed`/`MouseButtonReleased`. Touch events (`TouchBegan`/`TouchEnded`) arrive with correct logical coordinates and must also be handled. Replace `Dialog::handleEvent` in `src/ui/Dialog.cpp`:

```cpp
Dialog::Choice Dialog::handleEvent(const input::Event &event)
{
    ScreenPos pos;
    bool isPress = false;
    bool isRelease = false;

    if (event.type == input::Event::MouseButtonPressed) {
        pos = ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isPress = true;
    } else if (event.type == input::Event::MouseButtonReleased) {
        pos = ScreenPos(event.mouseButton.x, event.mouseButton.y);
        isRelease = true;
    } else if (event.type == input::Event::TouchBegan) {
        pos = ScreenPos(event.touch.x, event.touch.y);
        isPress = true;
    } else if (event.type == input::Event::TouchEnded) {
        pos = ScreenPos(event.touch.x, event.touch.y);
        isRelease = true;
    } else {
        return Invalid;
    }

    if (isPress) {
        for (int i=0; i<ChoicesCount; i++) {
            m_buttons[i].pressed = m_buttons[i].rect.contains(pos);
            if (m_buttons[i].pressed) {
                m_pressedButton = Choice(i);
            }
        }
        return Invalid;
    }

    // Release
    Choice choice = Invalid;
    for (int i=0; i<ChoicesCount; i++) {
        m_buttons[i].pressed = false;
        if (m_buttons[i].rect.contains(pos)) {
            choice = Choice(i);
        }
    }
    if (choice != m_pressedButton) {
        m_pressedButton = Invalid;
    }
    return m_pressedButton;
}
```

- [ ] **Step 5: Build and test**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Test: tap GameMenu button → dialog should appear with visible "Quit", "Cancel" etc. Tap Cancel → closes. Tap Quit → app exits.

- [ ] **Step 6: Commit**

```bash
git add src/ui/TextButton.h src/ui/TextButton.cpp src/ui/Dialog.cpp
git commit -m "$(cat <<'EOF'
fix: implement TextButton rendering for SDL2 — menu dialog now visible

TextButton::render() was an empty stub under USE_SDL2. Implemented
using SdlRenderTarget draw APIs: filled rect background, 3-pixel
beveled border, centered text with outline. Text uses cached SDL
textures so TTF_RenderUTF8_Blended (slow on Android) runs only once.
Added touch event handling to Dialog::handleEvent.
EOF
)"
```

---

### Task 2: Fix double-tap reliability

Two problems: (a) dual event system — `SDL_HINT_TOUCH_MOUSE_EVENTS = "1"` makes every touch fire BOTH `TouchBegan/Ended` AND synthesized `MouseButtonPressed/Released`, so both `handleTouchEvent` and `handleMouseRelease` process the same tap, causing duplicate `selectUnits` and `onRightClick` calls; (b) `DRAG_THRESHOLD = 25` logical pixels, while already above Android's 8dp touch slop, could be more generous for fat-finger taps.

Note: SDL2 auto-scales synthesized mouse coordinates to logical resolution via `SDL_RenderSetLogicalSize`, so there is no coordinate mismatch — both systems receive correct logical coordinates. The problem is purely that both systems run.

**Files:**
- Modify: `src/Engine.cpp:912` (hint value)
- Modify: `src/Engine.h:171` (drag threshold)
- Modify: `src/Engine.cpp:686-715` (remove dead Android mouse press code)
- Modify: `src/Engine.cpp:826-889` (remove dead Android mouse release code)

- [ ] **Step 1: Disable touch-to-mouse synthesis**

In `src/Engine.cpp:912`, change:

```cpp
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0"); // handle touch directly, no mouse synthesis
```

This eliminates the dual-system conflict. `handleTouchEvent` at lines 717-824 already handles everything: buttons, action panel, minimap, unit selection, double-tap→right-click, drag→camera scroll.

- [ ] **Step 2: Increase drag threshold**

In `src/Engine.h:171`, change:

```cpp
        static constexpr float DRAG_THRESHOLD = 50.f;
```

50 logical pixels ≈ 94 physical pixels on a 2400-wide screen. Android's touch slop is 8dp ≈ 22px, so 50 logical is very generous but appropriate for the double-tap use case where fingers naturally shift between taps.

- [ ] **Step 3: Remove `#ifdef ANDROID` blocks from mouse handlers**

With touch-to-mouse synthesis disabled, synthesized mouse events never fire on Android. Remove dead code:

In `handleMousePress` (`src/Engine.cpp`), remove the `#ifdef ANDROID` block (lines 689-713 approximately) that sets `m_input.mouseDown`, `pressPos`, `pressTime`, and calls `onLeftClick`.

In `handleMouseRelease` (`src/Engine.cpp`), remove the `#ifdef ANDROID` block (lines 830-851) that handles `m_input.dragging`, double-click detection, and `lastClickTime`.

- [ ] **Step 4: Clean up InputState struct**

In `src/Engine.h`, remove fields that were only used by the Android mouse code: `mouseDown`, `dragging`, `pressPos`, `lastMovePos`, `pressTime`, `lastClickTime`, `lastClickPos`, `DRAG_THRESHOLD`, `DOUBLE_CLICK_MS`, `DOUBLE_CLICK_DIST`. If nothing else uses `InputState`, remove the entire struct.

Remove `suppressNextMouseRelease` from `TouchState` if present — it was a workaround for the dual event system.

- [ ] **Step 5: Build and test**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Test:
- Single tap unit → selects (once, not twice)
- Double-tap ground → unit moves (one move command, not two)
- Slightly sloppy second tap → should still register as double-tap
- Drag to scroll → camera scrolls
- Pinch to zoom → still works (uses SDL_MULTIGESTURE, unaffected)

- [ ] **Step 6: Commit**

```bash
git add src/Engine.h src/Engine.cpp
git commit -m "$(cat <<'EOF'
fix: disable mouse synthesis on Android, increase drag threshold

Set SDL_HINT_TOUCH_MOUSE_EVENTS to "0" — touch handler already has
complete input logic. The dual system caused duplicate selectUnits
and onRightClick calls, making double-tap unreliable.

Increase DRAG_THRESHOLD from 25 to 50 logical pixels for more
forgiving double-tap detection on touchscreens.

Remove dead #ifdef ANDROID code from mouse handlers and unused
InputState fields.
EOF
)"
```

---

### Task 3: Replace fonts with Cyrillic-capable versions

All three bundled fonts lack Cyrillic glyphs. Russian text renders as blank rectangles.

| Font | Glyphs | Cyrillic | Used for |
|------|--------|----------|----------|
| Alegreya-Bold.latin | 209 | 0 | UI: FPS, resources, dialog text |
| BerryRotunda.ttf | 211 | 0 | Stylish: decorative text |
| CharisSILEur-R.ttf | 677 | 2 | Plain: unit names, messages, most text |

**Files:**
- Replace: `misc/fonts/charis/CharisSILEur-R.ttf` → full Charis-Regular.ttf
- Replace: `misc/fonts/Alegreya/Alegreya-Bold.latin` → full Alegreya-Bold.ttf
- Modify: font embedding build step (CMakeLists.txt or xxd commands)
- Modify: `src/render/SdlRenderTarget.cpp:45-48` (if embedded symbol names change)

- [ ] **Step 1: Find font embedding mechanism**

```bash
grep -rn "resource_CharisSILEur_R_ttf\|xxd\|bin2c\|EMBED\|font.*\.h" CMakeLists.txt cmake/ src/render/SdlRenderTarget.cpp --include="*.cmake" --include="*.txt" --include="*.cpp"
```

Identify the build step that converts .ttf files to C byte arrays and what symbol names it generates.

- [ ] **Step 2: Download full Charis font (v7.000, June 2025)**

Charis v7.000 renamed from "Charis SIL" to "Charis". Download from [GitHub releases](https://github.com/silnrsi/font-charis/releases):

```bash
cd /home/dima/Projects/freeaoe/misc/fonts/charis
unset LD_PRELOAD && curl -L "https://github.com/silnrsi/font-charis/releases/download/v7.000/Charis-7.000.zip" -o charis.zip
unzip -l charis.zip | grep -i regular  # find exact filename
unzip -j charis.zip "*/Charis-Regular.ttf" -d .
rm charis.zip
```

Verify Cyrillic support:

```bash
fc-query Charis-Regular.ttf | grep -i cyrillic
```

Expected: lines showing Cyrillic script coverage.

If the filename is different (e.g. `Charis-R.ttf`), adjust accordingly.

- [ ] **Step 3: Download full Alegreya Bold**

From [Google Fonts](https://fonts.google.com/specimen/Alegreya) or [GitHub](https://github.com/huertatipografica/Alegreya):

```bash
cd /home/dima/Projects/freeaoe/misc/fonts/Alegreya
unset LD_PRELOAD && curl -L "https://github.com/huertatipografica/Alegreya/raw/master/fonts/ttf/Alegreya-Bold.ttf" -o Alegreya-Bold.ttf
fc-query Alegreya-Bold.ttf | grep -i cyrillic
```

Expected: Cyrillic and Cyrillic Extended support confirmed.

- [ ] **Step 4: Update font references**

Based on Step 1 findings, update the build system to embed the new font files. Then update `src/render/SdlRenderTarget.cpp` lines 45-48 if generated symbol names changed:

```cpp
    // If the build system generates symbols from filenames:
    s_plainFont.data = resource_Charis_Regular_ttf_data;        // was resource_CharisSILEur_R_ttf_data
    s_plainFont.dataSize = resource_Charis_Regular_ttf_size;    // was resource_CharisSILEur_R_ttf_size

    s_uiFont.data = resource_Alegreya_Bold_ttf_data;            // was resource_Alegreya_Bold_latin_data
    s_uiFont.dataSize = resource_Alegreya_Bold_ttf_size;        // was resource_Alegreya_Bold_latin_size
```

Note: The new fonts are larger (~500KB Charis vs 200KB, ~200KB Alegreya vs 29KB). This increases APK size by ~500KB — acceptable.

- [ ] **Step 5: Build and test**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Test: unit names, resource labels, messages, dialog buttons — all should display Cyrillic text (Russian characters visible, not blank boxes).

- [ ] **Step 6: Commit**

```bash
git add misc/fonts/ src/render/SdlRenderTarget.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
fix: replace Latin-only fonts with full versions including Cyrillic

CharisSILEur-R.ttf (European subset, 2 Cyrillic glyphs) replaced with
Charis-Regular.ttf v7.000 (full Latin+Cyrillic, 3600+ glyphs).
Alegreya-Bold.latin (Latin subset) replaced with full Alegreya-Bold.ttf.
Russian text was rendering as blank rectangles due to missing glyphs.
EOF
)"
```

---

### Task 4: Fix Russian string loading order

`LanguageManager::loadTxtFile()` does `m_cache[id] = text` unconditionally — last file loaded wins. Current order: RU files first, then EN fallback files. English overwrites Russian for every overlapping string ID.

**Files:**
- Modify: `src/resource/LanguageManager.cpp:32-43`

- [ ] **Step 1: Load English first, then Russian on top**

In `src/resource/LanguageManager.cpp`, replace lines 32-43:

```cpp
        const std::string language = Config::Inst().getValue(Config::Language);
        std::vector<std::string> extraStringFiles;

        if (language != "en") {
            // English first as fallback base
            extraStringFiles.emplace_back("/resources/en/strings/history/history-utf8.txt");
            extraStringFiles.emplace_back("/resources/en/strings/key-value/key-value-strings-utf8.txt");
            extraStringFiles.emplace_back("/resources/en/strings/key-value/key-value-modded-strings-utf8.txt");
        }

        // Target language on top — overwrites English for overlapping IDs
        extraStringFiles.push_back("/resources/" + language + "/strings/history/history-utf8.txt");
        extraStringFiles.push_back("/resources/" + language + "/strings/key-value/key-value-strings-utf8.txt");
        extraStringFiles.push_back("/resources/" + language + "/strings/key-value/key-value-modded-strings-utf8.txt");
```

- [ ] **Step 2: Build and test**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Test: with Cyrillic fonts from Task 3, unit names and UI strings should display in Russian, not English.

- [ ] **Step 3: Commit**

```bash
git add src/resource/LanguageManager.cpp
git commit -m "$(cat <<'EOF'
fix: load English strings first so Russian overwrites them

loadTxtFile() unconditionally sets m_cache[id] = text — last writer
wins. Was loading RU then EN, so English overwrote Russian for all
overlapping string IDs. Now loads EN first as fallback, then RU on top.
EOF
)"
```

---

## Summary

| Task | Root Cause | Fix | Independent |
|------|-----------|-----|-------------|
| 1 | `TextButton::render()` empty SDL2 stub | Implement SDL2 rendering with draw APIs + touch event handling | Yes |
| 2 | Dual touch+mouse event system causes duplicate actions | Disable `SDL_HINT_TOUCH_MOUSE_EVENTS`, increase drag threshold | Yes |
| 3 | Fonts lack Cyrillic glyphs (Latin-only subsets) | Replace with full Charis v7 + full Alegreya Bold | Yes |
| 4 | EN strings loaded after RU, overwriting them | Reverse load order: EN first, RU on top | Yes |

All 4 tasks are fully independent — can be executed in any order or in parallel.
