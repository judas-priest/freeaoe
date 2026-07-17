# Module 22: Shared Vision Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allied players share fog of war — what one ally sees, all allies see.

**Architecture:** When rendering fog, merge visibility from all allied players. The per-player `VisibilityMap` stays independent (each player tracks their own units' LOS). At render time, a tile is "Visible" if ANY allied player has it visible, "Explored" if any ally has explored it.

**Tech Stack:** C++20, existing `VisibilityMap` system, `MapRenderer`

---

## Background

- Each player owns a `std::shared_ptr<VisibilityMap> visibility` (`Player.h:151`)
- `VisibilityMap` stores per-tile int counters: `Unexplored` (INT_MIN), `Explored` (0), `Visible` (>0)
- `MapRenderer::setVisibilityMap()` takes one player's map for rendering (`MapRenderer.h:50`)
- Fog rendering happens in `MapRenderer.cpp:140-146`
- Allied status checked via `Player::isAllied(playerId)` (`Player.h:208`)

## Key Files

- `src/mechanics/Player.h:26-120` — VisibilityMap class
- `src/render/MapRenderer.h:50,66` — setVisibilityMap, m_visibilityMap
- `src/render/MapRenderer.cpp` — fog rendering logic
- `src/mechanics/GameState.h` — holds all players

---

### Task 1: Add ally visibility query to VisibilityMap

**Files:**
- Modify: `src/mechanics/Player.h`

- [ ] **Step 1: Add method to check merged ally visibility**

Add a free function or static method that checks visibility across allies. The simplest approach: add a method to `Player` that checks if a tile is visible to any ally.

In `Player.h`, add to the `Player` class (public section):

```cpp
/// Check if tile is visible to this player or any ally
VisibilityMap::Visibility teamVisibilityAt(const int tileX, const int tileY, const std::vector<std::shared_ptr<Player>> &allPlayers) const;
```

- [ ] **Step 2: Implement in Player.cpp**

```cpp
VisibilityMap::Visibility Player::teamVisibilityAt(const int tileX, const int tileY, const std::vector<std::shared_ptr<Player>> &allPlayers) const
{
    // Start with our own visibility
    VisibilityMap::Visibility best = visibility->visibilityAt(tileX, tileY);
    if (best > VisibilityMap::Explored) {
        return best; // Already fully visible to us
    }

    // Check allies
    for (const auto &other : allPlayers) {
        if (!other || other.get() == this) continue;
        if (!isAllied(other->playerId)) continue;

        VisibilityMap::Visibility allyVis = other->visibility->visibilityAt(tileX, tileY);
        if (allyVis > best) {
            best = allyVis;
            if (best > VisibilityMap::Explored) {
                return best; // Visible — no need to check more allies
            }
        }
    }
    return best;
}
```

- [ ] **Step 3: Build**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/Player.h src/mechanics/Player.cpp
git commit -m "feat: add Player::teamVisibilityAt() for shared vision query"
```

---

### Task 2: Use shared vision in MapRenderer

**Files:**
- Modify: `src/render/MapRenderer.h`
- Modify: `src/render/MapRenderer.cpp`

- [ ] **Step 1: Add player list reference to MapRenderer**

In `MapRenderer.h`, add a member to hold reference to all players:

```cpp
void setAllPlayers(const std::vector<std::shared_ptr<Player>> &players);
```

```cpp
std::vector<std::shared_ptr<Player>> m_allPlayers;
std::shared_ptr<Player> m_humanPlayer;
```

- [ ] **Step 2: Implement setAllPlayers**

In `MapRenderer.cpp`:

```cpp
void MapRenderer::setAllPlayers(const std::vector<std::shared_ptr<Player>> &players)
{
    m_allPlayers = players;
}
```

- [ ] **Step 3: Replace visibility checks in fog rendering**

Find the fog rendering code in `MapRenderer.cpp` (around line 140-146). Where it currently calls `m_visibilityMap->visibilityAt(tileX, tileY)`, replace with the team visibility check:

```cpp
VisibilityMap::Visibility vis;
if (m_humanPlayer && !m_allPlayers.empty()) {
    vis = m_humanPlayer->teamVisibilityAt(tileX, tileY, m_allPlayers);
} else {
    vis = m_visibilityMap->visibilityAt(tileX, tileY);
}
```

- [ ] **Step 4: Wire up in Engine.cpp**

Find where `MapRenderer` is initialized and `setVisibilityMap()` is called. Add a call to `setAllPlayers()` and `setHumanPlayer()` with the game's player list.

```cpp
m_mapRenderer->setAllPlayers(gameState->players());
m_mapRenderer->setHumanPlayer(humanPlayer);
```

- [ ] **Step 5: Build and test**

```bash
cd build && make -j$(nproc)
```

Start a game with an AI ally. Verify you can see what your ally's units see on the map (their explored area visible to you).

- [ ] **Step 6: Commit**

```bash
git add src/render/MapRenderer.h src/render/MapRenderer.cpp src/Engine.cpp
git commit -m "feat: shared vision — allies share fog of war"
```

---

### Task 3: Update minimap to use shared vision

**Files:**
- Modify: `src/ui/Minimap.h`
- Modify: `src/ui/Minimap.cpp`

- [ ] **Step 1: Add shared vision to minimap rendering**

The minimap also renders fog of war via `setVisibilityMap()`. Apply the same team visibility pattern.

Add player list member and setter to `Minimap`:

```cpp
void setAllPlayers(const std::vector<std::shared_ptr<Player>> &players);
std::shared_ptr<Player> m_humanPlayer;
std::vector<std::shared_ptr<Player>> m_allPlayers;
```

- [ ] **Step 2: Replace visibility checks in minimap**

Where minimap checks `m_visibilityMap->visibilityAt()`, use the same team query:

```cpp
auto vis = m_humanPlayer
    ? m_humanPlayer->teamVisibilityAt(tx, ty, m_allPlayers)
    : m_visibilityMap->visibilityAt(tx, ty);
```

- [ ] **Step 3: Wire up in Engine.cpp**

```cpp
m_minimap->setAllPlayers(gameState->players());
m_minimap->setHumanPlayer(humanPlayer);
```

- [ ] **Step 4: Build and test**

```bash
cd build && make -j$(nproc)
```

Verify minimap shows ally-explored areas.

- [ ] **Step 5: Commit**

```bash
git add src/ui/Minimap.h src/ui/Minimap.cpp src/Engine.cpp
git commit -m "feat: shared vision on minimap for allied players"
```
