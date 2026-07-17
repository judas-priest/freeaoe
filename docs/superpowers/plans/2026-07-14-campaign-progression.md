# Plan: Campaign Progression

**Goal:** After victory, load the next scenario in the current campaign. Show "Campaign Complete!" when the last scenario is beaten. Requires extending `ScenarioBrowser::Result` with a `CpxFile` handle and `scenarioIndex`, then loading the next scenario from `Engine` after `DeclareVictory` fires.

**Status:** Not started

---

## Current flow

`ScenarioBrowser::show()` returns a `ScenarioBrowser::Result` containing only `genie::ScnFilePtr scenario`. The `CpxFile` that was loaded to extract the scenario is discarded. There is no record of which scenario index within the campaign was selected.

`ScenarioController::handleTriggerEffect` handles `DeclareVictory` effects (presumably calls `Engine::declareVictory` or similar — check `Engine.cpp`).

---

## Step 1: Extend `ScenarioBrowser::Result`

### `src/ui/ScenarioBrowser.h`

```cpp
struct Result {
    genie::ScnFilePtr scenario;
    bool isRandomMap = false;
    int randomMapType = 0;
    int randomMapSize = 144;
    int randomPlayerCount = 2;

    // Campaign progression
    std::string campaignPath;    // path to the .cpx file, empty if standalone scenario
    int scenarioIndex = -1;      // current scenario's index within the campaign (-1 = not a campaign)
    int scenarioCount = 0;       // total scenarios in campaign
};
```

### `src/ui/ScenarioBrowser.cpp`

In `openCampaign(const Entry &entry)` — or wherever the scenario is loaded from a CPX — store the cpx path and index in the result:

```cpp
// When a scenario inside a campaign is selected:
m_result = cpx.getScnFile(selectedIndex);
m_campaignPath = entry.path;
m_selectedScenarioIndex = selectedIndex;
m_selectedScenarioCount = entry.scenarioCount;
```

In `ScenarioBrowser::run()`, before returning:

```cpp
// Populate result campaign info
if (!m_campaignPath.empty()) {
    // result.scenario already set
    // We need to return campaignPath and index to caller
}
```

The `run()` method returns `genie::ScnFilePtr` directly. Restructure: make it populate a `Result` member instead:

```cpp
// In ScenarioBrowser private:
Result m_fullResult;
std::string m_campaignPath;
int m_selectedScenarioIndex = -1;
int m_selectedScenarioCount = 0;
```

In `ScenarioBrowser::show()`:

```cpp
ScenarioBrowser::Result ScenarioBrowser::show(const std::string &campaignsPath)
{
    ScenarioBrowser browser;
    browser.scan(campaignsPath);
    Result result;
    if (browser.m_entries.empty()) {
        WARN << "No campaigns found in" << campaignsPath;
        return result;
    }
    result.scenario = browser.run();
    result.campaignPath       = browser.m_campaignPath;
    result.scenarioIndex      = browser.m_selectedScenarioIndex;
    result.scenarioCount      = browser.m_selectedScenarioCount;
    if (browser.m_randomMapResult.start) {
        result.isRandomMap        = true;
        result.randomMapType      = browser.m_randomMapResult.mapType;
        result.randomMapSize      = browser.m_randomMapResult.mapSize;
        result.randomPlayerCount  = browser.m_randomMapResult.playerCount;
    }
    return result;
}
```

When a campaign entry is tapped in `openCampaign`, store:

```cpp
m_campaignPath = entry.path;
m_selectedScenarioIndex = selectedScenarioIndex; // 0-based index in CPX
m_selectedScenarioCount = entry.scenarioCount;
```

---

## Step 2: Store campaign state in `Engine`

### `Engine.h` — add member

```cpp
// Campaign progression
std::string m_campaignPath;
int m_campaignScenarioIndex = -1;
int m_campaignScenarioCount = 0;
```

### `Engine::setup()` — record campaign info from result

```cpp
// After the scenario is loaded:
m_campaignPath           = result.campaignPath;
m_campaignScenarioIndex  = result.scenarioIndex;
m_campaignScenarioCount  = result.scenarioCount;
```

---

## Step 3: Hook into victory

Find where `DeclareVictory` is handled. In `ScenarioController::handleTriggerEffect`:

```cpp
case genie::TriggerEffect::DeclareVictory:
    // ... existing: m_engine->addMessage("Victory!") or show result screen
```

Add a call to `Engine::onVictory()` (new method):

```cpp
if (m_engine) {
    m_engine->onVictory();
}
```

### `Engine.h` — add method

```cpp
void onVictory();
```

### `Engine.cpp` — implement

```cpp
void Engine::onVictory()
{
    // Is this a campaign scenario?
    if (m_campaignPath.empty() || m_campaignScenarioIndex < 0) {
        addMessage("Victory!");
        // Show result overlay (existing behavior)
        return;
    }

    const int nextIndex = m_campaignScenarioIndex + 1;

    if (nextIndex >= m_campaignScenarioCount) {
        // Last scenario — campaign complete
        addMessage("Campaign Complete!");
        if (m_resultOverlay) {
            m_resultOverlay->text = "Campaign Complete!";
        }
        return;
    }

    // Load next scenario
    try {
        genie::CpxFile cpx;
        cpx.load(m_campaignPath);
        genie::ScnFilePtr nextScenario = cpx.getScnFile(nextIndex);
        if (!nextScenario) {
            WARN << "Failed to load scenario" << nextIndex << "from" << m_campaignPath;
            addMessage("Victory!");
            return;
        }

        // Brief victory message before transitioning
        addMessage("Victory! Loading next mission...");

        // Short delay then reload (or immediate)
        m_campaignScenarioIndex = nextIndex;
        setup(nextScenario); // reload the engine with the next scenario
    } catch (const std::exception &ex) {
        WARN << "Exception loading next campaign scenario:" << ex.what();
        addMessage("Victory!");
    }
}
```

---

## Step 4: Includes

In `Engine.cpp` add:

```cpp
#include <genie/script/ScnFile.h>  // for CpxFile
```

`genie::CpxFile` is declared in `genie/script/ScnFile.h` (line 198).

---

## Considerations

- `Engine::setup(scenario)` currently sets up the full game state. Calling it again mid-game will reload everything — this is the intended behavior for campaign transitions.
- The brief "Victory! Loading next mission..." message gives the player a moment to see the result before the map reloads.
- For the full campaign complete case, we keep the result overlay showing "Campaign Complete!" without reloading.
- The `ScenarioBrowser::Result` must be preserved beyond the initial scenario load — store `campaignPath` and `scenarioIndex/Count` as `Engine` members (already done in Step 2).

---

## Testing

1. Load campaign "William Wallace" (6 scenarios).
2. Complete scenario 1 (trigger DeclareVictory fires).
3. Map should automatically reload with scenario 2.
4. Complete scenario 6 — "Campaign Complete!" overlay should appear, no reload.
5. Load a standalone `.scx` — winning shows "Victory!" without campaign progression.
