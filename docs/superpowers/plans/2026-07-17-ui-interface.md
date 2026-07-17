# UI & Interface Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete UI -- chat with taunts, save/load dialogs, post-game statistics screen, game speed control UI, Go-to-TC hotkey polish, hotkey remapping, minimap mode toggle button, and scenario editor stub.

**Architecture:** Engine is the main game loop (`/home/dima/Projects/freeaoe/src/Engine.h`/`.cpp`). It owns all UI components as `std::unique_ptr` members: `m_actionPanel`, `m_minimap`, `m_unitInfoPanel`, `m_diplomacyScreen`, `m_settingsScreen`, `m_techTreeScreen`, and a `m_currentDialog` for the in-game menu. Overlay screens (SettingsScreen, TechTreeScreen, DiplomacyScreen) follow a pattern: they have `show()`/`hide()`/`isVisible()`/`render()`/`handleEvent()` methods. They render a full-screen semi-transparent overlay + a centered panel. The `Dialog` struct is the in-game menu with choices (Quit/Stats/Save/Options/About/Cancel). Event flow: `Engine::handleEvent()` dispatches to overlay screens first, then `handleKeyEvent()` for hotkeys. Chat input uses `m_chat.active` flag + `m_chat.buffer` string; Enter toggles chat mode, text is captured via `TextEntered` events. Minimap already has `MinimapMode::Normal/Economic/Diplomatic` with full rendering logic and `cycleMode()`. SaveGame class (`/home/dima/Projects/freeaoe/src/mechanics/SaveGame.h`) has `save()`/`load()`/`listSaves()` -- currently F5/F9 do quick save/load only.

**Tech Stack:** C++20, SDL2

**Build & test:** `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)` then run the binary.

---

## Task 1: Chat taunt numbers (1-42 standard AoE2 taunts)

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
In `Engine::onChatMessage()`, after the cheat code check, detect if the message is a numeric string 1-42. If so, look up the standard AoE2 taunt text and display it instead of the raw number. Also play the corresponding taunt audio file (`taunt/tauntNN.mp3`).

Add a static array of the 42 standard AoE2 taunt strings:

```cpp
static const char *s_taunts[] = {
    nullptr,                       // 0 unused
    "Yes",                         // 1
    "No",                          // 2
    "I need food",                 // 3
    "I need wood",                 // 4
    "I need gold",                 // 5
    "I need stone",                // 6
    "Ahh!",                        // 7
    "All hail, king of the losers!",// 8
    "Ooh!",                        // 9
    "I'll beat you back to Age of Empires",// 10
    "Nice town, I'll take it",     // 11
    "Raiding party!",              // 12
    "Blame your isp",              // 13
    "Start the game already!",     // 14
    "Don't point that thing at me!",// 15
    "Enemy sighted!",              // 16
    "It is good to be the king",   // 17
    "Monk! I need a monk!",        // 18
    "Long time, no siege",         // 19
    "My granny could scrap better than that",// 20
    "Nice town, I'll take it",     // 21
    "Attack an enemy now",         // 22
    "Cease creating extra villagers",// 23
    "Create extra villagers",      // 24
    "Build a navy",                // 25
    "Stop buying food",            // 26
    "Buy food",                    // 27
    "Stop buying wood",            // 28
    "Buy wood",                    // 29
    "Stop buying gold",            // 30
    "Buy gold",                    // 31
    "Stop buying stone",           // 32
    "Buy stone",                   // 33
    "Ally",                        // 34
    "Enemy",                       // 35
    "Neutral",                     // 36
    "What age are you in?",        // 37
    "What is your strategy?",      // 38
    "How many resources?",         // 39
    "We are under attack!",        // 40
    "Flare!",                      // 41
    "I resign"                     // 42
};
```

In `onChatMessage()`, after the `"marco"` check:

