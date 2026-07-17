# Plan: Objectives Panel

**Goal:** Collect triggers with `isObjective != 0`, display them as a bulleted list in a HUD overlay (sorted by `descriptionOrder`), and mark each one completed (strikethrough/checkmark) when the trigger fires.

**Status:** Not started

---

## Data available

`genie::Trigger` (in `genie/script/scn/Trigger.h`) has:
- `int8_t isObjective` — non-zero if this trigger should appear as an objective
- `int32_t descriptionOrder` — sort order in the objectives list
- `std::string description` — the text to display
- `int8_t startingState` — whether it starts enabled

`ScenarioController` already holds `std::vector<Trigger> m_triggers` where each `Trigger` wraps a `genie::Trigger`.

---

## Step 1: Expose objectives from `ScenarioController`

### `src/mechanics/ScenarioController.h`

Add a public struct and accessor:

```cpp
struct Objective {
    std::string description;
    int order = 0;
    bool completed = false;
    bool enabled = false;
};

const std::vector<Objective> &objectives() const { return m_objectives; }
```

Add private member:

```cpp
std::vector<Objective> m_objectives;
```

### `src/mechanics/ScenarioController.cpp`

In `setScenario()`, after the trigger vector is built (after the `m_triggers.emplace_back(trigger)` loop), collect objectives:

```cpp
m_objectives.clear();
for (const genie::Trigger &trig : scenario->triggers) {
    if (trig.isObjective == 0) continue;
    Objective obj;
    obj.description = trig.description;
    obj.order = trig.descriptionOrder;
    obj.enabled = (trig.startingState != 0);
    obj.completed = false;
    m_objectives.push_back(obj);
}
// Sort by descriptionOrder
std::sort(m_objectives.begin(), m_objectives.end(),
    [](const Objective &a, const Objective &b) { return a.order < b.order; });
```

In `handleTriggerEffect` (where effects are executed after a trigger fires), mark the corresponding objective complete. Add at the top of `handleTriggerEffect` or at the point where trigger completion is processed in `update()`:

In `ScenarioController::update()`, when a trigger is satisfied and its effects are executed:

```cpp
// After: if (trigger.isSatisfied()) { ... fire effects ... }
// Add:
for (Objective &obj : m_objectives) {
    // Match by description (best available key since triggers lack stable IDs)
    if (!trigger.name.empty() && obj.description == trigger.name) {
        obj.completed = true;
    }
}
```

A more robust match: iterate `m_objectives` with the same index as `m_triggers` if the order is preserved.

**Better approach** — store the trigger index alongside the Objective:

```cpp
struct Objective {
    std::string description;
    int order = 0;
    bool completed = false;
    bool enabled = false;
    int triggerIndex = -1; // index into m_triggers
};
```

Then match by `triggerIndex` in `update()`.

---

## Step 2: Objectives overlay in `Engine`

### `Engine.h` — add cached objective text

```cpp
// In Engine private section:
struct ObjectiveItem {
    Drawable::Text::Ptr text;
    bool completed = false;
};
std::vector<ObjectiveItem> m_objectiveItems;
bool m_objectivesDirty = true;
bool m_objectivesVisible = false;
```

### `Engine.cpp` — toggle visibility

In `handleKeyEvent`, add:

```cpp
case SDLK_F11: // or 'O' for objectives
    m_objectivesVisible = !m_objectivesVisible;
    return true;
```

### `Engine.cpp` — draw objectives

In `drawUi()`:

```cpp
if (m_objectivesVisible && m_scenarioController) {
    const auto &objs = m_scenarioController->objectives();
    if (objs.empty()) {
        // Draw "No objectives" placeholder
    } else {
        // Draw dark background panel
        Drawable::Rect bg;
        bg.rect = ScreenRect(10, 60, 280, 20 + int(objs.size()) * 24);
        bg.fillColor = Drawable::Color(0, 0, 0, 160);
        renderTarget_->draw(bg);

        // Draw title
        // renderTarget_->draw(titleText, ScreenPos(16, 64));

        for (size_t i = 0; i < objs.size(); i++) {
            const auto &obj = objs[i];
            if (!obj.enabled) continue;

            std::string prefix = obj.completed ? "[x] " : "[ ] ";
            std::string line = prefix + obj.description;

            // Cache or recreate text
            if (i >= m_objectiveItems.size() || m_objectivesDirty) {
                ObjectiveItem item;
                item.text = renderTarget_->createText(line, 14);
                item.completed = obj.completed;
                if (i < m_objectiveItems.size()) {
                    m_objectiveItems[i] = std::move(item);
                } else {
                    m_objectiveItems.push_back(std::move(item));
                }
            }

            Drawable::Color textColor = obj.completed ?
                Drawable::Color(128, 255, 128) : Drawable::White;
            // renderTarget_->draw(*m_objectiveItems[i].text, ScreenPos(16, 84 + i * 24), textColor);
        }

        m_objectivesDirty = false;
    }
}
```

The exact draw API depends on whether `IRenderTarget` accepts a color parameter — check how `addMessage` draws text.

---

## Step 3: Auto-show on scenario load

When a scenario is loaded and objectives exist, auto-show the panel:

```cpp
// In Engine::setup() after scenario is loaded:
if (m_scenarioController && !m_scenarioController->objectives().empty()) {
    m_objectivesVisible = true;
}
```

---

## Step 4: Mark dirty when objectives complete

In `ScenarioController::update()`, when an objective is marked complete, notify Engine somehow. Options:
- Add a signal `ObjectiveCompleted` to `ScenarioController` using `SignalEmitter`
- Have `Engine` poll `objectives()` each frame and compare against cached state

Simplest: Engine polls each draw call (the vector is small, comparison is cheap):

```cpp
// In Engine::drawUi(), before drawing objectives:
if (m_scenarioController) {
    const auto &objs = m_scenarioController->objectives();
    if (objs.size() != m_objectiveItems.size()) {
        m_objectivesDirty = true;
    } else {
        for (size_t i = 0; i < objs.size(); i++) {
            if (objs[i].completed != m_objectiveItems[i].completed) {
                m_objectivesDirty = true;
                break;
            }
        }
    }
}
```

---

## Access from Engine to ScenarioController

`Engine` does not currently hold a direct reference to `ScenarioController`. It is owned by `GameState`. Add an accessor:

### `src/mechanics/GameState.h`

```cpp
ScenarioController *scenarioController() { return &m_scenarioController; }
```

### In `Engine`, access via `state->scenarioController()`.

---

## Testing

1. Load a campaign scenario with defined objectives (e.g., William Wallace tutorial).
2. Press F11 — objectives panel should appear with unchecked items.
3. Complete an objective trigger (e.g., build a TC) — corresponding item should turn green with [x].
4. Press F11 again — panel hides.
