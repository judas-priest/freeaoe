# freeaoe — Feature Status (Android Port)

Last updated: 2026-07-12 (end of day)

## What We Fixed Today (25 commits)

| Fix | Status |
|-----|--------|
| Touch input: state machine (Idle→Pending→Dragging/WaitSecondTap) | DONE ✓ |
| Double-tap = right click with deferred deselect | DONE ✓ |
| Cyrillic fonts (Charis v7 + Alegreya Bold) | DONE ✓ |
| String load order (EN first, RU on top) | DONE ✓ |
| TextButton SDL2 rendering (menu dialog visible) | DONE ✓ |
| Dialog touch event handling | DONE ✓ |
| Pinch zoom: remove camera viewport shift (was double-zoom) | DONE ✓ |
| Scenario browser: scan campaigns, pick mission | DONE ✓ (but crashes — see below) |
| Terrain PNG loading: add terrain/ paths, fix m_isPng fallback | DONE ✓ (but still black screen) |
| Audio: stop MP3 looping, prevent duplicate streams | DONE ✓ |
| Trigger: reset looping timer conditions after firing | DONE ✓ |
| stdout/stderr redirect to Android logcat | DONE ✓ |

## Known Bugs — BLOCKING

| Bug | Root Cause | Fix Needed |
|-----|-----------|------------|
| **Crash: "Android only supports one window"** | ScenarioBrowser creates SdlWindow, then Engine creates another. Android SDL2 allows only one | Refactor: one SdlWindow for entire app lifecycle. ScenarioBrowser renders into Engine's window, or Engine exposes window before setup() |
| **Black screen on campaign scenarios** | PNG terrain textures found but MapRenderer::updateTexture() may not handle PNG tiles correctly. m_tileSquareCount not set for PNG terrain (requires m_slp) | Fix TerrainSprite to set m_tileSquareCount from PNG dimensions. May need MapRenderer PNG rendering path |
| **Scenario text in English** | Scenario dialogue strings (Bertrand, Joan) come from .scn file embedded text, not LanguageManager. Language in scenario file is set at creation time | Need to load localized .scn from language-specific campaign dir (resources/ru/campaign/) if it exists |
| **Audio overlap** | Multiple DisplayInstructions triggers fire close together, playing sounds simultaneously | Queue audio — play next sound after current finishes, or add delay between |

## Working

| Feature | Notes |
|---------|-------|
| Map rendering (demo map) | Hardcoded tiles work. Campaign terrain needs PNG fix |
| Unit rendering | Sprites, animations, player colors |
| Unit selection | Tap to select, tap another to switch, deferred deselect |
| Unit movement | Double-tap = right click (move/attack) |
| Camera drag | Touch drag to scroll |
| Pinch zoom | Two-finger zoom, SDL_RenderCopy crop only |
| Building placement | Place buildings, walls, validate terrain |
| Building construction | Progress bar, villager builds |
| Unit training | Production queue in buildings |
| Technology research | Research queue, tech effects applied |
| Resource gathering | Wood, food, gold, stone |
| Combat | Melee and ranged attacks, projectiles, HP |
| Fog of war | Unexplored/explored/visible, doppelgangers |
| Player colors & diplomacy | Allied/neutral/enemy stances |
| Sound & music | WAV, MP3, MIDI via miniaudio |
| Scenario loading | SCN/SCX files parse correctly |
| Scenario triggers | 7 conditions, most effects. Looping timers fixed |
| Russian language | Cyrillic fonts, correct string loading. Unit names in Russian ✓ |
| Touch state machine | Idle→Pending→Dragging/WaitSecondTap |
| Game menu dialog | TextButton renders, Quit/Cancel work |
| Scenario browser | Finds 9 campaigns, shows list, loads scenario (crashes on window) |
| Logcat integration | All stdout/stderr → adb logcat -s FreeAoE |

## Not Implemented — Critical

| Feature | Status | Effort |
|---------|--------|--------|
| Save/Load game | Zero implementation | HIGH — serialize GameState |
| AI players | update() empty, 25+ stub conditions | HUGE — months of work |
| Random map generation | No implementation | HUGE — need RMS parser |

## Not Implemented — Important

| Feature | Status |
|---------|--------|
| Diplo/Chat/TechTree/Settings buttons | Click but no handler |
| Dialog: Save/Options/Achievements/About | "(TODO)" labels |
| Patrol/Guard/Follow/Repair commands | Buttons shown, no handler |
| Monk healing/conversion | No ActionHeal/ActionConvert |
| Trade routes, relics, market | No implementation |
| Formations | Commands defined, no logic |
| Victory modes (Standard/Score/Timed) | TODO warnings |
| Player defeated handling | Empty stub |
| Map Editor | "Not implemented, click to exit" |
| Multiplayer | Skeleton only |

## Not Implemented — Minor

| Feature | Status |
|---------|--------|
| Elevation damage bonus | TODO |
| Projectile spread | Commented out |
| Gate open/close, Town Bell | No implementation |
| Ungarrison positioning | Uses old position |

## Trigger System

### Implemented Conditions (7)
OwnObjects, OwnFewerObjects, ObjectSelected, ObjectsInArea, Timer, DestroyObject, AccumulateAttribute

### Missing Conditions
BringObjectToArea (TODO), PlayerDefeated (empty handler), others

### Implemented Effects
Activate/deactivate trigger, display instructions, play sound, create/remove/task/damage objects, change diplomacy, declare victory, research tech, change view

### Missing Effects
ChangeObjectName (TODO), several fall through to warning

## MVP Roadmap

### Phase 1: Make campaigns playable (CURRENT)
1. Fix crash: single SdlWindow lifecycle
2. Fix terrain PNG rendering
3. Fix scenario language (load from resources/ru/)
4. Audio queue (don't overlap dialogues)

### Phase 2: Army management
5. Patrol command
6. Guard command

### Phase 3: Quality of life
7. Save/Load (minimal: resources + units + map)
8. Tech Tree display
9. Diplomacy screen
