# Module 38: Hotkey Configuration File — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow hotkey rebinding via a JSON config file instead of hardcoded keys in Engine.cpp.

**Architecture:** Create a HotkeyManager that loads key bindings from `~/.freeaoe/hotkeys.json` (or game dir). Falls back to defaults if file missing. Engine::handleKeyEvent reads from HotkeyManager instead of hardcoded switch/case.

**Tech Stack:** C++, JSON (use existing config system or simple parser)

---

### Task 1: Create HotkeyManager with default bindings

**Files:**
- Create: `src/global/HotkeyManager.h`
- Create: `src/global/HotkeyManager.cpp`

- [ ] **Step 1: Define action enum and default bindings**

```cpp
// HotkeyManager.h
#pragma once
#include "input/InputEvent.h"
#include <unordered_map>
#include <string>

class HotkeyManager {
public:
    enum class Action {
        Pause, SpeedUp, SpeedDown,
        QuickSave, QuickLoad,
        CycleIdleVillager, CycleTownCenter,
        Stop, AttackMove, Patrol, Guard,
        DeleteUnit, ShowMenu, ToggleStats,
        ToggleObjectives, CycleMinimapMode,
        HotkeyHelp, CameraFollow,
        SelectAllMilitary,
        Count
    };

    static HotkeyManager &instance();

    void loadFromFile(const std::string &path);
    void saveToFile(const std::string &path) const;

    input::Key keyFor(Action action) const;
    Action actionFor(input::Key key, bool shift = false, bool ctrl = false) const;

private:
    HotkeyManager();
    void setDefaults();

    struct Binding {
        input::Key key;
        bool shift = false;
        bool ctrl = false;
    };
    std::unordered_map<int, Binding> m_bindings; // Action -> Binding
};
```

- [ ] **Step 2: Implement defaults matching current hardcoded keys**

```cpp
// HotkeyManager.cpp
void HotkeyManager::setDefaults() {
    m_bindings[int(Action::Pause)]              = {input::Key::F3};
    m_bindings[int(Action::CycleMinimapMode)]   = {input::Key::F4};
    m_bindings[int(Action::QuickSave)]          = {input::Key::F5};
    m_bindings[int(Action::QuickLoad)]          = {input::Key::F9};
    m_bindings[int(Action::CycleIdleVillager)]  = {input::Key::F1};
    m_bindings[int(Action::HotkeyHelp)]         = {input::Key::F2};
    m_bindings[int(Action::CameraFollow)]       = {input::Key::F6};
    m_bindings[int(Action::SpeedDown)]          = {input::Key::F7};
    m_bindings[int(Action::SpeedUp)]            = {input::Key::F8};
    m_bindings[int(Action::ToggleObjectives)]   = {input::Key::F11};
    m_bindings[int(Action::CycleTownCenter)]    = {input::Key::H};
    m_bindings[int(Action::Stop)]               = {input::Key::S};
    m_bindings[int(Action::AttackMove)]         = {input::Key::A};
    m_bindings[int(Action::Patrol)]             = {input::Key::P};
    m_bindings[int(Action::Guard)]              = {input::Key::G};
    m_bindings[int(Action::DeleteUnit)]         = {input::Key::Delete};
    m_bindings[int(Action::SelectAllMilitary)]  = {input::Key::A, true}; // Shift+A
    m_bindings[int(Action::ToggleStats)]        = {input::Key::Tab};
}
```

- [ ] **Step 3: Implement JSON load/save**

Use a simple key=value format or minimal JSON parser. The config file format:
```json
{
    "Pause": "F3",
    "SpeedUp": "F8",
    "Stop": "S",
    "AttackMove": "A",
    "Patrol": "P"
}
```

- [ ] **Step 4: Build and verify**

- [ ] **Step 5: Commit**

---

### Task 2: Refactor Engine::handleKeyEvent to use HotkeyManager

**Files:**
- Modify: `src/Engine.cpp` (replace hardcoded key checks with HotkeyManager lookups)

- [ ] **Step 1: Replace key checks**

Change patterns like:
```cpp
case input::Key::F3:
    m_paused = !m_paused;
```
To:
```cpp
if (HotkeyManager::instance().actionFor(event.key.code) == HotkeyManager::Action::Pause) {
    m_paused = !m_paused;
}
```

- [ ] **Step 2: Build, test, commit**

---

### Task 3: Add hotkey display in F2 help screen

- [ ] **Step 1: Read bindings from HotkeyManager instead of hardcoded strings**

Currently F2 shows hardcoded help text. Update to read from HotkeyManager so rebindings are reflected.

- [ ] **Step 2: Build, test, commit**

**Note:** No in-game rebinding UI is planned — editing the JSON file is sufficient for now.
