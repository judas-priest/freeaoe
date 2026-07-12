# Mobile Playability — Scenario Browser, Patrol/Guard, Save/Load

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make freeaoe playable on Android: pick campaigns/scenarios, manage armies (patrol/guard), save/load games interrupted by phone calls.

**Architecture:**
1. Scenario browser: reuse existing `FileDialog` component (already works on SDL2) adapted to show campaigns and scenarios from the AoE2 HD data directory.
2. Patrol/Guard: new `ActionPatrol` and `ActionGuard` classes following the existing `IAction` pattern, wired into `ActionPanel` button dispatch and `UnitManager` state machine.
3. Save/Load: serialize `GameState` (map, players, units, resources, triggers) to a binary file using genieutils-style serialization.

**Tech Stack:** C++20, SDL2, genieutils (CpxFile, ScnFile)

**Reference docs:**
- [OpenAge Genie Engine RE](https://simonsan.github.io/openage-webdocs/sphinx/doc/sphinx/handbooks/reverse_engineering.html)
- [AoE2 AI Scripting Encyclopedia](https://airef.github.io/)
- [genieutils](https://github.com/sandsmark/genieutils) — CpxFile, ScnFile already parse campaigns/scenarios

**Existing code to reuse:**
- `FileDialog` (`src/ui/FileDialog.cpp`) — filesystem browser with SDL2 rendering, scrollbar, back/OK buttons
- `AssetManager::campaignsPath()` / `AssetManager_HD::campaignsPath()` — resolves campaign directory
- `genie::CpxFile` — loads .cpx/.cpn campaign files, extracts scenario metadata and ScnFile objects
- `genie::ScnFile` — scenario data (map, players, units, triggers)
- `cpxtool` (`src/extern/genieutils/src/tools/cpxtool/cpxtool.cpp`) — reference implementation for listing campaigns

---

## Part 1: Scenario Browser

### Task 1: Show campaign list on startup (Android)

Currently on Android, `main.cpp` (SDL2 path, line 331-346) either loads a `--scenario-file` from command line or falls through to demo map. There is no UI to choose. We'll add a simple scenario selection flow before `engine.setup()`.

**Files:**
- Create: `src/ui/ScenarioBrowser.h`
- Create: `src/ui/ScenarioBrowser.cpp`
- Modify: `src/main.cpp:331-346`
- Modify: `CMakeLists.txt` (add new source files)

- [ ] **Step 1: Create ScenarioBrowser header**

```cpp
// src/ui/ScenarioBrowser.h
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "render/IRenderTarget.h"
#include "render/EventTypes.h"

namespace genie { class ScnFile; }

struct SdlWindow;

// Simple fullscreen scenario browser for Android
// Shows: campaign list → scenario list → loads selected scenario
class ScenarioBrowser
{
public:
    struct Entry {
        std::string name;
        std::string path;         // full path to .cpx/.cpn file (for campaigns) or .scx/.scn (for standalone)
        int scenarioIndex = -1;   // -1 = this is a campaign entry, >=0 = scenario inside a campaign
        int scenarioCount = 0;    // number of scenarios (for campaign entries)
    };

    ScenarioBrowser(SdlWindow *window, const std::shared_ptr<IRenderTarget> &renderTarget);

    // Scan campaigns directory and populate entries
    void scan(const std::string &campaignsPath);

    // Run the browser event loop. Returns selected scenario or nullptr if cancelled.
    std::shared_ptr<genie::ScnFile> run();

private:
    void render();
    void handleEvent(const input::Event &event);
    void loadCampaign(const Entry &entry);

    SdlWindow *m_window;
    std::shared_ptr<IRenderTarget> m_renderTarget;

    std::vector<Entry> m_entries;          // current list being shown
    std::vector<Entry> m_campaignEntries;  // top-level campaign list (for back button)
    int m_scrollOffset = 0;
    int m_selectedIndex = -1;
    bool m_done = false;
    bool m_inCampaign = false;             // true when showing scenarios inside a campaign
    std::string m_currentCampaignName;

    std::shared_ptr<genie::ScnFile> m_result;

    Drawable::Text::Ptr m_titleText;
    Drawable::Text::Ptr m_itemText;
};
```

- [ ] **Step 2: Implement ScenarioBrowser::scan — find all campaigns**

```cpp
// src/ui/ScenarioBrowser.cpp
#include "ScenarioBrowser.h"

#include <filesystem>
#include <algorithm>
#include <genie/script/ScnFile.h>
#include "render/SdlRenderTarget.h"
#include "core/Logger.h"

namespace fs = std::filesystem;

ScenarioBrowser::ScenarioBrowser(SdlWindow *window, const std::shared_ptr<IRenderTarget> &renderTarget)
    : m_window(window), m_renderTarget(renderTarget)
{
    m_titleText = renderTarget->createText(Drawable::Text::UI);
    m_titleText->pointSize = 24;
    m_titleText->color = Drawable::White;

    m_itemText = renderTarget->createText(Drawable::Text::Plain);
    m_itemText->pointSize = 18;
}

void ScenarioBrowser::scan(const std::string &campaignsPath)
{
    m_entries.clear();

    if (campaignsPath.empty() || !fs::exists(campaignsPath)) {
        DBG << "Campaigns path not found:" << campaignsPath;
        return;
    }

    // Scan for .cpx, .cpn files
    for (const auto &dirEntry : fs::directory_iterator(campaignsPath)) {
        if (!dirEntry.is_regular_file()) continue;
        std::string ext = dirEntry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".cpx" && ext != ".cpn") continue;

        try {
            genie::CpxFile cpx;
            cpx.load(dirEntry.path().string());
            Entry e;
            e.name = cpx.name;
            // Clean up null-terminated string
            e.name.erase(std::find(e.name.begin(), e.name.end(), '\0'), e.name.end());
            if (e.name.empty()) e.name = dirEntry.path().stem().string();
            e.path = dirEntry.path().string();
            e.scenarioCount = cpx.getFilecount();
            m_entries.push_back(std::move(e));
        } catch (const std::exception &ex) {
            WARN << "Failed to load campaign" << dirEntry.path() << ex.what();
        }
    }

    // Also scan for standalone .scx/.scn files
    for (const auto &dirEntry : fs::directory_iterator(campaignsPath)) {
        if (!dirEntry.is_regular_file()) continue;
        std::string ext = dirEntry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".scx" && ext != ".scn") continue;

        Entry e;
        e.name = dirEntry.path().stem().string();
        e.path = dirEntry.path().string();
        e.scenarioIndex = 0; // standalone scenario
        m_entries.push_back(std::move(e));
    }

    // Sort alphabetically
    std::sort(m_entries.begin(), m_entries.end(),
        [](const Entry &a, const Entry &b) { return a.name < b.name; });

    m_campaignEntries = m_entries;
}
```

- [ ] **Step 3: Implement ScenarioBrowser::loadCampaign — list scenarios in a campaign**

```cpp
void ScenarioBrowser::loadCampaign(const Entry &entry)
{
    m_entries.clear();
    m_inCampaign = true;
    m_currentCampaignName = entry.name;
    m_scrollOffset = 0;

    try {
        genie::CpxFile cpx;
        cpx.load(entry.path);
        auto filenames = cpx.getFilenames();
        for (size_t i = 0; i < filenames.size(); i++) {
            Entry e;
            e.name = std::to_string(i + 1) + ". " + filenames[i];
            // Remove .scx/.scn extension from display name
            auto dotPos = e.name.rfind('.');
            if (dotPos != std::string::npos) e.name = e.name.substr(0, dotPos);
            e.path = entry.path;
            e.scenarioIndex = static_cast<int>(i);
            m_entries.push_back(std::move(e));
        }
    } catch (const std::exception &ex) {
        WARN << "Failed to open campaign" << entry.path << ex.what();
        // Go back to campaign list
        m_entries = m_campaignEntries;
        m_inCampaign = false;
    }
}
```

- [ ] **Step 4: Implement render — draw list of entries**

```cpp
void ScenarioBrowser::render()
{
    m_renderTarget->clear(Drawable::Color(20, 15, 10, 255));

    Size screenSize = m_renderTarget->getSize();
    float y = 20;

    // Title
    m_titleText->string = m_inCampaign ? m_currentCampaignName : "Select Campaign";
    m_titleText->position = ScreenPos(20, y);
    m_renderTarget->draw(m_titleText);
    y += 40;

    // Back button hint
    if (m_inCampaign) {
        m_itemText->string = "< Back";
        m_itemText->color = Drawable::Color(150, 150, 150, 255);
        m_itemText->position = ScreenPos(20, y);
        m_renderTarget->draw(m_itemText);
        y += 30;
    }

    // Separator line
    m_renderTarget->draw(ScreenRect(20, y, screenSize.width - 40, 1),
                         Drawable::Color(100, 80, 60, 255));
    y += 10;

    // List entries
    const float itemHeight = 35;
    const int visibleCount = static_cast<int>((screenSize.height - y - 20) / itemHeight);

    for (int i = m_scrollOffset; i < static_cast<int>(m_entries.size()) && i < m_scrollOffset + visibleCount; i++) {
        bool isSelected = (i == m_selectedIndex);
        if (isSelected) {
            m_renderTarget->draw(ScreenRect(15, y - 2, screenSize.width - 30, itemHeight),
                                 Drawable::Color(60, 40, 20, 255));
        }

        const Entry &entry = m_entries[i];
        m_itemText->color = isSelected ? Drawable::Color(255, 220, 150, 255) : Drawable::White;

        std::string label = entry.name;
        if (entry.scenarioIndex == -1) {
            label += " (" + std::to_string(entry.scenarioCount) + " scenarios)";
        }
        m_itemText->string = label;
        m_itemText->position = ScreenPos(30, y);
        m_renderTarget->draw(m_itemText);

        y += itemHeight;
    }
}
```

- [ ] **Step 5: Implement handleEvent — touch to select/scroll**

```cpp
void ScenarioBrowser::handleEvent(const input::Event &event)
{
    if (event.type == input::Event::TouchEnded || event.type == input::Event::MouseButtonReleased) {
        float x, y;
        if (event.type == input::Event::TouchEnded) {
            x = event.touch.x;
            y = event.touch.y;
        } else {
            x = event.mouseButton.x;
            y = event.mouseButton.y;
        }

        float listStartY = m_inCampaign ? 100 : 70;

        // Back button
        if (m_inCampaign && y < listStartY && y > 50) {
            m_entries = m_campaignEntries;
            m_inCampaign = false;
            m_scrollOffset = 0;
            return;
        }

        // List item tap
        if (y >= listStartY) {
            int index = m_scrollOffset + static_cast<int>((y - listStartY) / 35);
            if (index >= 0 && index < static_cast<int>(m_entries.size())) {
                const Entry &entry = m_entries[index];
                if (entry.scenarioIndex == -1) {
                    // Campaign — drill into scenarios
                    loadCampaign(entry);
                } else if (entry.scenarioIndex >= 0) {
                    // Scenario — load it
                    try {
                        if (entry.path.find(".cpx") != std::string::npos ||
                            entry.path.find(".cpn") != std::string::npos) {
                            genie::CpxFile cpx;
                            cpx.load(entry.path);
                            m_result = cpx.getScnFile(entry.scenarioIndex);
                        } else {
                            m_result = std::make_shared<genie::ScnFile>();
                            m_result->load(entry.path);
                        }
                        m_done = true;
                    } catch (const std::exception &ex) {
                        WARN << "Failed to load scenario" << entry.path << ex.what();
                    }
                }
            }
        }
    }

    // Scroll with touch drag
    if (event.type == input::Event::TouchMoved) {
        // Simple scroll: if moved up, scroll down
        static float lastY = 0;
        float dy = event.touch.y - lastY;
        lastY = event.touch.y;
        if (std::abs(dy) > 5) {
            if (dy < 0 && m_scrollOffset < static_cast<int>(m_entries.size()) - 5) m_scrollOffset++;
            if (dy > 0 && m_scrollOffset > 0) m_scrollOffset--;
        }
    }
    if (event.type == input::Event::TouchBegan) {
        // Reset drag tracking — store initial Y for scroll
    }
}
```

- [ ] **Step 6: Implement run — event loop**

```cpp
std::shared_ptr<genie::ScnFile> ScenarioBrowser::run()
{
    if (m_entries.empty()) return nullptr;

    while (!m_done) {
        input::Event event;
        while (m_renderTarget->pollEvent(event, m_window->sdlWindow)) {
            if (event.type == input::Event::Closed) {
                return nullptr;
            }
            handleEvent(event);
        }

        render();
        m_window->display();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return m_result;
}
```

**Event loop pattern** — copied from `FileDialog::getPath()` (FileDialog.cpp:141-179):
```cpp
while (m_window->isOpen()) {
    input::Event event;
    while (m_window->pollEvent(event)) {  // SdlWindow::pollEvent, NOT renderTarget
        handleEvent(event);
    }
    render();
    m_window->display();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}
```
ScenarioBrowser takes `SdlWindow*` (not Engine), same as FileDialog.

- [ ] **Step 7: Wire ScenarioBrowser into main.cpp**

In `src/main.cpp`, replace the SDL2 path (lines 331-346 approximately) where it currently hardcodes demo map:

```cpp
#ifdef USE_SDL2
    if (config.isOptionSet(Config::ScenarioFile)) {
        scenarioFile = std::make_shared<genie::ScnFile>();
        scenarioFile->load(config.getValue(Config::ScenarioFile));
    }
#ifdef ANDROID
    if (!scenarioFile) {
        // Show scenario browser
        auto *sdlRT = static_cast<SdlRenderTarget*>(engine.renderTarget().get());
        ScenarioBrowser browser(engine.sdlWindow(), engine.renderTarget());
        std::string camPath = AssetManager::Inst()->campaignsPath();
        browser.scan(camPath);
        scenarioFile = browser.run();
        if (!scenarioFile) {
            // User cancelled or no campaigns found — fall back to demo map
            config.setValue(Config::SinglePlayer, "1");
        }
    }
#endif
    if (!scenarioFile && !config.isOptionSet(Config::SinglePlayer) && !config.isOptionSet(Config::GameSample)) {
        config.setValue(Config::SinglePlayer, "1");
    }
#endif
```

This requires making `renderTarget()` and `sdlWindow()` accessible on Engine, or running the browser before engine setup. Check `FileDialog` usage pattern in main.cpp (around line 310) — it runs before engine setup using the same window.

Actually, looking at the existing code flow: `Engine::setup()` creates the window (line 914), then FileDialog can run. The ScenarioBrowser should run after `Engine::setup()` but before `Engine::start()`:

```cpp
    Engine engine;
    if (!engine.setup(nullptr)) { // setup without scenario first
        return 1;
    }

#ifdef ANDROID
    if (!scenarioFile) {
        ScenarioBrowser browser(engine.m_sdlWindow.get(), engine.renderTarget_);
        browser.scan(AssetManager::Inst()->campaignsPath());
        scenarioFile = browser.run();
    }
#endif

    // Now setup game state with selected scenario
    // ... may need to restructure Engine::setup to allow late scenario binding
```

The exact integration point depends on how Engine exposes its window/renderTarget. Check FileDialog usage for the pattern. This step may need adaptation based on Engine's public API.

- [ ] **Step 8: Add to CMakeLists.txt**

Add `src/ui/ScenarioBrowser.cpp` and `src/ui/ScenarioBrowser.h` to the source file list in CMakeLists.txt alongside other UI files.

- [ ] **Step 9: Build and test**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
unset LD_PRELOAD && /home/dima/.local/opt/android-sdk/platform-tools/adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Test: app should show campaign list → tap campaign → scenario list → tap scenario → game starts with that scenario.

- [ ] **Step 10: Commit**

```bash
git add src/ui/ScenarioBrowser.h src/ui/ScenarioBrowser.cpp src/main.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: add scenario browser for Android — pick campaign and mission

Scans campaigns directory for .cpx/.cpn files, shows list with
scenario count. Tap campaign → shows scenarios inside. Tap scenario
→ loads and starts the game. Falls back to demo map if no campaigns
found or user cancels.
EOF
)"
```

---

## Part 2: Patrol & Guard Commands

### Task 2: Implement ActionPatrol

Patrol: unit moves to destination, then back to start, repeating. Attacks enemies on sight. Based on existing ActionMove pattern.

**Files:**
- Create: `src/actions/ActionPatrol.h`
- Create: `src/actions/ActionPatrol.cpp`
- Modify: `src/actions/IAction.h` (add Type::Patrol)
- Modify: `src/ui/ActionPanel.cpp` (wire button)
- Modify: `src/mechanics/UnitManager.h` (add SelectingPatrolTarget state)
- Modify: `src/mechanics/UnitManager.cpp` (handle patrol target selection)
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add Patrol type to IAction::Type enum**

In `src/actions/IAction.h`, add to the `Type` enum (around line 63-73):

```cpp
enum Type {
    Move,
    Build,
    Gather,
    Attack,
    Fly,
    Garrison,
    Patrol,  // NEW
    Guard,   // NEW
};
```

- [ ] **Step 2: Create ActionPatrol**

```cpp
// src/actions/ActionPatrol.h
#pragma once

#include "IAction.h"
#include "core/Types.h"
#include "mechanics/Unit.h"
#include <memory>

class ActionMove;

class ActionPatrol : public IAction
{
public:
    ActionPatrol(const Unit::Ptr &unit, const MapPos &destination);

    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::MoveTo; }
    UnitState unitState() const override;

private:
    MapPos m_startPos;
    MapPos m_destPos;
    bool m_returning = false;
    std::shared_ptr<ActionMove> m_currentMove;

    void startNextLeg();
};
```

```cpp
// src/actions/ActionPatrol.cpp
#include "ActionPatrol.h"
#include "ActionMove.h"

ActionPatrol::ActionPatrol(const Unit::Ptr &unit, const MapPos &destination)
    : IAction(Type::Patrol, unit, Task())
    , m_destPos(destination)
{
    Unit::Ptr u = m_unit.lock();
    if (u) {
        m_startPos = u->position();
    }
}

ActionPatrol::UpdateResult ActionPatrol::update(Time time)
{
    (void)time;
    Unit::Ptr unit = m_unit.lock();
    if (!unit) return Completed;

    // Check if we've arrived at current target (or close enough)
    MapPos target = m_returning ? m_startPos : m_destPos;
    float dist = unit->position().distanceTo(target);

    if (dist < 5.f) {
        // Reached waypoint — reverse direction
        m_returning = !m_returning;
        target = m_returning ? m_startPos : m_destPos;
    }

    // Prepend a move action. ActionMove will complete, then control
    // returns to this patrol action (it's the base of the action queue).
    // Auto-attack (from UnitActionHandler) will also prepend attacks,
    // which complete and return to patrol naturally.
    unit->actions.queueAction(ActionMove::moveUnitTo(unit, target));
    return Updated;
}
```

**Key design**: ActionPatrol is the **base action** — it stays as `currentAction`. It queues ActionMove via `queueAction`. When move completes, `UnitActionHandler` returns to patrol. If enemies appear, auto-attack prepends ActionAttack, which completes and returns to the queued move, then to patrol.

**IMPORTANT**: `ActionMove::moveUnitTo(unit, destination)` is a confirmed static factory (ActionMove.h:57). ActionMove constructor is private — must use factory.

- [ ] **Step 3: Wire Patrol button in ActionPanel**

In `src/ui/ActionPanel.cpp`, `handleButtonClick()` (around line 666), add before the `default:` case:

```cpp
case Command::Patrol:
    m_unitManager->selectPatrolTarget();
    break;
```

- [ ] **Step 4: Add SelectingPatrolTarget state to UnitManager**

In `src/mechanics/UnitManager.h`, add to the `State` enum:

```cpp
enum State {
    Default,
    PlacingBuilding,
    PlacingWall,
    SelectingAttackTarget,
    SelectingGarrisonTarget,
    SelectingPatrolTarget,   // NEW
    SelectingGuardTarget,    // NEW
};
```

Add method declaration:

```cpp
void selectPatrolTarget();
void selectGuardTarget();
```

- [ ] **Step 5: Handle patrol target selection in UnitManager**

In `src/mechanics/UnitManager.cpp`, add `selectPatrolTarget()`:

```cpp
void UnitManager::selectPatrolTarget()
{
    m_state = State::SelectingPatrolTarget;
}
```

In `onLeftClick()`, add a case for `SelectingPatrolTarget` (inside the switch on m_state):

```cpp
case State::SelectingPatrolTarget: {
    MapPos targetPos = camera->absoluteMapPos(screenPos);
    for (const Unit::Ptr &unit : m_selectedUnits) {
        if (unit->playerId() != humanPlayer->playerId) continue;
        auto patrol = std::make_shared<ActionPatrol>(unit, targetPos);
        unit->actions.setCurrentAction(patrol);
    }
    break;
}
```

- [ ] **Step 6: Build and test**

```bash
cd /home/dima/Projects/freeaoe/android && unset LD_PRELOAD && ./gradlew assembleDebug
```

Test: select military unit → tap Patrol button → tap map location → unit should walk back and forth.

- [ ] **Step 7: Commit**

```bash
git add src/actions/ActionPatrol.h src/actions/ActionPatrol.cpp src/actions/IAction.h src/ui/ActionPanel.cpp src/mechanics/UnitManager.h src/mechanics/UnitManager.cpp CMakeLists.txt
git commit -m "feat: implement Patrol command — units walk back and forth between positions"
```

---

### Task 3: Implement ActionGuard

Guard: unit follows a target and attacks anything that attacks the target. Uses `genie::ActionType::Guard = 13`.

**Files:**
- Create: `src/actions/ActionGuard.h`
- Create: `src/actions/ActionGuard.cpp`
- Modify: `src/ui/ActionPanel.cpp`
- Modify: `src/mechanics/UnitManager.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create ActionGuard**

```cpp
// src/actions/ActionGuard.h
#pragma once

#include "IAction.h"
#include "core/Types.h"
#include "mechanics/Unit.h"

class ActionGuard : public IAction
{
public:
    ActionGuard(const Unit::Ptr &unit, const Unit::Ptr &target);

    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Guard; }

private:
    std::weak_ptr<Unit> m_guardTarget;
    static constexpr float FOLLOW_DISTANCE = 48.f; // ~1.5 tiles
};
```

```cpp
// src/actions/ActionGuard.cpp
#include "ActionGuard.h"
#include "ActionMove.h"

ActionGuard::ActionGuard(const Unit::Ptr &unit, const Unit::Ptr &target)
    : IAction(Type::Guard, unit, Task())
    , m_guardTarget(target)
{
}

ActionGuard::UpdateResult ActionGuard::update(Time time)
{
    (void)time;
    Unit::Ptr unit = m_unit.lock();
    Unit::Ptr target = m_guardTarget.lock();
    if (!unit || !target) return Completed;

    float dist = unit->position().distanceTo(target->position());

    // Follow if too far — but only queue move if not already moving
    if (dist > FOLLOW_DISTANCE && !m_isFollowing) {
        m_isFollowing = true;
        unit->actions.queueAction(ActionMove::moveUnitTo(unit, target->position()));
        return Updated;
    }

    // Reset follow flag when close enough
    if (dist <= FOLLOW_DISTANCE) {
        m_isFollowing = false;
    }

    // Auto-attack is handled by UnitActionHandler's existing auto-target logic
    return NotUpdated;
}
```

Add `bool m_isFollowing = false;` to ActionGuard header's private section. This prevents queueing move every frame when far away.

`ActionMove::moveUnitTo` returns `std::shared_ptr<ActionMove>` which inherits `IAction`, so `queueAction(ActionPtr)` accepts it.

- [ ] **Step 2: Wire Guard button in ActionPanel**

In `src/ui/ActionPanel.cpp`, `handleButtonClick()`:

```cpp
case Command::Guard:
    m_unitManager->selectGuardTarget();
    break;
```

- [ ] **Step 3: Handle guard target selection in UnitManager**

Add `selectGuardTarget()`:

```cpp
void UnitManager::selectGuardTarget()
{
    m_state = State::SelectingGuardTarget;
}
```

In `onLeftClick()`, the `case State::SelectingGuardTarget:` already exists (lines 401-436) but for garrison. Add a new case:

```cpp
case State::SelectingGuardTarget: {
    Unit::Ptr targetUnit = unitAt(screenPos, camera, NoAlignment);
    if (!targetUnit) {
        WARN << "No unit at guard target position";
        break;
    }
    for (const Unit::Ptr &unit : m_selectedUnits) {
        if (unit->playerId() != humanPlayer->playerId) continue;
        if (unit == targetUnit) continue; // don't guard yourself
        auto guard = std::make_shared<ActionGuard>(unit, targetUnit);
        unit->actions.setCurrentAction(guard);
    }
    break;
}
```

- [ ] **Step 4: Build and test**

Test: select military unit → tap Guard button → tap allied unit → guarding unit follows the target.

- [ ] **Step 5: Commit**

```bash
git add src/actions/ActionGuard.h src/actions/ActionGuard.cpp src/ui/ActionPanel.cpp src/mechanics/UnitManager.cpp CMakeLists.txt
git commit -m "feat: implement Guard command — unit follows and protects target"
```

---

## Part 3: Save/Load

### Task 4: Save game state to file

This is a large feature. Minimal viable approach: serialize enough state to resume a game — player resources, unit positions/HP, building state, research completed. Skip: trigger state (complex), AI state (not working anyway).

**Files:**
- Create: `src/mechanics/SaveGame.h`
- Create: `src/mechanics/SaveGame.cpp`
- Modify: `src/ui/Dialog.cpp` (wire Save button)
- Modify: `src/Engine.cpp` (wire Save/Load)
- Modify: `CMakeLists.txt`

This task is a research spike — the implementation details depend heavily on how GameState, Map, Player, and Unit serialize. The plan here provides the architecture; exact field-by-field serialization needs to be discovered from the data structures.

- [ ] **Step 1: Design save file format**

```cpp
// src/mechanics/SaveGame.h
#pragma once

#include <string>
#include <memory>

class GameState;
namespace genie { class ScnFile; }

struct SaveGame
{
    // Save current game state to a file
    static bool save(const std::string &path, const GameState &state);

    // Load a save file, returns a ScnFile-like object that Engine::setup can use
    // (or directly restores GameState)
    static bool load(const std::string &path, GameState &state);

    // List save files in the saves directory
    static std::vector<std::string> listSaves(const std::string &savesDir);
};
```

- [ ] **Step 2: Implement minimal save — player resources + unit positions**

This requires reading through `GameState`, `Player`, `Unit`, `Map` to identify what needs to be serialized. Use a simple binary format:

```
[4 bytes] magic "FAOE"
[4 bytes] version (1)
[4 bytes] map width
[4 bytes] map height
[map tiles data]
[4 bytes] player count
[per player: resources, diplomacy, researched techs]
[4 bytes] unit count
[per unit: type ID, player ID, position x/y, HP, carried resources, action state]
```

Full implementation depends on exact struct layouts — needs discovery. Start with player resources and unit positions, iterate from there.

- [ ] **Step 3: Wire Save button in Dialog**

In `src/ui/Dialog.cpp`, change the save button text and handle it in `Engine::handleEvent`:

```cpp
// In Dialog.cpp constructor:
m_buttons[Save].text = "Save";  // remove "(TODO)"

// In Engine::handleEvent, after the dialog choice check:
} else if (choice == Dialog::Save) {
    std::string savePath = SDL_AndroidGetExternalStoragePath()
        ? std::string(SDL_AndroidGetExternalStoragePath()) + "/save.faoe"
        : "save.faoe";
    SaveGame::save(savePath, *state);
    addMessage("Game saved");
    m_currentDialog.reset();
}
```

- [ ] **Step 4: Build and test**

- [ ] **Step 5: Commit**

```bash
git add src/mechanics/SaveGame.h src/mechanics/SaveGame.cpp src/ui/Dialog.cpp src/Engine.cpp CMakeLists.txt
git commit -m "feat: implement basic save/load game"
```

---

## Summary

| Task | Feature | Complexity | Priority |
|------|---------|-----------|----------|
| 1 | Scenario browser — pick campaign → play | Medium (new UI, reuse FileDialog patterns) | P0 — without this, can't choose what to play |
| 2 | Patrol command | Low (new action class, wire 3 files) | P1 — army management |
| 3 | Guard command | Low (new action class, wire 3 files) | P1 — army management |
| 4 | Save/Load | High (serialization of full game state) | P2 — needed but can start with minimal version |

Tasks 2 and 3 are independent of each other and of Task 1. Task 4 depends on understanding GameState internals deeply.
