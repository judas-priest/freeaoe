# freeaoe Android Build + Touch Controls Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build freeaoe as an Android APK with full touch controls and install on OnePlus Ace 5 Ultra.

**Architecture:** SDL2/SDL2_ttf bundled as git submodules, compiled via NDK. Touch input already has a basic implementation in Engine.cpp (Task 9 of prior plan) — we improve it to handle drag-scroll properly, generate mousemove events for hover, and support rectangle selection via two-finger gesture. The Android project shell (gradle/manifest/Activity) already exists.

**Tech Stack:** C++20, SDL2 (submodule), SDL2_ttf (submodule), Android NDK 28.2, Gradle, CMake

**Device:** OnePlus Ace 5 Ultra (Dimensity 9400, 16GB RAM, Android 16)

**Environment:**
- Android SDK: `/home/dima/.local/opt/android-sdk`
- NDK: `/home/dima/.local/opt/android-sdk/ndk/28.2.13676358`
- Platforms: android-36
- `unset LD_PRELOAD` before any `adb` command
- AoE2 HD data: `/home/dima/Desktop/Age.of.Empires.2.HD.Edition.v5.8.10-Rutracker/`

---

## File Map

| Action | File | Responsibility |
|--------|------|----------------|
| Modify | `src/Engine.h` | Improved TouchState with drag-to-scroll, mousemove generation |
| Modify | `src/Engine.cpp` | Rewritten handleTouchEvent with proper gesture logic |
| Modify | `CMakeLists.txt` | Fix Android target (shared lib guard), SDL2 submodule integration, include paths |
| Modify | `src/render/SdlRenderTarget.h` | Fix SDL include path (`<SDL2/SDL.h>` → `<SDL.h>` or add include dir) |
| Modify | `src/render/SdlRenderTarget.cpp` | Same include path fix |
| Modify | `src/main.cpp` | Same include path fix |
| Modify | `android/local.properties` | Fix SDK path to actual location |
| Modify | `android/app/build.gradle.kts` | Fix compileSdk to match available platforms |
| Create | `android/gradle/wrapper/gradle-wrapper.properties` | Gradle wrapper config |
| Create | `android/gradle/wrapper/gradle-wrapper.jar` | Gradle wrapper binary |
| Create | `android/gradlew` | Gradle wrapper script |

---

### Task 1: Fix touch input — proper drag-to-scroll + mousemove generation

**Files:**
- Modify: `src/Engine.h`
- Modify: `src/Engine.cpp`