```cpp
// Check for taunt number
try {
    int tauntNum = std::stoi(message);
    if (tauntNum >= 1 && tauntNum <= 42) {
        std::string display = "Player " + std::to_string(sourcePlayer) + ": " + s_taunts[tauntNum];
        addMessage(display);
        std::string tauntFile = "taunt/taunt" + std::to_string(tauntNum) + ".mp3";
        AudioPlayer::instance().playStream(tauntFile);
        return;
    }
} catch (...) {}
```

**Commit message:** `feat: chat taunts — numeric messages 1-42 display taunt text and play audio`

---

## Task 2: Game speed +/- hotkeys (F7/F8)

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
In `Engine::handleKeyEvent()`, add cases for F7 (slow down) and F8 (speed up) between the existing F6 and F10 cases. Use 0.5x increments, clamp to 0.5 - 3.0.

Add after the `case input::Key::F6:` block (around line 1136):

```cpp
case input::Key::F7:
    m_gameSpeed = std::max(0.5f, m_gameSpeed - 0.5f);
    addMessage("Game speed: " + std::to_string(m_gameSpeed).substr(0, 3) + "x");
    return true;
case input::Key::F8:
    m_gameSpeed = std::min(3.0f, m_gameSpeed + 0.5f);
    addMessage("Game speed: " + std::to_string(m_gameSpeed).substr(0, 3) + "x");
    return true;
```

**Commit message:** `feat: game speed hotkeys — F7 slower, F8 faster (0.5x-3.0x)`

---

## Task 3: Sync SettingsScreen game speed with Engine

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/ui/SettingsScreen.h`
- `/home/dima/Projects/freeaoe/src/ui/SettingsScreen.cpp`
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
Currently `SettingsScreen::m_gameSpeed` is local and never read back by Engine. Add a pointer/reference so the settings screen reads and writes the Engine's `m_gameSpeed`.

In `SettingsScreen.h`, add:
```cpp
float *m_engineGameSpeed = nullptr;
public:
void setGameSpeedPtr(float *ptr) { m_engineGameSpeed = ptr; }
```

In `SettingsScreen::show()`, read from Engine speed:
```cpp
if (m_engineGameSpeed) m_gameSpeed = *m_engineGameSpeed;
```

In `SettingsScreen::hide()`, write back:
```cpp
if (m_engineGameSpeed) *m_engineGameSpeed = m_gameSpeed;
```

In `Engine.cpp`, where `m_settingsScreen` is created (search for `new SettingsScreen`), add:
```cpp
m_settingsScreen->setGameSpeedPtr(&m_gameSpeed);
```

**Commit message:** `fix: settings screen reads/writes engine game speed`

---

## Task 4: Save/Load UI dialog with file list

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/Engine.h`
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
When `Dialog::Save` is chosen, instead of immediately saving, show a save dialog overlay. Create a new inner screen class `SaveLoadScreen` following the SettingsScreen pattern (show/hide/isVisible/render/handleEvent). It lists existing save files from `SaveGame::listSaves()` and has a "New Save" button plus a "Load" tab.

Add to `Engine.h`:
```cpp
struct SaveLoadScreen {
    bool visible = false;
    bool loadMode = false; // false=save, true=load
    std::vector<std::string> files;
    int selectedIndex = -1;
    std::string saveName;
    Drawable::Text::Ptr titleText;
    Drawable::Text::Ptr itemText;

    void show(bool isLoad, const std::shared_ptr<IRenderTarget> &rt);
    void hide() { visible = false; }
    void render(const std::shared_ptr<IRenderTarget> &rt);
    bool handleEvent(const input::Event &event, const std::shared_ptr<IRenderTarget> &rt);
};
std::unique_ptr<SaveLoadScreen> m_saveLoadScreen;
```

In `Engine.cpp`, implement `SaveLoadScreen::show()`:
- Set `loadMode`, `visible = true`
- Call `SaveGame::listSaves()` for the save directory
- Create text drawables if null

