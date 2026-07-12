# freeaoe — Feature Status (Android Port)

Last updated: 2026-07-12

## Working

| Feature | Notes |
|---------|-------|
| Map rendering | Tiles, terrain, elevation |
| Unit rendering | Sprites, animations, player colors |
| Unit selection | Tap to select, tap another to switch |
| Unit movement | Double-tap = right click (move/attack) |
| Camera drag | Touch drag to scroll |
| Pinch zoom | Two-finger zoom in/out |
| Building placement | Place buildings, walls, validate terrain |
| Building construction | Progress bar, villager builds |
| Unit training | Production queue in buildings |
| Technology research | Research queue, tech effects applied |
| Resource gathering | Wood, food, gold, stone. Carry capacity, drop-off |
| Combat | Melee and ranged attacks, projectiles, HP |
| Fog of war | Unexplored/explored/visible, doppelgangers |
| Player colors & diplomacy | Allied/neutral/enemy stances |
| Sound & music | WAV, MP3, MIDI via miniaudio |
| Scenario loading | SCN/SCX files from AoE2 HD data |
| Scenario triggers | Partial — 7 conditions, most common effects |
| Russian language | Cyrillic fonts, correct string loading order |
| Touch state machine | Clean single-handler input (no dual events) |
| Game menu dialog | TextButton rendering for SDL2, Quit/Cancel work |
| Top bar buttons | GameMenu button opens dialog |

## Not Working — Critical

| Feature | Status | Where |
|---------|--------|-------|
| Home Screen (SDL2) | SFML-only, skipped on Android | `HomeScreen.cpp` (not ported) |
| Campaign/scenario browser | Hardcoded to one scenario, no UI | `main.cpp:167-193` |
| Save/Load game | Not implemented at all | `Dialog.cpp:18` — "(TODO)" |
| AI players | `update()` doesn't evaluate rules, 25+ stub conditions | `AiScript.cpp:28`, `ScriptLoader.cpp` |
| Random map generation | No implementation, only scenarios or debug maps | `Map.cpp` |

## Not Working — Important

| Feature | Status | Where |
|---------|--------|-------|
| Diplo button | Clicks but no handler | `Engine.cpp` — no handler for `IconButton::Diplo` |
| Chat button | Clicks but no handler | same |
| Tech Tree button | Clicks but no handler | same |
| Settings button | Clicks but no handler | same |
| Dialog: Achievements | "(TODO)", no action | `Dialog.cpp:17` |
| Dialog: Save | "(TODO)", no action | `Dialog.cpp:18` |
| Dialog: Options | "(TODO)", no action | `Dialog.cpp:19` |
| Dialog: About | "(TODO)", no action | `Dialog.cpp:20` |
| Patrol command | Button shown, no handler | `ActionPanel.cpp:570-585` |
| Guard command | Button shown, no handler | same |
| Follow command | Button shown, no handler | same |
| Repair command | Button shown, no handler | `ActionPanel.cpp:424` |
| Monk healing | No ActionHeal | — |
| Monk conversion | No ActionConvert | — |
| Trade routes | No ActionTrade, no market UI | — |
| Relic mechanics | Monks can't pick up/deposit relics | — |
| Formations | Commands defined, no logic | `ActionPanel.cpp:269-283` |
| Market buy/sell | Commands defined, no UI | `ActionPanel.cpp:257-264` |
| Victory: Standard/Score/Timed | TODO warnings | `ScenarioController.cpp:143-168` |
| Alliance win | Commented out | `GameState.cpp:184-198` |
| Player defeated | Empty handler | `ScenarioController.cpp:725-729` |
| Tech: DisableTech | Not implemented | `Player.cpp:157-167` |
| Tech: TechCostModifier | Not implemented | same |
| Tech: TechTimeModifier | Not implemented | same |
| Map Editor | Stub: "Not implemented, click to exit" | `Editor.cpp:27-72` |
| Multiplayer | Skeleton only, no networking | `GameServer.cpp`, `GameClient.cpp` |

## Not Working — Minor

| Feature | Status | Where |
|---------|--------|-------|
| Elevation damage bonus | TODO comment | `ActionAttack.cpp:178` |
| Projectile spread area | Commented out | `ActionAttack.cpp:193-227` |
| Attack flare | TODO comment | `ActionAttack.cpp:168` |
| Ungarrison positioning | Uses old position | `Building.cpp:56` |
| Gate open/close | No implementation | `ActionPanel.cpp:275-276` |
| Town Bell | No implementation | `ActionPanel.cpp:284-285` |
| Corpse visibility | TODO comment | `UnitManager.cpp:198` |
| Age advancement deps | TODO: need to recurse | `Player.cpp:195` |

## Trigger System Coverage

### Implemented Conditions
- OwnObjects, OwnFewerObjects, ObjectSelected, ObjectsInArea
- Timer, DestroyObject, AccumulateAttribute

### Missing Conditions
- BringObjectToArea (TODO), PlayerDefeated (empty handler), and others

### Implemented Effects
- Activate/deactivate trigger, display instructions
- Create/remove/task/damage objects
- Change diplomacy, declare victory, research tech

### Missing Effects
- ChangeObjectName (TODO), several others fall through to warning

## Android-Specific

| Feature | Status |
|---------|--------|
| Touch input state machine | Working — Idle→Pending→Dragging/WaitSecondTap |
| SDL_HINT_TOUCH_MOUSE_EVENTS | Disabled ("0"), touch handler owns all input |
| Pinch zoom | Working — SDL_RenderCopy crop, no viewport change |
| TextButton SDL2 rendering | Working — beveled borders, centered text |
| Dialog touch events | Working — TouchBegan/TouchEnded handled |
| Cyrillic fonts | Charis v7 + Alegreya Bold (full Cyrillic) |
| String load order | EN first, then RU on top |

## Priority Roadmap (Mobile)

1. **Scenario browser** — pick campaign → pick mission → play
2. **Patrol/Guard commands** — army management
3. **Save/Load** — phone interruptions need save support
4. AI — only needed for non-campaign games (campaigns use triggers)
