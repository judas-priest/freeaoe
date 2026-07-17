# Module 17: Game Setup Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add civilization selection, team assignment, and starting age selection to the random map setup screen.

**Architecture:** Extend `RandomMapSetup` with dropdown-style UI elements for each player slot. Civ list read from dat file (DataManager). Selections passed to `GameState` constructor which applies them to `Player` objects. Uses existing UI patterns from ActionPanel buttons.

**Tech Stack:** C++, RandomMapSetup, SDL2 rendering, DataManager, Civilization

**Verified APIs:**
- `RandomMapSetup::Result` struct: `bool start`, `int mapType`, `int mapSize`, `int playerCount`
- `RandomMapSetup::show(SdlWindow*, shared_ptr<IRenderTarget>)` — static method, blocks until user clicks Start
- Members: `m_mapType` (int), `m_mapSize` (index into sizes array), `m_playerCount`
- UI is drawn directly via `m_renderTarget` with `Drawable::Text::Ptr`
- `MAP_TYPE_COUNT=5`, `MAP_SIZE_COUNT=5`, constexpr arrays for names/values
- The `Result` struct must be extended with civ/team/age fields
- `DataManager::Inst().datFile()->Civs` gives civ data with `.Name` field

---

### Task 1: Add civilization selection to RandomMapSetup

**Files:**
- Modify: `src/ui/RandomMapSetup.h` (add civ selection state)
- Modify: `src/ui/RandomMapSetup.cpp` (render civ selector, handle clicks)
- Modify: `src/mechanics/GameState.cpp` (apply selected civ to player)

**Context:** `RandomMapSetup` currently has: map type, map size, player count. It renders as a simple overlay with text labels and left/right arrows. Civ names come from `DataManager::Inst().civilizationNames()` or by iterating civ IDs in the dat. The current `GameState` constructor creates players with `civId = p` (sequential), not player-chosen.

- [ ] **Step 1: Extend Result and add members to RandomMapSetup.h**

Extend `Result` struct:

```cpp
struct Result {
    bool start = false;
    int mapType = 0;
    int mapSize = 144;
    int playerCount = 2;
    int startingAge = 0; // 0=Dark, 1=Feudal, 2=Castle, 3=Imperial
    struct PlayerSetup {
        int civId = 0; // 0 = random
        int team = 0;  // 0 = no team, 1-4 = team
    };
    PlayerSetup players[8];
};
```

Add private members:

```cpp
int m_startingAge = 0;
std::vector<std::string> m_civNames;
int m_playerCivs[8] = {};
int m_playerTeams[8] = {};
```

- [ ] **Step 2: Populate civ names from dat in constructor**

In `RandomMapSetup` constructor:

```cpp
// Load civilization names from dat
const auto &civs = DataManager::Inst().datFile()->Civs;
m_civNames.push_back("Random"); // Index 0
for (size_t i = 0; i < civs.size(); i++) {
    m_civNames.push_back(civs[i].Name);
}
m_playerSetups.resize(8); // Max 8 players
```

- [ ] **Step 3: Render civ selectors per player**

In `RandomMapSetup::render()`, for each player slot (p = 0 to playerCount-1), draw:

```cpp
// Player N: [< CivName >]  Team: [< N >]
int y = baseY + p * rowHeight;
std::string civText = m_civNames[m_playerSetups[p].civId];

// Draw "Player N:" label
renderText("Player " + std::to_string(p + 1) + ":", x, y);

// Draw civ name with left/right arrows
renderText("< " + civText + " >", civX, y);

// Draw team selector
std::string teamText = m_playerSetups[p].team == 0 ? "-" : std::to_string(m_playerSetups[p].team);
renderText("Team: < " + teamText + " >", teamX, y);
```

- [ ] **Step 4: Handle click events on civ/team arrows**

In `RandomMapSetup::handleEvent()`, detect clicks on the left/right arrows:

```cpp
// Civ left arrow clicked for player p
m_playerSetups[p].civId--;
if (m_playerSetups[p].civId < 0) {
    m_playerSetups[p].civId = m_civNames.size() - 1;
}

// Civ right arrow clicked for player p
m_playerSetups[p].civId++;
if (m_playerSetups[p].civId >= static_cast<int>(m_civNames.size())) {
    m_playerSetups[p].civId = 0;
}

// Team left/right cycles 0-4
m_playerSetups[p].team = (m_playerSetups[p].team + 1) % 5;
```

- [ ] **Step 5: Pass civ selections to GameState**

Extend `RandomMapSetup::Settings` (or the struct passed to GameState) with the player setup data:

```cpp
struct Settings {
    MapType type = Arabia;
    int size = Medium;
    int playerCount = 2;
    int startingAge = 0;
    std::vector<PlayerSetup> players;
};
```

In `GameState` constructor, when creating players:

```cpp
int civId = settings.players[p].civId;
if (civId == 0) {
    // Random: pick a random civ (1 to numCivs)
    civId = 1 + (syncRandom.next() % (numCivs - 1));
}
auto player = std::make_shared<Player>(p + 1, civId, map, startingResources);
```

- [ ] **Step 6: Apply starting age**

In `GameState` constructor, after creating all players:

```cpp
if (settings.startingAge > 0) {
    for (auto &player : m_players) {
        if (!player || player->playerId() == 0) continue;
        player->setAge(static_cast<Player::Age>(settings.startingAge));
    }
}
```

- [ ] **Step 7: Apply team stances**

```cpp
// Set team allies
for (size_t i = 0; i < m_players.size(); i++) {
    for (size_t j = 0; j < m_players.size(); j++) {
        if (i == j || !m_players[i] || !m_players[j]) continue;
        if (settings.players[i].team > 0 &&
            settings.players[i].team == settings.players[j].team) {
            m_players[i]->setDiplomaticStance(j, Player::DiplomaticStance::Ally);
        }
    }
}
```

- [ ] **Step 8: Build, test, commit**

```bash
cd build && make -j$(nproc)
git add src/ui/RandomMapSetup.h src/ui/RandomMapSetup.cpp src/mechanics/GameState.cpp
git commit -m "feat: civ selection, team assignment, starting age in game setup"
```

---

### Task 2: Add starting age selector

**Files:**
- Modify: `src/ui/RandomMapSetup.cpp`

**Context:** Simple left/right toggle cycling Dark/Feudal/Castle/Imperial.

- [ ] **Step 1: Render starting age row**

```cpp
const char *ageNames[] = {"Dark Age", "Feudal Age", "Castle Age", "Imperial Age"};
renderText("Starting Age: < " + std::string(ageNames[m_startingAge]) + " >", x, ageY);
```

- [ ] **Step 2: Handle clicks**

```cpp
// Left arrow: m_startingAge = (m_startingAge + 3) % 4;
// Right arrow: m_startingAge = (m_startingAge + 1) % 4;
```

- [ ] **Step 3: Build, commit**

```bash
cd build && make -j$(nproc)
git add src/ui/RandomMapSetup.cpp
git commit -m "feat: starting age selector in game setup screen"
```