Implement `SaveLoadScreen::render()`:
- Draw semi-transparent overlay
- Draw panel with title "Save Game" or "Load Game"
- List files with highlight on `selectedIndex`
- "New Save" button (save mode only), "OK" button, "Cancel" button

Implement `SaveLoadScreen::handleEvent()`:
- Click on file item -> set `selectedIndex`
- Click OK -> return the selected path
- Click Cancel -> hide

Wire into Engine event loop:
- In `handleEvent()`, before `m_currentDialog` check, check `m_saveLoadScreen->visible`
- On `Dialog::Save`, call `m_saveLoadScreen->show(false, renderTarget_)` instead of direct save
- Add `Dialog::Achievements` handler to show it in load mode
- In `drawUi()`, call `m_saveLoadScreen->render()` after dialog render

Save directory:
- Android: `SDL_AndroidGetExternalStoragePath() + "/saves/"`
- Desktop: `~/.local/share/freeaoe/saves/`

**Commit message:** `feat: save/load UI dialog with file list`

---

## Task 5: Post-game statistics screen

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/Engine.h`
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
When the game ends (result != Running) and the result overlay is shown, add a "Statistics" button next to "Return to Menu" and "Continue Playing". Also, make the `Dialog::Achievements` button show this screen.

Create a `StatsScreen` struct in Engine.h following the overlay pattern:
```cpp
struct StatsScreen {
    bool visible = false;
    Drawable::Text::Ptr titleText;
    Drawable::Text::Ptr bodyText;

