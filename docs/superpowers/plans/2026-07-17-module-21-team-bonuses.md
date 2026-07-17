# Module 21: Team Bonuses Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Apply civilization team bonuses to all allied players when a game starts or diplomacy changes.

**Architecture:** Each civilization has a `TeamBonusID` in genie data that references a tech effect. When a player's team is set up, iterate all allies and apply each ally's team bonus effect to this player. Re-apply on diplomacy changes.

**Tech Stack:** C++20, genie::Civ data, existing `Player::applyTechEffect()` pipeline

---

## Background

- `genie::Civ::TeamBonusID` (field in `src/extern/genieutils/include/genie/dat/Civ.h:48`) stores the effect ID for each civ's team bonus
- `Player.cpp:55` already logs `TeamBonusID` but never applies it
- Team bonuses in AoE2 apply to ALL allies (including self) when playing on a team
- The effect is applied once via `Player::applyTechEffect(effectId)` — same pipeline as age/research effects
- Examples: Britons +20% archery range LOS, Teutons +2 unit LoS for relics, etc.

## Key Files

- `src/mechanics/Player.h` — Player class with `applyTechEffect()`, diplomacy
- `src/mechanics/Player.cpp` — Constructor (line 21-58), `applyTechEffect()` (line 117-137)
- `src/mechanics/Civilization.h` — `teamBonusId()` accessor needed
- `src/mechanics/Civilization.cpp` — `applyData()` (line 177-234)
- `src/mechanics/GameState.h` / `GameState.cpp` — Holds all players, game init

---

### Task 1: Add teamBonusId accessor to Civilization

**Files:**
- Modify: `src/mechanics/Civilization.h`
- Modify: `src/mechanics/Civilization.cpp`

- [ ] **Step 1: Add accessor to Civilization.h**

Add after the existing `id()` accessor:

```cpp
int16_t teamBonusId() const noexcept { return m_teamBonusId; }
```

Add member variable in private section:

```cpp
int16_t m_teamBonusId = -1;
```

- [ ] **Step 2: Store TeamBonusID in applyData()**

In `Civilization.cpp`, inside `applyData()` (around line 185), add:

```cpp
m_teamBonusId = data.TeamBonusID;
```

- [ ] **Step 3: Build and verify**

```bash
cd build && make -j$(nproc)
```

Expected: Compiles clean.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/Civilization.h src/mechanics/Civilization.cpp
git commit -m "feat: expose Civilization::teamBonusId() accessor"
```

---

### Task 2: Apply team bonuses during game initialization

**Files:**
- Modify: `src/mechanics/GameState.cpp`

- [ ] **Step 1: Find game init where players are set up**

In `GameState.cpp`, find where players are created and diplomacy is established (typically after map generation, when all players exist with their civs and teams set).

- [ ] **Step 2: Add applyTeamBonuses() call**

After all players are initialized and diplomacy is set, add a loop:

```cpp
// Apply team bonuses to all allied players
for (auto &player : m_players) {
    if (!player) continue;
    const int16_t bonusId = player->civilization.teamBonusId();
    if (bonusId == -1) continue;

    // Apply to self
    player->applyTechEffect(bonusId);

    // Apply to allies
    for (auto &ally : m_players) {
        if (!ally || ally == player) continue;
        if (player->diplomaticStanceTo(ally->playerId) == Player::DiplomaticStance::Allied) {
            ally->applyTechEffect(bonusId);
        }
    }
}
```

- [ ] **Step 3: Build and verify**

```bash
cd build && make -j$(nproc)
```

Expected: Compiles clean.

- [ ] **Step 4: Test in-game**

Start a random map game with 2 allied players (e.g., Britons + Teutons). Verify that team bonus effects from genie data are applied (check unit stats affected by team bonuses).

- [ ] **Step 5: Commit**

```bash
git add src/mechanics/GameState.cpp
git commit -m "feat: apply civilization team bonuses to allied players"
```

---

### Task 3: Re-apply team bonuses on diplomacy change

**Files:**
- Modify: `src/mechanics/Player.cpp`

- [ ] **Step 1: Find diplomacy change handler**

In `Player.cpp`, find `setDiplomaticStance()` method.

- [ ] **Step 2: Apply/remove team bonus on alliance change**

When a player becomes allied, apply their team bonus to the new ally (and vice versa). Note: `applyTechEffect()` already guards against double-application via `m_activeTechs.count(effectId)` check at line 125.

After the stance is set in `setDiplomaticStance()`:

```cpp
// Apply team bonuses when becoming allies
if (stance == DiplomaticStance::Allied) {
    // Apply our team bonus to new ally
    const int16_t ourBonus = civilization.teamBonusId();
    if (ourBonus != -1 && otherPlayer) {
        otherPlayer->applyTechEffect(ourBonus);
    }
    // Apply their team bonus to us
    const int16_t theirBonus = otherPlayer->civilization.teamBonusId();
    if (theirBonus != -1) {
        applyTechEffect(theirBonus);
    }
}
```

Note: Removing team bonuses when un-allying is complex (would need effect reversal). AoE2 doesn't support mid-game un-allying team bonuses either — once applied, they stay. So this is correct behavior.

- [ ] **Step 3: Build and test**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/Player.cpp
git commit -m "feat: apply team bonuses on diplomacy alliance change"
```
