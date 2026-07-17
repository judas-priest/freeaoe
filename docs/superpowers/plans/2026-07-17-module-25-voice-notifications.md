# Module 25: Voice Notifications Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Play voice notifications for key game events: "Your town is under attack", "A wonder has been built", etc.

**Architecture:** Game events (unit under attack, research complete, age advance, wonder built) trigger voice sound playback via the existing `AudioPlayer`. Sound files are in the original AoE2 assets (taunt/notification .wav files in the sound DRS). Add cooldown per notification type to prevent spam.

**Tech Stack:** C++20, existing AudioPlayer, genie sound data

---

## Background

- `AudioPlayer` already supports `playSound(soundId, civId)` (`AudioPlayer.h:30`)
- AoE2 voice notifications are stored in sounds DRS with specific IDs
- Key notification sounds in AoE2:
  - "Your town is under attack" — plays when own building takes damage
  - "One of your villagers has finished building" — construction complete
  - "Research complete" — tech finished
  - "A wonder has been built" — any player builds wonder
  - Aging up notification
- Events already fire in the codebase (building damage, research complete, etc.)

## Key Files

- `src/audio/AudioPlayer.h:30` — playSound()
- `src/mechanics/Building.cpp` — Construction completion, research completion
- `src/mechanics/Unit.cpp` — Taking damage events
- `src/mechanics/Player.cpp` — Age advancement
- `src/mechanics/EventManager.h` — Event system (if exists)

---

### Task 1: Create notification manager

**Files:**
- Create: `src/audio/NotificationManager.h`
- Create: `src/audio/NotificationManager.cpp`

- [ ] **Step 1: Define notification types and cooldowns**

`src/audio/NotificationManager.h`:

```cpp
#pragma once

#include <cstdint>
#include <unordered_map>
#include <memory>

class AudioPlayer;

class NotificationManager
{
public:
    enum class Type {
        TownUnderAttack,
        VillagerBuiltComplete,
        ResearchComplete,
        AgeAdvance,
        WonderBuilt,
        UnitTrained
    };

    explicit NotificationManager(const std::shared_ptr<AudioPlayer> &audioPlayer);

    void notify(Type type, int civId = 0);
    void update(uint32_t time);

private:
    std::shared_ptr<AudioPlayer> m_audioPlayer;
    std::unordered_map<int, uint32_t> m_lastPlayed; // type -> last time
    uint32_t m_currentTime = 0;

    static constexpr uint32_t COOLDOWN_MS = 30000; // 30s between same notification
    static constexpr uint32_t ATTACK_COOLDOWN_MS = 20000; // 20s for attack warnings

    int soundIdFor(Type type) const;
    uint32_t cooldownFor(Type type) const;
};
```

- [ ] **Step 2: Implement notification manager**

`src/audio/NotificationManager.cpp`:

```cpp
#include "NotificationManager.h"
#include "AudioPlayer.h"

NotificationManager::NotificationManager(const std::shared_ptr<AudioPlayer> &audioPlayer)
    : m_audioPlayer(audioPlayer)
{
}

void NotificationManager::update(uint32_t time)
{
    m_currentTime = time;
}

void NotificationManager::notify(Type type, int civId)
{
    const int key = static_cast<int>(type);
    const uint32_t cooldown = cooldownFor(type);

    if (m_lastPlayed.count(key) && (m_currentTime - m_lastPlayed[key]) < cooldown) {
        return; // On cooldown
    }

    const int soundId = soundIdFor(type);
    if (soundId > 0 && m_audioPlayer) {
        m_audioPlayer->playSound(soundId, civId);
        m_lastPlayed[key] = m_currentTime;
    }
}

int NotificationManager::soundIdFor(Type type) const
{
    // AoE2 HD notification sound IDs — verify against actual DRS content
    switch (type) {
    case Type::TownUnderAttack: return 5765; // "Your town is under attack"
    case Type::ResearchComplete: return 5316; // "Research complete"
    case Type::AgeAdvance: return 5318; // Age advance fanfare
    case Type::WonderBuilt: return 5319; // "A wonder has been built"
    case Type::UnitTrained: return -1; // No notification for unit training
    case Type::VillagerBuiltComplete: return -1; // No notification
    }
    return -1;
}

uint32_t NotificationManager::cooldownFor(Type type) const
{
    if (type == Type::TownUnderAttack) return ATTACK_COOLDOWN_MS;
    return COOLDOWN_MS;
}
```

Note: Sound IDs (5765, 5316, etc.) are examples — verify against actual AoE2 HD sound DRS. The IDs may differ by data version.

- [ ] **Step 3: Build**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/audio/NotificationManager.h src/audio/NotificationManager.cpp
git commit -m "feat: add NotificationManager with cooldown-based voice alerts"
```

---

### Task 2: Wire notifications to game events

**Files:**
- Modify: `src/mechanics/Unit.cpp` — attack notification
- Modify: `src/mechanics/Building.cpp` — research complete, construction complete
- Modify: `src/mechanics/Player.cpp` — age advance
- Modify: `src/Engine.cpp` — create and update NotificationManager

- [ ] **Step 1: Create NotificationManager in Engine**

In `Engine.h`, add member:

```cpp
#include "audio/NotificationManager.h"
std::unique_ptr<NotificationManager> m_notifications;
```

In Engine initialization:

```cpp
m_notifications = std::make_unique<NotificationManager>(m_audioPlayer);
```

In Engine update loop:

```cpp
m_notifications->update(currentTime);
```

- [ ] **Step 2: Fire attack notification**

In `Unit.cpp`, in the method where a unit takes damage (look for `m_damageTaken` modification or `takeDamage()`), add:

```cpp
// Only notify for human player's units
if (isBuilding() && player() == humanPlayer) {
    notifications->notify(NotificationManager::Type::TownUnderAttack, player()->civId());
}
```

The exact wiring depends on how the notification manager reference reaches Unit. Options: pass via Player, or use a global/singleton. The simplest approach is to add a static or pass through the existing event system.

- [ ] **Step 3: Fire research complete notification**

In `Building.cpp`, where research completes (in `update()` when production finishes a tech):

```cpp
if (isOwnedByHuman) {
    notifications->notify(NotificationManager::Type::ResearchComplete, player->civId());
}
```

- [ ] **Step 4: Fire age advance notification**

In `Player.cpp`, at the end of `setAge()`:

```cpp
if (this == humanPlayer) {
    notifications->notify(NotificationManager::Type::AgeAdvance, civilization.id());
}
```

- [ ] **Step 5: Build and test**

```bash
cd build && make -j$(nproc)
```

Play a game, research a tech — verify voice plays. Get attacked — verify "town under attack" plays (once per 20s max).

- [ ] **Step 6: Commit**

```bash
git add src/Engine.h src/Engine.cpp src/mechanics/Unit.cpp src/mechanics/Building.cpp src/mechanics/Player.cpp src/audio/NotificationManager.h src/audio/NotificationManager.cpp
git commit -m "feat: voice notifications for attack, research, age advance"
```