    void show(const std::shared_ptr<GameState> &state, const std::shared_ptr<IRenderTarget> &rt);
    void hide() { visible = false; }
    void render(const std::shared_ptr<IRenderTarget> &rt);
    bool handleEvent(const input::Event &event, const std::shared_ptr<IRenderTarget> &rt);
};
std::unique_ptr<StatsScreen> m_statsScreen;
```

In `StatsScreen::show()`:
- Iterate `state->players()`, for each:
  - Get `player->name`, `player->score()`, `player->unitsKilled`, `player->unitsLost`, `player->buildingsRazed`, `player->techsResearched`, `player->totalResourcesGathered`
  - Format as a table

In `StatsScreen::render()`:
- Draw overlay + panel
- Title: "Post-Game Statistics"
- Table: columns for Player, Score, Kills, Losses, Buildings Razed, Techs, Resources
- Close/OK button

Wire into Engine:
- `Dialog::Achievements` -> `m_statsScreen->show(...)`
- In end-game result overlay code (around line 857 in drawUi), add a "Statistics" TextButton
- In handleEvent, check `m_statsScreen->visible` before other checks

**Commit message:** `feat: post-game statistics screen with player scores`

---

## Task 6: Go-to-TC hotkey polish (H key cycles TCs, selects TC)

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
Currently H key just centers on the first TC found. Improve it to:
1. Also select the TC
2. Cycle through multiple TCs on repeated H presses
3. Check all TC unit IDs (109=TC, 71=TC Foundation, 141=TC Castle Age, 142=TC Imperial)

Replace the existing `case input::Key::H:` block:

```cpp
case input::Key::H: {
    const Player::Ptr &human = state->humanPlayer();
    if (human) {
        static int lastTcIdx = -1;
        std::vector<Unit::Ptr> tcs;
        for (const Unit::Ptr &unit : state->unitManager()->units()) {
            if (unit->playerId() == human->playerId &&
                (unit->data()->ID == 109 || unit->data()->ID == 71 ||
                 unit->data()->ID == 141 || unit->data()->ID == 142)) {
                tcs.push_back(unit);
            }
        }
        if (!tcs.empty()) {
            lastTcIdx = (lastTcIdx + 1) % static_cast<int>(tcs.size());
            renderTarget_->camera()->setTargetPosition(tcs[lastTcIdx]->position());
            state->unitManager()->setSelectedUnits({tcs[lastTcIdx]});
        }
    }
    return true;
}
```

**Commit message:** `feat: H key cycles through TCs and selects them`

---

## Task 7: Minimap mode toggle button

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
Add a small clickable label/button near the minimap that shows the current mode and cycles on click. The minimap already has `cycleMode()` and F4 hotkey. Add a visual indicator.

In `Engine::drawUi()`, after the minimap draw call, render a small text label showing the current mode:

```cpp
if (m_minimap) {
    const char *modeNames[] = {"Diplo", "Econ", "Normal"};
    int modeIdx = static_cast<int>(m_minimap->mode());
    ScreenRect mmRect = m_minimap->rect();
    if (!m_menuItemText) m_menuItemText = renderTarget_->createText(Drawable::Text::Plain);
    m_menuItemText->string = modeNames[modeIdx];
    m_menuItemText->pointSize = 10;
    m_menuItemText->color = Drawable::Color(200, 200, 160, 220);
    m_menuItemText->position = ScreenPos(mmRect.x + 2, mmRect.y - 14);
    renderTarget_->draw(m_menuItemText);
}
```

In `handleMousePress()` and the touch handler, add a hit test for the mode label area above the minimap:

```cpp
ScreenRect modeLabelRect(m_minimap->rect().x, m_minimap->rect().y - 16, 50, 16);
if (modeLabelRect.contains(mousePos)) {
    m_minimap->cycleMode();
    return true;
}
```

**Commit message:** `feat: clickable minimap mode label (Diplo/Econ/Normal)`

---

## Task 8: Hotkey display in chat help

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
Add an F2 hotkey that shows a help overlay listing all hotkeys. This is simpler than a full remapping system and more useful.

In `handleKeyEvent()`, add:

```cpp
case input::Key::F2: {
    static bool showHelp = false;
    showHelp = !showHelp;
    if (showHelp) {
        addMessage("=== HOTKEYS ===");
        addMessage("Enter: Chat | Esc: Menu | F1: Idle Villager");
        addMessage("F2: This help | F3: Pause | F4: Minimap mode");
        addMessage("F5: Quick Save | F9: Quick Load");
        addMessage("F6: Follow unit | F7: Slower | F8: Faster");
        addMessage("F11: Objectives | H: Go to TC");
        addMessage("S: Stop | A: Attack-move | Del: Kill unit");
        addMessage("Ctrl+1-9: Set group | 1-9: Recall group");
    }
    return true;
}
```

**Commit message:** `feat: F2 hotkey help overlay`

---

## Task 9: Stub Scenario Editor button

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/ui/HomeScreen.cpp`

**What to do:**
The HomeScreen has a `MapEditor` button (Button::Type::MapEditor = 4). When clicked, show a message that the scenario editor is not yet implemented.

Find where button clicks are handled in HomeScreen. If clicking MapEditor does nothing, add handling so it shows a TODO message. Look for `getSelection()` usage in Engine/startup code and add:

```cpp
case HomeScreen::Button::MapEditor:
    // Scenario editor is a future milestone
    // Just show a message for now
    break;
```

If the button already triggers `getSelection()` to return `MapEditor`, then in Engine's start screen handling, detect that value and call `addMessage("Scenario Editor: Coming soon!")` or display a small dialog.

**Commit message:** `feat: stub scenario editor button with 'coming soon' message`

---

## Task 10: Chat message targeting (All / Allies / Enemy dropdown)

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/Engine.h`
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
Extend the chat input bar to show a target selector. Currently chat always sends to `targetPlayer = -1` (all). Add a `m_chatTarget` field and cycle through targets with Tab while chat is active.

In `Engine.h`, add to `ChatState`:
```cpp
struct ChatState {
    bool active = false;
    std::string buffer;
    int target = -1; // -1 = all, -2 = allies, or specific player ID
} m_chat;
```

In `handleKeyEvent()`, add a Tab handler when chat is active (before the existing chat key handling):
```cpp
case input::Key::Tab:
    if (m_chat.active) {
        if (m_chat.target == -1) m_chat.target = -2;      // All -> Allies
        else m_chat.target = -1;                            // Allies -> All
        return true;
    }
    break;
