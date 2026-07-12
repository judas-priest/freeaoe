# freeaoe — Complete Mobile Game Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform freeaoe from a 35%-complete tech demo into a fully playable Age of Empires 2 mobile game with ALL expected features: random maps, AI, save/load, hotkeys, formations, monks, diplomacy, tech tree, market, game speed, and complete UI.

**Architecture:** 8 phases, 45 tasks. Each task produces a working build. Phases ordered by player impact — what matters most to actually playing the game.

**Tech Stack:** C++20, SDL2, SDL_ttf, Android NDK, genieutils, miniaudio

**Build:** `cd android && rm -rf app/.cxx app/build && unset LD_PRELOAD && ./gradlew assembleDebug`
**Install:** `unset LD_PRELOAD && adb install -r android/app/build/outputs/apk/debug/app-debug.apk`

**References:**
- [AoE2 Tech Tree](https://aoe2techtree.net/)
- [AoE2 Diplomacy Wiki](https://ageofempires.fandom.com/wiki/Diplomacy)
- [AoE2 RMS Guide](https://steamcommunity.com/sharedfiles/filedetails/?id=155256742)
- [genie-rms parser](https://github.com/genie-js/genie-rms)
- [AoE2 Save Format](https://github.com/stefan-kolb/aoc-mgx-format)
- [OpenAge RE docs](https://simonsan.github.io/openage-webdocs/sphinx/doc/sphinx/handbooks/reverse_engineering.html)

---

## Phase 1: Game Speed, Pause, and Hotkeys (small effort, huge impact)

### Task 1: Implement game speed control and pause

**Problem:** No pause, no speed control. Game runs at fixed rate.

**Files:**
- Modify: `src/Engine.h` — add m_gameSpeed, m_paused
- Modify: `src/Engine.cpp` — multiply elapsed time by speed, skip update when paused
- Modify: `src/mechanics/GameState.cpp` — pass scaled time

- [ ] **Step 1:** Add `float m_gameSpeed = 1.0f;` and `bool m_paused = false;` to Engine.h
- [ ] **Step 2:** In Engine::start() game loop, multiply delta time: `Time scaledTime = deltaTime * m_gameSpeed;`. When paused, skip `state->update()`.
- [ ] **Step 3:** Handle +/- keys to adjust speed (0.5x, 1.0x, 1.5x, 2.0x), F3 for pause toggle
- [ ] **Step 4:** Show speed indicator in UI: "Paused", "1.0x", "1.5x", "2.0x"
- [ ] **Step 5:** Commit

---

### Task 2: Implement all keyboard hotkeys

**Problem:** Only arrow keys and Escape work. No build hotkeys, no control groups, no unit command hotkeys.

**Files:**
- Modify: `src/Engine.cpp` — handleKeyEvent()
- Modify: `src/mechanics/UnitManager.h/cpp` — control group support
- Modify: `src/mechanics/Player.h/cpp` — group recall

- [ ] **Step 1:** Add key bindings in handleKeyEvent:
```
Delete → kill selected units
S → stop
Z → patrol mode
A → attack move mode
G → garrison mode
R → repair mode
H → center on TC
. (period) → select idle villager
F3 → pause
+/- → game speed
```

- [ ] **Step 2:** Implement control groups:
```
Ctrl+1..9 → save selected units as group N
1..9 → select group N (double-press = center camera on group)
```
Use existing `Player::setUnitGroup`/`getUnitGroup`.

- [ ] **Step 3:** Commit

---

### Task 3: Implement shift-click waypoint queuing

**Problem:** Can't queue commands with shift. Important for patrol paths and economy.

**Files:**
- Modify: `src/Engine.cpp` — handleTouchEvent, handleMouseRelease
- Modify: `src/mechanics/UnitManager.cpp` — onRightClick, onLeftClick

- [ ] **Step 1:** Track shift key state in event handling
- [ ] **Step 2:** When shift is held, use `queueAction()` instead of `setCurrentAction()`
- [ ] **Step 3:** On Android: long-press-then-tap pattern for queuing (alternative to shift)
- [ ] **Step 4:** Commit

---

## Phase 2: Missing Unit Commands (medium effort)

### Task 4: Implement monk Convert action

**Files:**
- Create: `src/actions/ActionConvert.h/cpp`
- Modify: `src/ui/ActionPanel.cpp`, `src/mechanics/UnitManager.h/cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1:** Create ActionConvert — monk targets enemy unit, after conversion time (~5 sec), unit changes owner via `unit->setPlayer()`
- [ ] **Step 2:** Wire into ActionPanel for monk units
- [ ] **Step 3:** Commit

---

### Task 5: Implement monk Heal action

**Files:**
- Create: `src/actions/ActionHeal.h/cpp`

- [ ] **Step 1:** Create ActionHeal — monk targets allied unit, heals HP over time (similar to Repair but uses faith/mana)
- [ ] **Step 2:** Wire into ActionPanel
- [ ] **Step 3:** Commit

---

### Task 6: Implement monk Relic pickup and deposit

**Files:**
- Create: `src/actions/ActionPickupRelic.h/cpp`
- Modify: `src/mechanics/Building.cpp` — relic deposit generates gold

- [ ] **Step 1:** Create ActionPickupRelic — monk moves to relic, picks it up (relic becomes carried resource)
- [ ] **Step 2:** When deposited in monastery, start generating 1 gold per second
- [ ] **Step 3:** Commit

---

### Task 7: Implement Trade action (trade carts)

**Files:**
- Create: `src/actions/ActionTrade.h/cpp`

- [ ] **Step 1:** Create ActionTrade — trade cart moves between two markets, generates gold based on distance
- [ ] **Step 2:** Wire into ActionPanel for trade cart units
- [ ] **Step 3:** Commit

---

### Task 8: Implement Market buy/sell UI

**Problem:** Market buttons exist but aren't wired.

**Files:**
- Modify: `src/ui/ActionPanel.cpp` — handle SellWood/Food/Stone, BuyFood/BuyStone commands
- Modify: `src/mechanics/GameState.cpp` — tradingPrices logic already exists

- [ ] **Step 1:** Wire market buy/sell buttons to `GameState::tradingPrices`
- [ ] **Step 2:** Apply price changes (100 base, +3 per buy, -3 per sell, clamped)
- [ ] **Step 3:** Commit

---

### Task 9: Implement Formations

**Problem:** Units clump together instead of forming lines/boxes.

**Files:**
- Modify: `src/mechanics/UnitManager.cpp` — formation offset calculation
- Modify: `src/ui/ActionPanel.cpp` — wire formation buttons

- [ ] **Step 1:** When move command issued to group, calculate formation offsets:
  - Line: units spread perpendicular to movement direction
  - Box: units in rectangular grid
  - Flank: two columns
- [ ] **Step 2:** Wire formation buttons to set `m_formation` on selected units
- [ ] **Step 3:** Commit

---

### Task 10: Implement Ungarrison button, Gate open/close, Town Bell

**Files:**
- Modify: `src/ui/ActionPanel.cpp` — handle these commands
- Modify: `src/mechanics/Building.cpp` — gate state, town bell

- [ ] **Step 1:** Ungarrison button → eject all units from building
- [ ] **Step 2:** Gate open/close → toggle gate graphic and passability
- [ ] **Step 3:** Town bell → garrison all villagers in TC
- [ ] **Step 4:** Commit

---

## Phase 3: UI Screens (medium-large effort)

### Task 11: Implement Diplomacy screen

**Problem:** Diplo button shows "not yet implemented".

**Files:**
- Create: `src/ui/DiplomacyScreen.h/cpp`
- Modify: `src/Engine.cpp` — show screen when Diplo button pressed

- [ ] **Step 1:** Create DiplomacyScreen — fullscreen overlay showing:
  - Each player with color and civ name
  - Three buttons per player: Ally / Neutral / Enemy
  - Current stance highlighted
  - "Allied Victory" checkbox
- [ ] **Step 2:** On button press, call `Player::setDiplomaticStance()`
- [ ] **Step 3:** Render over game (similar to Dialog)
- [ ] **Step 4:** Commit

---

### Task 12: Implement Tech Tree viewer

**Problem:** Tech Tree button shows "not yet implemented".

**Files:**
- Create: `src/ui/TechTreeScreen.h/cpp`
- Modify: `src/Engine.cpp`

- [ ] **Step 1:** Create TechTreeScreen — scrollable overlay showing:
  - 4 age columns (Dark, Feudal, Castle, Imperial)
  - Buildings in each age with available units/techs
  - Color coding: green=available, red=unavailable, gray=not yet
  - Tap to see unit/tech stats
- [ ] **Step 2:** Load building/unit/tech data from DataManager
- [ ] **Step 3:** Render with scroll support (touch drag)
- [ ] **Step 4:** Commit

---

### Task 13: Implement Settings/Options screen

**Problem:** Settings button shows "not yet implemented".

**Files:**
- Create: `src/ui/SettingsScreen.h/cpp`
- Modify: `src/Engine.cpp`

- [ ] **Step 1:** Create SettingsScreen overlay with:
  - Sound volume slider (Config::SoundVolume)
  - Music volume slider (Config::MusicVolume)
  - Game speed selector
  - Language selector (en/ru)
- [ ] **Step 2:** Apply changes immediately via Config
- [ ] **Step 3:** Commit

---

### Task 14: Implement post-game Statistics screen

**Files:**
- Create: `src/ui/StatsScreen.h/cpp`
- Modify: `src/mechanics/Player.h/cpp` — track stats

- [ ] **Step 1:** Track during gameplay: units killed, units lost, buildings razed, resources gathered per type
- [ ] **Step 2:** Show after game ends (victory/defeat): score breakdown, timeline
- [ ] **Step 3:** Commit

---

## Phase 4: Save/Load Game (large effort)

### Task 15: Design save file format

**Files:**
- Create: `src/mechanics/SaveGame.h/cpp`

- [ ] **Step 1:** Define binary format:
```
Header: magic "FAOE", version, timestamp
Map: width, height, terrain IDs, elevations
Players: count, per-player: resources, diplomacy, age, researched techs
Units: count, per-unit: typeID, playerID, posX, posY, posZ, HP, carriedResources, currentAction
Triggers: state of each trigger (enabled/disabled, condition amounts)
Camera: position
```
- [ ] **Step 2:** Implement `SaveGame::save(path, GameState)`
- [ ] **Step 3:** Implement `SaveGame::load(path)` → returns ScnFile-like object
- [ ] **Step 4:** Commit

---

### Task 16: Wire Save/Load into menu

**Files:**
- Modify: `src/Engine.cpp` — handle Dialog::Save choice
- Modify: `src/ui/Dialog.cpp` — add Load button
- Modify: `src/main.cpp` — load from save file

- [ ] **Step 1:** Save button → SaveGame::save() to `saves/` directory
- [ ] **Step 2:** Add Load option to ScenarioBrowser (list .faoe files alongside .cpx)
- [ ] **Step 3:** Commit

---

## Phase 5: AI System (huge effort)

### Task 17: Wire AiPlayer into game loop

**Problem:** AI parser exists but AI never acts. `AiScript::update()` is empty.

**Files:**
- Modify: `src/ai/AiScript.cpp` — implement rule evaluation loop
- Modify: `src/mechanics/GameState.cpp` — create AiPlayer instances for non-human players

- [ ] **Step 1:** In setupScenario/setupGame, create `AiPlayer` for each non-human enabled player
- [ ] **Step 2:** Load default AI script (.per file) for each AI player
- [ ] **Step 3:** In AiScript::update(), loop through rules: for each rule, evaluate all conditions; if all true, execute all actions
- [ ] **Step 4:** Commit

---

### Task 18: Implement core AI conditions (20 most common)

**Files:**
- Modify: `src/ai/ScriptLoader.cpp` — implement condition factories

- [ ] **Step 1:** Implement: CanBuild, CanTrain, CanResearch, CurrentAge, PopulationCap, HousingHeadroom, IdleVillagerCount, MilitaryPopulation, CivilianPopulation, WallCompletedPercentage, ResourceAmount comparisons, StrategicNumber checks, UnitTypeCount, BuildingTypeCount, TechState, GameTime
- [ ] **Step 2:** Each condition reads from Player/UnitManager state and returns true/false
- [ ] **Step 3:** Commit

---

### Task 19: Implement core AI actions (15 most common)

**Files:**
- Modify: `src/ai/actions/Actions.cpp` — implement action execute() bodies
- Modify: `src/ai/ScriptLoader.cpp` — implement action factories

- [ ] **Step 1:** Implement: TrainUnit, BuildBuilding, ResearchItem, SetStrategicNumber, SetDiplomaticStance, Tribute, SetGoal, ChatToPlayer, EnableTimer, DisableTimer, AttackNow, DefendHere, TaskUnits
- [ ] **Step 2:** Each action modifies Player/UnitManager state
- [ ] **Step 3:** Commit

---

### Task 20: Implement AI building placement

**Problem:** AI places buildings at random offset from builder.

**Files:**
- Modify: `src/ai/actions/Actions.cpp:327`

- [ ] **Step 1:** Find open space near TC, away from existing buildings, on buildable terrain
- [ ] **Step 2:** Check building size and terrain passability
- [ ] **Step 3:** Commit

---

## Phase 6: Random Map Generation (huge effort)

### Task 21: Implement basic random terrain generator

**Problem:** No random maps — only scenarios and demo map.

**Files:**
- Create: `src/mechanics/RandomMapGenerator.h/cpp`
- Modify: `src/mechanics/Map.cpp` — add `createRandom()` method

- [ ] **Step 1:** Implement terrain generation:
  - Fill with grass base
  - Perlin noise for elevation
  - Water bodies (rivers, lakes, ocean edges based on map type)
  - Forest patches
  - Cliff generation
- [ ] **Step 2:** Resource placement: gold/stone mines, berry bushes, deer, boar, sheep
- [ ] **Step 3:** Starting positions: TC, 3 villagers, scout per player, evenly spaced
- [ ] **Step 4:** Commit

---

### Task 22: Implement Random Map Setup screen

**Files:**
- Create: `src/ui/RandomMapSetup.h/cpp`
- Modify: `src/ui/ScenarioBrowser.cpp` — add "Random Map" option

- [ ] **Step 1:** Screen with options:
  - Map type: Arabia, Islands, Black Forest, Arena, Nomad
  - Map size: Tiny/Small/Medium/Large/Giant
  - Number of players: 2-8
  - Difficulty: Standard/Moderate/Hard
  - Civilization picker per player
- [ ] **Step 2:** Generate map and start game
- [ ] **Step 3:** Commit

---

## Phase 7: Victory Conditions and Scoring

### Task 23: Implement Score tracking

**Files:**
- Modify: `src/mechanics/Player.h/cpp` — add score calculation
- Modify: `src/Engine.cpp` — display score

- [ ] **Step 1:** Calculate score from: military (units×cost), economy (resources gathered), technology (techs researched×cost), society (relics + wonders + map explored %)
- [ ] **Step 2:** Display current score in top bar or on F4
- [ ] **Step 3:** Commit

---

### Task 24: Implement Score and Timed victory

**Files:**
- Modify: `src/mechanics/ScenarioController.cpp:162-175`

- [ ] **Step 1:** Score victory: check if any player reached required score
- [ ] **Step 2:** Timed victory: when timer expires, player with highest score wins
- [ ] **Step 3:** Commit

---

### Task 25: Implement Wonder victory

**Files:**
- Modify: `src/mechanics/ScenarioController.cpp`
- Modify: `src/mechanics/Building.cpp`

- [ ] **Step 1:** When Wonder built, start 200-year countdown
- [ ] **Step 2:** If countdown completes without Wonder being destroyed, that player wins
- [ ] **Step 3:** All players notified when Wonder construction begins
- [ ] **Step 4:** Commit

---

### Task 26: Implement Relic victory

**Files:**
- Modify: `src/mechanics/ScenarioController.cpp`

- [ ] **Step 1:** Track relics per player
- [ ] **Step 2:** When one player holds all relics for 200 years, they win
- [ ] **Step 3:** Commit

---

### Task 27: Implement Alliance victory

**Files:**
- Modify: `src/mechanics/GameState.cpp:184-198` — uncomment and fix

- [ ] **Step 1:** Uncomment alliance win check
- [ ] **Step 2:** Check if all remaining players are allied
- [ ] **Step 3:** Commit

---

## Phase 8: Polish and Remaining Features

### Task 28: In-game background music

**Files:**
- Modify: `src/Engine.cpp` — play age-specific music during gameplay

- [ ] **Step 1:** Play random music track from `sound/music/` during gameplay
- [ ] **Step 2:** Change music when advancing ages
- [ ] **Step 3:** Commit

---

### Task 29: Sound spatialization (panning)

**Files:**
- Modify: `src/audio/AudioPlayer.cpp` — calculate pan from unit screen position

- [ ] **Step 1:** Calculate pan based on unit's screen X position relative to viewport center
- [ ] **Step 2:** Pass pan to playSound()
- [ ] **Step 3:** Commit

---

### Task 30: Idle villager button

**Files:**
- Modify: `src/Engine.cpp` — add idle villager cycling

- [ ] **Step 1:** Period key (.) → find next idle villager → select and center camera
- [ ] **Step 2:** Cycle through idle villagers on repeated presses
- [ ] **Step 3:** Commit

---

### Task 31: Double-click select all of same type

**Files:**
- Modify: `src/Engine.cpp` — touch handler

- [ ] **Step 1:** On double-tap on a unit, select all visible units of same type
- [ ] **Step 2:** Commit

---

### Task 32: Accuracy/miss system for ranged units

**Files:**
- Modify: `src/actions/ActionAttack.cpp`

- [ ] **Step 1:** Use `AccuracyPercent` from unit data
- [ ] **Step 2:** Random roll — if miss, offset missile target position
- [ ] **Step 3:** Commit

---

### Task 33: Area of effect damage (siege)

**Files:**
- Modify: `src/mechanics/Missile.cpp`

- [ ] **Step 1:** On missile impact, find all units within blast radius
- [ ] **Step 2:** Apply damage with falloff based on distance from center
- [ ] **Step 3:** Commit

---

### Task 34: Auto-reseed farms

**Files:**
- Modify: `src/mechanics/Farm.cpp`

- [ ] **Step 1:** When farm food depletes, auto-rebuild if player has resources and queue slot
- [ ] **Step 2:** Commit

---

### Task 35: Population cap message

**Files:**
- Modify: `src/mechanics/Building.cpp`

- [ ] **Step 1:** When trying to create unit with insufficient housing, show "Need more houses" message
- [ ] **Step 2:** Commit

---

### Task 36: Age indicator in UI

**Files:**
- Modify: `src/Engine.cpp` — drawUi()

- [ ] **Step 1:** Show current age name next to resources: "Dark Age", "Feudal Age", etc.
- [ ] **Step 2:** Commit

---

### Task 37: Game clock display

**Files:**
- Modify: `src/Engine.cpp` — drawUi()

- [ ] **Step 1:** Show elapsed game time (HH:MM:SS) in top bar
- [ ] **Step 2:** Commit

---

### Task 38: Tooltip/help text on hover

**Files:**
- Modify: `src/ui/ActionPanel.cpp`
- Modify: `src/Engine.cpp`

- [ ] **Step 1:** When hovering/long-pressing action button, show help text from LanguageManager
- [ ] **Step 2:** Commit

---

### Task 39: Cheat codes

**Files:**
- Modify: `src/Engine.cpp`

- [ ] **Step 1:** Implement text input for cheats:
  - "aegis" → instant build
  - "robin hood" → +10000 gold
  - "cheese steak jimmy's" → +10000 food
  - "lumberjack" → +10000 wood
  - "rock on" → +10000 stone
  - "marco" → reveal map
  - "polo" → remove fog
- [ ] **Step 2:** Commit

---

### Task 40: Taunts

**Files:**
- Modify: `src/audio/AudioPlayer.cpp`

- [ ] **Step 1:** Play taunt sounds from `sound/taunt/` directory (taunt1.mp3 through taunt42.mp3)
- [ ] **Step 2:** Trigger via chat input: type "1" through "42"
- [ ] **Step 3:** Commit

---

### Task 41: Defensive stance auto-return

**Files:**
- Modify: `src/mechanics/UnitActionHandler.cpp`

- [ ] **Step 1:** In Defensive stance, units chase enemies but return to original position after 5 tiles
- [ ] **Step 2:** Commit

---

### Task 42: Ballistics (missile leading)

**Files:**
- Modify: `src/mechanics/Missile.cpp`

- [ ] **Step 1:** If unit has Ballistics researched, missiles predict target movement
- [ ] **Step 2:** Calculate intercept point based on target velocity and missile speed
- [ ] **Step 3:** Commit

---

### Task 43: Pack/Unpack for siege (trebuchet)

**Files:**
- Modify: `src/ui/ActionPanel.cpp`
- Modify: `src/mechanics/Unit.cpp`

- [ ] **Step 1:** Pack button → change unit graphic to packed, disable attack
- [ ] **Step 2:** Unpack button → change to unpacked, enable attack with delay
- [ ] **Step 3:** Commit

---

### Task 44: Transport ship embark/disembark

**Files:**
- Modify: `src/ui/ActionPanel.cpp`
- Modify: `src/mechanics/UnitManager.cpp`

- [ ] **Step 1:** Right-click transport on shore → garrison nearby land units
- [ ] **Step 2:** Unload button → place units on nearest land tile
- [ ] **Step 3:** Commit

---

### Task 45: Flare signal

**Files:**
- Modify: `src/ui/ActionPanel.cpp`
- Modify: `src/Engine.cpp`

- [ ] **Step 1:** Flare button → click map position → show flare animation at that point
- [ ] **Step 2:** Commit

---

## Summary

| Phase | Tasks | Effort | What |
|-------|-------|--------|------|
| 1 | 1-3 | Small | Game speed, pause, hotkeys, shift-queue |
| 2 | 4-10 | Medium | Monks, trade, market, formations, garrison, gates |
| 3 | 11-14 | Medium-Large | Diplomacy, tech tree, settings, stats screens |
| 4 | 15-16 | Large | Save/Load game |
| 5 | 17-20 | Huge | AI opponents |
| 6 | 21-22 | Huge | Random map generation |
| 7 | 23-27 | Medium | Victory conditions and scoring |
| 8 | 28-45 | Small-Medium each | Music, sound, idle villager, select-all, accuracy, AOE, farms, cheats, taunts, etc. |

**Total: 45 tasks.** Full game implementation.

**NOT included (separate projects):**
- Multiplayer networking (needs server architecture)
- Scenario Editor (needs full editor UI)
- Replay system
- Advanced pathfinding rewrite (A* with collision)
