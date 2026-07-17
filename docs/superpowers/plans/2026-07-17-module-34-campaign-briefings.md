# Module 34: Campaign Briefing Screens — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show scenario briefing/objectives text before each campaign mission starts, using the scenario's built-in description strings from the .scx file.

**Architecture:** Create a BriefingScreen (UiScreen subclass) that displays the scenario's description text (from ScnFile instructions/hints). Show it after scenario selection and before gameplay begins. Player clicks "Start" to proceed.

**Tech Stack:** C++, SDL2, genie ScnFile data

---

### Task 1: Create BriefingScreen UI

**Files:**
- Create: `src/ui/BriefingScreen.h`
- Create: `src/ui/BriefingScreen.cpp`

- [ ] **Step 1: Create BriefingScreen header**

Create `src/ui/BriefingScreen.h`:
```cpp
#pragma once

#include "ui/UiScreen.h"
#include <string>

class BriefingScreen : public UiScreen
{
public:
    BriefingScreen(const std::string &scenarioName,
                   const std::string &description,
                   const std::string &hints);

    bool init() override;
    void render() override;
    bool handleMouseEvent(const input::Event &event) override;
    void handleKeyEvent(const input::Event &event) override;

    bool startClicked() const { return m_startClicked; }

private:
    std::string m_scenarioName;
    std::string m_description;
    std::string m_hints;
    bool m_startClicked = false;

    Drawable::Text::Ptr m_titleText;
    Drawable::Text::Ptr m_descText;
    Drawable::Text::Ptr m_hintsText;
    Drawable::Text::Ptr m_startButtonText;
    ScreenRect m_startButtonRect;
};
```

- [ ] **Step 2: Create BriefingScreen implementation**

Create `src/ui/BriefingScreen.cpp`:
```cpp
#include "BriefingScreen.h"
#include "render/IRenderTarget.h"
#include "input/InputEvent.h"

BriefingScreen::BriefingScreen(const std::string &scenarioName,
                               const std::string &description,
                               const std::string &hints) :
    UiScreen("scr_brief.sin"),
    m_scenarioName(scenarioName),
    m_description(description),
    m_hints(hints)
{
}

bool BriefingScreen::init()
{
    if (!UiScreen::init()) {
        return false;
    }

    const Size screenSize = m_renderTarget->getSize();
    const int centerX = screenSize.width / 2;

    m_titleText = m_renderTarget->createText(Drawable::Text::Bold);
    m_titleText->setString(m_scenarioName);
    m_titleText->pointSize = 18;
    m_titleText->color = Drawable::Color(255, 220, 100);
    m_titleText->outlineColor = Drawable::Color(0, 0, 0);
    m_titleText->position = ScreenPos(centerX - 200, 40);

    m_descText = m_renderTarget->createText();
    m_descText->setString(m_description);
    m_descText->pointSize = 12;
    m_descText->color = Drawable::Color(220, 220, 220);
    m_descText->position = ScreenPos(60, 100);

    if (!m_hints.empty()) {
        m_hintsText = m_renderTarget->createText();
        m_hintsText->setString("Hints: " + m_hints);
        m_hintsText->pointSize = 11;
        m_hintsText->color = Drawable::Color(180, 180, 255);
        m_hintsText->position = ScreenPos(60, screenSize.height - 150);
    }

    m_startButtonText = m_renderTarget->createText(Drawable::Text::Bold);
    m_startButtonText->setString("Start Mission");
    m_startButtonText->pointSize = 14;
    m_startButtonText->color = Drawable::Color(255, 255, 255);

    m_startButtonRect = ScreenRect(centerX - 80, screenSize.height - 70, 160, 40);
    m_startButtonText->position = ScreenPos(m_startButtonRect.x + 20, m_startButtonRect.y + 10);

    return true;
}

void BriefingScreen::render()
{
    m_renderTarget->clear(Drawable::Color(20, 15, 10));

    m_renderTarget->draw(m_titleText);
    m_renderTarget->draw(m_descText);

    if (m_hintsText) {
        m_renderTarget->draw(m_hintsText);
    }

    // Draw start button background
    m_renderTarget->draw(m_startButtonRect,
                         Drawable::Color(80, 60, 30),
                         Drawable::Color(200, 180, 100), 2.f);
    m_renderTarget->draw(m_startButtonText);
}

bool BriefingScreen::handleMouseEvent(const input::Event &event)
{
    if (event.type == input::EventType::MouseButtonReleased) {
        if (m_startButtonRect.contains(event.mousePos)) {
            m_startClicked = true;
            return true;
        }
    }
    return false;
}

void BriefingScreen::handleKeyEvent(const input::Event &event)
{
    if (event.type == input::EventType::KeyPressed) {
        if (event.key.code == input::Key::Return || event.key.code == input::Key::Space) {
            m_startClicked = true;
        }
    }
}
```

