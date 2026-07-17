# Module 8: Save/Load Game UI

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Functional in-game save/load screen with file list and quicksave/quickload hotkeys.

**Architecture:** `SaveGame` has static API: `SaveGame::save(path, state, cameraX, cameraY)` and `SaveGame::load(path, state, cameraX, cameraY)` (SaveGame.h:17-18). Both take `const std::string &path` and `GameState &state` plus camera floats. Engine.h already has `SaveLoadScreen` struct (line 278-284) with `visible`, `loadMode`, `files`, `selectedIndex`, `newSaveName`. Methods `showSaveLoadScreen()`, `renderSaveLoadScreen()`, `handleSaveLoadEvent()` are declared (lines 285-287) — need implementation.

**Tech Stack:** C++20, modify Engine.cpp.

---

### Task 1: Implement save/load screen rendering

**Files:**
- Modify: `src/Engine.cpp` — implement `showSaveLoadScreen()`, `renderSaveLoadScreen()`, `handleSaveLoadEvent()`

- [ ] **Step 1: Read current implementations of these 3 methods**

They may be stubs or partially implemented. Read Engine.cpp to find them.

- [ ] **Step 2: Implement showSaveLoadScreen**

```cpp
void Engine::showSaveLoadScreen(bool loadMode)
{
    m_saveLoadScreen.visible = true;
    m_saveLoadScreen.loadMode = loadMode;
    m_saveLoadScreen.selectedIndex = -1;
    m_saveLoadScreen.newSaveName = "";

    // Scan for .faoe save files
    m_saveLoadScreen.files.clear();
    namespace fs = std::filesystem;
    std::string saveDir = "./saves";
    if (!fs::exists(saveDir)) {
        fs::create_directories(saveDir);
    }
    for (auto &entry : fs::directory_iterator(saveDir)) {
        if (entry.path().extension() == ".faoe") {
            m_saveLoadScreen.files.push_back(entry.path().stem().string());
        }
    }
    std::sort(m_saveLoadScreen.files.begin(), m_saveLoadScreen.files.end());
}
```

- [ ] **Step 3: Implement renderSaveLoadScreen**

Draw modal overlay with file list, highlight selected, show save name input in save mode, and Save/Load + Cancel buttons. Use existing `Drawable::Text` and `renderTarget_->fillRect()` patterns from other overlays in Engine.cpp.

- [ ] **Step 4: Implement handleSaveLoadEvent**

```cpp
bool Engine::handleSaveLoadEvent(const input::Event &event)
{
    if (!m_saveLoadScreen.visible) return false;

    if (event.type == input::EventType::KeyDown) {
        if (event.key == input::Key::Escape) {
            m_saveLoadScreen.visible = false;
            return true;
        }
    }

    if (event.type == input::EventType::MouseButtonRelease) {
        // Hit test file list entries — set selectedIndex
        // Hit test Save/Load button — call SaveGame
        // Hit test Cancel button — close

        // On Save:
        if (/* save button clicked */ false) {
            std::string filename = "./saves/" + m_saveLoadScreen.newSaveName + ".faoe";
            auto state = state_manager_.currentState();
            if (state) {
                auto cam = /* get camera position */;
                SaveGame::save(filename, *state, cam.x, cam.y);
                addMessage("Game saved: " + m_saveLoadScreen.newSaveName);
            }
            m_saveLoadScreen.visible = false;
        }

        // On Load:
        if (/* load button clicked */ false) {
            int idx = m_saveLoadScreen.selectedIndex;
            if (idx >= 0 && idx < (int)m_saveLoadScreen.files.size()) {
                std::string filename = "./saves/" + m_saveLoadScreen.files[idx] + ".faoe";
                auto state = state_manager_.currentState();
                float cx, cy;
                if (state && SaveGame::load(filename, *state, cx, cy)) {
                    // Restore camera
                    addMessage("Game loaded: " + m_saveLoadScreen.files[idx]);
                }
            }
            m_saveLoadScreen.visible = false;
        }
    }

    return true; // consume all events when screen is open
}
```

- [ ] **Step 5: Wire into event loop and render loop**

Ensure `handleSaveLoadEvent()` is called before other event handling, and `renderSaveLoadScreen()` is called during UI rendering.

- [ ] **Step 6: Add menu button or hotkey to open save/load**

Add F6 for save screen, F7 for load screen in `handleKeyEvent()`:

```cpp
case input::Key::F6:
    showSaveLoadScreen(false); // save mode
    return true;
case input::Key::F7:
    showSaveLoadScreen(true); // load mode
    return true;
```

- [ ] **Step 7: Build, test, commit**

```bash
git add src/Engine.cpp
git commit -m "$(cat <<'EOF'
feat: save/load game screen with file list, F6/F7 hotkeys
EOF
)"
```

---

### Task 2: Quicksave/quickload

**Files:**
- Modify: `src/Engine.cpp` — F5 quicksave, F9 quickload

- [ ] **Step 1: Add quicksave/quickload hotkeys**

In `handleKeyEvent()`:

```cpp
case input::Key::F5: {
    auto state = state_manager_.currentState();
    if (state) {
        namespace fs = std::filesystem;
        if (!fs::exists("./saves")) fs::create_directories("./saves");
        float cx = 0, cy = 0; // TODO: get actual camera position
        SaveGame::save("./saves/quicksave.faoe", *state, cx, cy);
        addMessage("Quicksave");
    }
    return true;
}
case input::Key::F9: {
    auto state = state_manager_.currentState();
    if (state) {
        float cx, cy;
        if (SaveGame::load("./saves/quicksave.faoe", *state, cx, cy)) {
            addMessage("Quickload");
        } else {
            addMessage("No quicksave found");
        }
    }
    return true;
}
```

- [ ] **Step 2: Build, test, commit**

```bash
git add src/Engine.cpp
git commit -m "$(cat <<'EOF'
feat: quicksave (F5) and quickload (F9) hotkeys
EOF
)"
```