```

In the chat bar rendering, show the target label:
```cpp
const char *targetLabel = (m_chat.target == -1) ? "[All]" : "[Allies]";
m_statText->string = std::string(targetLabel) + " Say: " + m_chat.buffer + "_";
```

In the Return handler, pass `m_chat.target` to `EventManager::sendChatMessage()`.

**Commit message:** `feat: chat target selector — Tab cycles All/Allies`

---

## Task 11: Campaign cutscene stub (skip button)

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/Engine.cpp`

**What to do:**
AoE2 campaigns have intro/victory/loss text in the scenario description fields. When loading a campaign scenario, if there is scenario description text, show it as a text overlay with a "Skip" button before gameplay starts. This is a minimal cutscene implementation.

In `Engine::setup()`, after scenario is loaded successfully, check for `scenario->messages.instructions` text. If non-empty, store it in a new `m_scenarioBriefing` string field.

In `Engine.h`, add:
```cpp
std::string m_scenarioBriefing;
bool m_showBriefing = false;
```

In `drawUi()`, if `m_showBriefing` is true, render an overlay with the briefing text and a "Start Mission" button.

In `handleEvent()`, if `m_showBriefing` is true, consume events and check for click on "Start Mission" or Escape to dismiss.

**Commit message:** `feat: campaign scenario briefing screen`

---

## Task 12: Tech tree close on Escape key

**Files to modify:**
- `/home/dima/Projects/freeaoe/src/ui/TechTreeScreen.cpp`

**What to do:**
Currently the tech tree only closes on click. Add Escape key handling:

In `TechTreeScreen::handleEvent()`, add before the existing code:
```cpp
if (event.type == input::Event::KeyPressed && event.key.code == input::Key::Escape) {
    hide();
    return true;
}
```

Do the same for `SettingsScreen::handleEvent()` and `DiplomacyScreen` if they don't already support Escape.

**Commit message:** `fix: Escape key closes tech tree, settings, and diplomacy screens`

---

## Summary of tasks

| # | Task | Files | Est. |
|---|------|-------|------|
| 1 | Chat taunts (1-42 standard text + audio) | Engine.cpp | 3 min |
| 2 | Game speed hotkeys F7/F8 | Engine.cpp | 2 min |
| 3 | Sync SettingsScreen game speed with Engine | SettingsScreen.h/cpp, Engine.cpp | 3 min |
| 4 | Save/Load UI dialog with file list | Engine.h/cpp | 5 min |
| 5 | Post-game statistics screen | Engine.h/cpp | 5 min |
| 6 | H key cycles TCs and selects | Engine.cpp | 2 min |
| 7 | Minimap mode toggle button | Engine.cpp | 3 min |
| 8 | F2 hotkey help overlay | Engine.cpp | 2 min |
| 9 | Stub scenario editor button | HomeScreen.cpp | 2 min |
| 10 | Chat target selector (All/Allies) | Engine.h/cpp | 3 min |
| 11 | Campaign briefing screen | Engine.h/cpp | 4 min |
| 12 | Escape closes overlay screens | TechTreeScreen.cpp, SettingsScreen.cpp | 2 min |

**Total: ~36 minutes**

**Not in scope (separate milestones):**
- Scenario Editor: Full editor is a multi-week project. Button is stubbed in Task 9.
- Hotkey remapping system: Requires config file format + UI. Deferred; F2 help is interim.
- Graphical tech tree with icons/arrows: Current text-based version is functional. Graphical version requires SLP icon rendering + layout engine.
- Campaign cinematics (AVI playback): Requires video codec integration. Text briefing in Task 11 is interim.