Note: The exact `UiScreen` constructor, `Drawable::Text` API, `ScreenPos`, input types etc. must match the conventions used in other UiScreen subclasses (e.g., `HomeScreen`, `LobbyScreen`). Adjust field names and method calls based on the actual API found in `src/ui/UiScreen.h` and `src/render/IRenderTarget.h`.

If `scr_brief.sin` doesn't exist, use an empty string or `""` for the UiScreen constructor — the briefing screen can work without a .sin layout file by drawing everything manually.

- [ ] **Step 3: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
Expected: Clean compilation.

- [ ] **Step 4: Commit**

```bash
git add src/ui/BriefingScreen.h src/ui/BriefingScreen.cpp
git commit -m "feat: add BriefingScreen UI for campaign scenarios"
```

---

### Task 2: Integrate briefing into scenario loading flow

**Files:**
- Modify: `src/ui/ScenarioBrowser.cpp` or `src/Engine.cpp` (show briefing before game starts)

- [ ] **Step 1: Find scenario loading flow**

In `ScenarioBrowser.cpp` or the caller that loads a scenario (likely in Engine.cpp or GameState setup), find where the .scx file is loaded and gameplay begins. The scenario file contains:
- `scnFile.scenarioInstructions` — main briefing text
- `scnFile.hints` — hint text
- `scnFile.scenarioHeader.description` — scenario name/description

- [ ] **Step 2: Insert BriefingScreen before game start**

After the scenario file is parsed but before the game loop starts, show the briefing:

```cpp
#include "ui/BriefingScreen.h"

// After loading scnFile...
const std::string instructions = scnFile.scenarioInstructions;
if (!instructions.empty()) {
    BriefingScreen briefing(
        scnFile.scenarioHeader.description,
        instructions,
        scnFile.hints
    );
    briefing.setRenderTarget(m_renderTarget);
    if (!briefing.init()) {
        WARN << "Failed to init briefing screen";
    } else {
        briefing.run(); // blocks until player clicks Start
    }
}
// Continue to game...
```

The exact field names on `scnFile` depend on the genieutils `ScnFile` class. Check `src/extern/genieutils/include/genie/script/ScnFile.h` for the actual member names.

- [ ] **Step 3: Add BriefingScreen to CMakeLists.txt**

In the project's `CMakeLists.txt` (or relevant build file), add the new source files:
```cmake
src/ui/BriefingScreen.cpp
```

- [ ] **Step 4: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
Expected: Clean compilation.

- [ ] **Step 5: Manual test**

1. Launch the game and select a campaign scenario
2. The briefing screen should appear with scenario description text
3. Click "Start Mission" or press Enter/Space to proceed to gameplay
4. If the scenario has no instructions text, the briefing is skipped

- [ ] **Step 6: Commit**

```bash
git add src/ui/BriefingScreen.h src/ui/BriefingScreen.cpp src/Engine.cpp CMakeLists.txt
git commit -m "feat: show campaign briefing screen before scenario missions"
```