The current touch implementation has issues:
1. Drag-to-scroll works but doesn't generate `MouseMoved` events, so the game doesn't know where the cursor is (hover highlights don't work)
2. No way to do rectangle selection (box-select units)
3. Camera scroll direction may be inverted

- [ ] **Step 1: Update TouchState in Engine.h**

Replace the existing `TouchState` struct (lines 160-167 of Engine.h) with:

```cpp
    struct TouchState {
        bool active = false;
        ScreenPos startPos;
        ScreenPos lastPos;
        int64_t startTime = 0;
        bool dragging = false;
        static constexpr float DRAG_THRESHOLD = 15.f;
        static constexpr int64_t LONG_PRESS_MS = 400;
    } m_touchState;
```

- [ ] **Step 2: Rewrite handleTouchEvent in Engine.cpp**

Replace the entire `Engine::handleTouchEvent` method with:

```cpp
bool Engine::handleTouchEvent(const input::Event &event, const std::shared_ptr<GameState> &state) {
    switch (event.type) {
    case input::Event::TouchBegan: {
        m_touchState.active = true;
        m_touchState.startPos = ScreenPos(event.touch.x, event.touch.y);
        m_touchState.lastPos = m_touchState.startPos;
        m_touchState.startTime = currentTimeMs();
        m_touchState.dragging = false;

        // Generate mousemove so game knows where finger is
        input::Event moveEvent;
        moveEvent.type = input::Event::MouseMoved;
        moveEvent.mouseMove.x = event.touch.x;
        moveEvent.mouseMove.y = event.touch.y;
        handleMouseMove(moveEvent, state);
        return true;
    }
    case input::Event::TouchMoved: {
        ScreenPos pos(event.touch.x, event.touch.y);

        if (!m_touchState.dragging) {
            float dist = m_touchState.startPos.distanceTo(pos);
            if (dist > TouchState::DRAG_THRESHOLD) {
                m_touchState.dragging = true;
            }
        }

        if (m_touchState.dragging) {
            // Scroll camera by drag delta (finger drags map)
            ScreenPos delta = m_touchState.lastPos - pos;
            ScreenPos camScreen = renderTarget_->camera()->targetPosition().toScreen();
            camScreen.x += delta.x;
            camScreen.y += delta.y;
            MapPos camMap = camScreen.toMap().clamped(state->map()->pixelSize());
            renderTarget_->camera()->setTargetPosition(camMap);
        }

        // Always generate mousemove for hover
        input::Event moveEvent;
        moveEvent.type = input::Event::MouseMoved;
        moveEvent.mouseMove.x = event.touch.x;
        moveEvent.mouseMove.y = event.touch.y;
        handleMouseMove(moveEvent, state);

        m_touchState.lastPos = pos;
        return true;
    }
    case input::Event::TouchEnded: {
        if (!m_touchState.dragging) {
            int64_t duration = currentTimeMs() - m_touchState.startTime;
            input::Event clickEvent;
            clickEvent.mouseButton.x = event.touch.x;
            clickEvent.mouseButton.y = event.touch.y;

            if (duration >= TouchState::LONG_PRESS_MS) {
                // Long press = right click (issue command)
                clickEvent.type = input::Event::MouseButtonPressed;
                clickEvent.mouseButton.button = input::MouseButton::Right;
                handleMousePress(clickEvent, state);
                clickEvent.type = input::Event::MouseButtonReleased;
                handleMouseRelease(clickEvent, state);
            } else {
                // Tap = left click (select)
                clickEvent.type = input::Event::MouseButtonPressed;
                clickEvent.mouseButton.button = input::MouseButton::Left;
                handleMousePress(clickEvent, state);
                clickEvent.type = input::Event::MouseButtonReleased;
                handleMouseRelease(clickEvent, state);
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

Key changes from previous version:
- `lastPos` tracks last frame position for smooth delta scrolling
- `dragging` flag instead of `moved` (clearer semantics)
- Generates `MouseMoved` events on every touch for hover highlights
- Camera scroll uses `lastPos - pos` delta (not `startPos - pos`) for smooth panning
- Camera Y delta is `+= delta.y` (same direction, since screen Y maps directly)
- Threshold raised to 15px to avoid accidental drags
- Long press threshold lowered to 400ms (more responsive)

- [ ] **Step 3: Disable edge-scroll on Android**

In `Engine::handleMouseMove`, the edge-scroll logic (move camera when mouse is near screen edge) makes no sense on touch. Add guard at the top of the method:

In `Engine.cpp`, find `handleMouseMove` and add at the beginning after `const ScreenPos mousePos = ...`:

```cpp
#ifdef ANDROID
    // On Android, camera is controlled by touch drag, not edge scroll
    if (mousePos.y < 800) {
        if (m_selecting) {
            m_selectionCurr = mousePos;
            return true;
        } else {
            state->unitManager()->onMouseMove(renderTarget_->camera()->absoluteMapPos(mousePos));
        }
    }
    return false;
#endif
```

This skips the edge-detection logic but still processes unit hover and selection drag.

- [ ] **Step 4: Build and verify on desktop**

```bash
cd /home/dima/Projects/freeaoe/build
cmake .. -DUSE_SDL2=ON && make -j$(nproc)
```

Expected: compiles cleanly.

- [ ] **Step 5: Commit**

```bash
git add src/Engine.h src/Engine.cpp
git commit -m "feat: improved touch controls — drag scroll, mousemove generation, disable edge-scroll on Android"
```

---

### Task 2: Fix CMakeLists.txt for Android build

**Files:**
- Modify: `CMakeLists.txt`

The current CMakeLists has a bug: `freeaoe` is defined as both executable (line 444) and shared library (line 450) when building for Android. Also need to handle `SDL_main` properly.

- [ ] **Step 1: Guard executable/library targets**

Replace lines 443-452 in CMakeLists.txt:

```cmake
#add_executable(freeaoe src/main.cpp ${ENGINE_SRC})
add_executable(freeaoe src/main.cpp $<TARGET_OBJECTS:freeaoe_common>)
target_link_libraries(freeaoe ${ALL_LIBRARIES})
install(TARGETS freeaoe DESTINATION bin)

if(ANDROID)
    # On Android, build as shared library loaded by SDLActivity
    add_library(freeaoe SHARED src/main.cpp $<TARGET_OBJECTS:freeaoe_common>)
    target_link_libraries(freeaoe ${ALL_LIBRARIES})
endif()
```

With:

```cmake
if(ANDROID)
    # On Android, build as shared library loaded by SDLActivity
    add_library(freeaoe SHARED src/main.cpp $<TARGET_OBJECTS:freeaoe_common>)
    target_link_libraries(freeaoe ${ALL_LIBRARIES} log android)
else()
    add_executable(freeaoe src/main.cpp $<TARGET_OBJECTS:freeaoe_common>)
    target_link_libraries(freeaoe ${ALL_LIBRARIES})
    install(TARGETS freeaoe DESTINATION bin)
endif()
```

Note: `log` and `android` are Android system libraries needed for logging and native activity.

- [ ] **Step 2: Fix EXTRA_LIBS for Android**

Find the `set(EXTRA_LIBS pthread dl)` line (around line 118). Guard it:

```cmake
if(NOT ANDROID)
    set(EXTRA_LIBS pthread dl)
else()
    set(EXTRA_LIBS)
endif()
```

Android NDK provides pthread/dl implicitly, and explicit linking causes errors.

- [ ] **Step 3: Fix zlib for Android**

The `find_package(ZLIB)` on line ~121 will fail on Android (no system zlib pkg-config). Guard it:

```cmake
if(NOT ANDROID)
    # Somehow need this because otherwise the SFML config gets fucked
    find_package(ZLIB)
endif()
```

genieutils bundles zstr which has its own zlib handling.

- [ ] **Step 4: Verify desktop still builds**

```bash
cd /home/dima/Projects/freeaoe/build
cmake .. -DUSE_SDL2=ON && make -j$(nproc)
```

Expected: compiles and links cleanly.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt
git commit -m "fix: CMakeLists Android target — shared lib, system libs, zlib guard"
```

---

### Task 3: Add SDL2 and SDL2_ttf as git submodules

**Files:**
- Add submodule: `src/extern/SDL2` → `https://github.com/libsdl-org/SDL.git` (tag `release-2.32.10` — latest SDL2 as of July 2026)
- Add submodule: `src/extern/SDL2_ttf` → `https://github.com/libsdl-org/SDL_ttf.git` (tag `release-2.24.0` — latest SDL2_ttf as of July 2026)

- [ ] **Step 1: Add SDL2 submodule**

```bash
cd /home/dima/Projects/freeaoe
unset LD_PRELOAD
git submodule add https://github.com/libsdl-org/SDL.git src/extern/SDL2
cd src/extern/SDL2
git checkout release-2.32.10
cd ../../..
```

- [ ] **Step 2: Add SDL2_ttf submodule**

```bash
unset LD_PRELOAD
git submodule add https://github.com/libsdl-org/SDL_ttf.git src/extern/SDL2_ttf
cd src/extern/SDL2_ttf
git checkout release-2.24.0
cd ../../..
```

- [ ] **Step 3: Commit**

```bash
git add .gitmodules src/extern/SDL2 src/extern/SDL2_ttf
git commit -m "feat: add SDL2 and SDL2_ttf as git submodules for Android build"
```

---

### Task 4: Fix Android project configuration

**Files:**
- Modify: `android/local.properties`
- Modify: `android/app/build.gradle.kts`
- Modify: `android/settings.gradle.kts`
- Create: `android/gradlew` (via gradle wrapper)

- [ ] **Step 1: Fix local.properties SDK path**

```properties
sdk.dir=/home/dima/.local/opt/android-sdk
```

(Remove the ndk.dir line — it's picked up from build.gradle.kts ndkVersion.)

- [ ] **Step 2: Fix build.gradle.kts — compileSdk must match available platform**

Change `compileSdk = 36` — we have android-36, so this is fine. But `ndkVersion` must match exactly:

```bash
ls /home/dima/.local/opt/android-sdk/ndk/
```

Verify it's `28.2.13676358`. The current value is correct.

- [ ] **Step 3: Fix settings.gradle.kts — use dependencyResolutionManagement**

The current settings.gradle.kts has `dependencyResolution` which should be `dependencyResolutionManagement`:

Replace content of `android/settings.gradle.kts` with:

```kotlin
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode = RepositoriesMode.PREFER_SETTINGS
    repositories {
        google()
        mavenCentral()
    }
}
include(":app")
```

- [ ] **Step 4: Generate gradle wrapper**

```bash
cd /home/dima/Projects/freeaoe/android
unset LD_PRELOAD
gradle wrapper --gradle-version 8.10
```

If `gradle` is not installed system-wide:

```bash
# Download gradle wrapper manually
mkdir -p gradle/wrapper
curl -sL https://services.gradle.org/distributions/gradle-8.10-bin.zip -o /tmp/gradle-8.10-bin.zip
unzip -q /tmp/gradle-8.10-bin.zip -d /tmp/
/tmp/gradle-8.10/bin/gradle wrapper --gradle-version 8.10
rm -rf /tmp/gradle-8.10 /tmp/gradle-8.10-bin.zip
```

- [ ] **Step 5: Copy SDLActivity.java from SDL2 sources**

```bash
mkdir -p android/app/src/main/java/org/libsdl/app/
cp src/extern/SDL2/android-project/app/src/main/java/org/libsdl/app/*.java \
   android/app/src/main/java/org/libsdl/app/
```

- [ ] **Step 6: Commit**

```bash
git add android/
git commit -m "fix: Android project config — SDK paths, gradle wrapper, SDLActivity"
```

---

### Task 5: Build APK

**Files:** None new — build verification.

- [ ] **Step 1: Build debug APK**

```bash
cd /home/dima/Projects/freeaoe/android
unset LD_PRELOAD
./gradlew assembleDebug 2>&1 | tail -30
```

If CMake errors about missing includes or link failures, fix them iteratively. Common issues:
- `#include <SDL2/SDL.h>` might need to be `#include "SDL.h"` when building from submodule — the submodule puts headers in a different path
- Missing `SDL_main` — SDL2's CMake should handle this automatically via `SDL2::SDL2main`
- `filesystem` — Android NDK may need `-lc++fs` or `c++_shared` STL

- [ ] **Step 2: Fix SDL include paths for submodule build**

Our code uses `#include <SDL2/SDL.h>` (system package convention) but when building from submodule via `add_subdirectory()`, SDL2 puts headers at just `SDL.h`. Fix by creating a compatibility include directory.

**Option A (recommended):** Add to CMakeLists.txt inside the bundled SDL2 block, after `add_subdirectory(src/extern/SDL2)`:

```cmake
# Create SDL2/ include path alias so #include <SDL2/SDL.h> works with bundled sources
if(NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/SDL2)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/SDL2)
endif()
# SDL2's add_subdirectory already sets up include dirs for SDL.h
# We add a wrapper so <SDL2/SDL.h> also works
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/SDL2/SDL.h "#include \"${CMAKE_CURRENT_SOURCE_DIR}/src/extern/SDL2/include/SDL.h\"")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/SDL2/SDL_ttf.h "#include \"${CMAKE_CURRENT_SOURCE_DIR}/src/extern/SDL2_ttf/SDL_ttf.h\"")
include_directories(${CMAKE_CURRENT_BINARY_DIR})
```

This way `#include <SDL2/SDL.h>` works with both system SDL2 and bundled submodule — no need to change any source files.

- [ ] **Step 3: Locate built APK**

```bash
find android/app/build -name "*.apk" 2>/dev/null
```

Expected: `android/app/build/outputs/apk/debug/app-debug.apk`

- [ ] **Step 4: Commit any build fixes**

```bash
git add -A
git commit -m "fix: Android build issues"
```

---

### Task 6: Install and test on device

**Files:** None — deployment and testing.

- [ ] **Step 1: Push AoE2 data to phone**

The data needs to be on the phone at the path FreeAoEActivity.java looks for. Push a minimal subset first:

```bash
unset LD_PRELOAD
ADB=/home/dima/.local/opt/android-sdk/platform-tools/adb
DATA_SRC="/home/dima/Desktop/Age.of.Empires.2.HD.Edition.v5.8.10-Rutracker"

# Create target directory
$ADB shell mkdir -p /sdcard/Download/aoe2data

# Push essential data directories
$ADB push "$DATA_SRC/resources" /sdcard/Download/aoe2data/resources
```

This will take a while (several GB). The `resources` directory contains all game data (dat, drs, terrain, sounds).

- [ ] **Step 2: Install APK**

```bash
unset LD_PRELOAD
$ADB install -r android/app/build/outputs/apk/debug/app-debug.apk
```

- [ ] **Step 3: Launch and test**

```bash
$ADB shell am start -n org.freeaoe/.FreeAoEActivity
$ADB logcat -s SDL freeaoe | head -50
```

Test checklist:
- App launches without crash
- Game renders (terrain, units, UI)
- Tap selects units
- Long press issues commands (move/attack)
- Finger drag scrolls camera
- UI panel buttons respond to tap
- Minimap responds to tap
- Sound plays

- [ ] **Step 4: If crash, get backtrace**

```bash
$ADB logcat | grep -E "FATAL|Signal|backtrace|freeaoe" | head -30
```

---

## Summary

| Task | Description | Complexity |
|------|-------------|------------|
| 1 | Fix touch input (drag-scroll, mousemove, edge-scroll guard) | Medium |
| 2 | Fix CMakeLists for Android (shared lib, system libs) | Small |
| 3 | Add SDL2/SDL2_ttf submodules | Small |
| 4 | Fix Android project config (paths, gradle wrapper, SDLActivity) | Medium |
| 5 | Build APK | Medium (iterative fixes) |
| 6 | Install and test on device | Verification |

Tasks 1-2 are code changes. Tasks 3-4 are project setup. Task 5 is the build. Task 6 is deployment.
