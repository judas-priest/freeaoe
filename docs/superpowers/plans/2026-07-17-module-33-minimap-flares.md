# Module 33: Minimap Flares/Pings — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Players can send signal flares on the minimap (Alt+click or dedicated key), visible to allies as a flashing beacon — matching AoE2's flare mechanic for multiplayer communication.

**Architecture:** Add a flare event to the multiplayer command system. When a player Alt+clicks the minimap (or presses the flare hotkey + clicks map), broadcast a FlareSignal to all allies. The Minimap renders active flares as pulsing circles that fade over 5 seconds.

**Tech Stack:** C++, SDL2, multiplayer sync

---

### Task 1: Add flare data structure and rendering

**Files:**
- Modify: `src/ui/Minimap.h` (add flare storage and rendering)
- Modify: `src/ui/Minimap.cpp` (draw flares, manage lifetime)

- [ ] **Step 1: Add flare struct and storage to Minimap.h**

In `Minimap.h`, add inside the class:
```cpp
struct Flare {
    MapPos position;
    int playerId = 0;
    float timeLeft = 5000.f; // milliseconds
};
std::vector<Flare> m_flares;
```

Add public method:
```cpp
void addFlare(const MapPos &position, int playerId);
```

- [ ] **Step 2: Implement addFlare and flare rendering in Minimap.cpp**

```cpp
void Minimap::addFlare(const MapPos &position, int playerId)
{
    m_flares.push_back({position, playerId, 5000.f});
}
```

In `Minimap::update()` or `Minimap::draw()`, after drawing units texture, add flare rendering:

```cpp
// Draw active flares
for (auto it = m_flares.begin(); it != m_flares.end(); ) {
    it->timeLeft -= elapsed; // elapsed = time since last frame in ms
    if (it->timeLeft <= 0) {
        it = m_flares.erase(it);
        continue;
    }

    // Convert map position to minimap screen position
    const ScreenPos flareScreen = mapPosToMinimapPos(it->position);

    // Pulsing effect: radius oscillates 4-8 pixels over 500ms cycle
    const float pulse = 4.f + 4.f * std::abs(std::sin(it->timeLeft * 0.006f));
    const float alpha = std::min(it->timeLeft / 1000.f, 1.f); // fade out in last second

    // Draw as bright circle (yellow/white)
    Drawable::Color flareColor(255, 255, 0, static_cast<uint8_t>(alpha * 255));
    m_renderTarget->draw(Drawable::Circle(flareScreen, pulse, flareColor));

    ++it;
}
```

Note: `mapPosToMinimapPos` may not exist — you'll need to write the coordinate conversion using the same logic as the existing unit-to-minimap mapping in `updateUnits()`. Look for how unit positions are converted to minimap coordinates and extract that logic.

- [ ] **Step 3: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
Expected: Clean compilation.

- [ ] **Step 4: Commit**

```bash
git add src/ui/Minimap.h src/ui/Minimap.cpp
git commit -m "feat: minimap flare rendering with pulse and fade"
```

---

### Task 2: Add flare input handling

**Files:**
- Modify: `src/Engine.h` (add flare mode state)
- Modify: `src/Engine.cpp` (handle Alt+click on minimap)
- Modify: `src/ui/Minimap.h` (expose coordinate conversion)
- Modify: `src/ui/Minimap.cpp` (handle Alt+click event)

- [ ] **Step 1: Handle Alt+click in Minimap event handler**

In `Minimap::handleEvent()`, detect Alt modifier during click:

```cpp
bool Minimap::handleEvent(input::Event event)
{
    // ... existing code checks if click is within minimap rect ...

    if (event.type == input::EventType::MouseButtonPressed) {
        // Check Alt modifier for flare
        if (SDL_GetModState() & KMOD_ALT) {
            MapPos mapPos = minimapPosToMapPos(screenPos); // convert click to map coordinates
            addFlare(mapPos, m_humanPlayerId);

            // Notify engine to broadcast flare in multiplayer
            if (m_onFlare) {
                m_onFlare(mapPos);
            }
            return true;
        }
        // ... existing camera-move logic ...
    }
}
```

Add callback in `Minimap.h`:
```cpp
std::function<void(const MapPos&)> m_onFlare;
void setFlareCallback(std::function<void(const MapPos&)> callback) { m_onFlare = callback; }
```

- [ ] **Step 2: Connect flare callback in Engine**

In `Engine.cpp` where the Minimap is created/initialized, set the callback:

```cpp
m_minimap->setFlareCallback([this, &state](const MapPos &pos) {
    // In multiplayer, broadcast flare to allies
    // For now, just add local flare (single-player visible feedback)
    // TODO: integrate with LockstepManager for multiplayer sync
});
```

- [ ] **Step 3: Add notification sound for incoming flares**

When a flare is received (from ally in multiplayer), play the standard notification:
```cpp
AudioPlayer::instance().playSound(20 /*flare sound ID*/, civilization);
```

The exact sound ID needs to be looked up in the .dat file — use any suitable alert sound.

- [ ] **Step 4: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
Expected: Clean compilation.

- [ ] **Step 5: Manual test**

1. Start a game
2. Hold Alt and click on the minimap
3. A pulsing yellow circle should appear at the click location
4. It should fade out over 5 seconds

- [ ] **Step 6: Commit**

```bash
git add src/ui/Minimap.h src/ui/Minimap.cpp src/Engine.h src/Engine.cpp
git commit -m "feat: Alt+click minimap to send flare signal"
```
