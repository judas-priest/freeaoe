# Module 7: Diplomacy UI

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Complete the diplomacy screen with working tribute and visual stance indicators.

**Architecture:** `DiplomacyScreen` already renders a panel with Ally/Neutral/Enemy buttons per player that are functional (DiplomacyScreen.cpp). `Player::setDiplomaticStance(uint8_t playerId, DiplomaticStance stance)` works (Player.h:205). Missing: tribute sending UI, current stance highlighting, and `CommandType::Tribute` handler in `GameState::executeCommands()` (GameState.cpp:355 — no Tribute case exists). `Player::sendTribute()` may exist — needs verification.

**Tech Stack:** C++20, modify 2 files.

---

### Task 1: Add Tribute command handler

**Files:**
- Modify: `src/mechanics/GameState.cpp:479` — add Tribute case

- [ ] **Step 1: Check if Player has sendTribute method**

Search Player.h/cpp for `sendTribute` or `tribute`. If it exists, use it. If not, implement inline:

```cpp
case CommandType::Tribute: {
    Player::Ptr sender = player(cmd.playerId);
    Player::Ptr receiver = player(cmd.targetId);
    if (!sender || !receiver) break;

    genie::ResourceType resType;
    switch (cmd.resourceType) {
        case 0: resType = genie::ResourceType::FoodStorage; break;
        case 1: resType = genie::ResourceType::WoodStorage; break;
        case 2: resType = genie::ResourceType::StoneStorage; break;
        case 3: resType = genie::ResourceType::GoldStorage; break;
        default: continue;
    }

    float amount = static_cast<float>(cmd.amount);
    if (sender->resourcesAvailable(resType) >= amount) {
        sender->removeResource(resType, amount);
        // 25% tribute fee (unless Coinage/Banking researched — skip for now)
        float received = amount * 0.75f;
        receiver->addResource(resType, received);
    }
    break;
}
```

- [ ] **Step 2: Build and commit**

```bash
git add src/mechanics/GameState.cpp
git commit -m "$(cat <<'EOF'
feat: tribute command handler with 25% tax
EOF
)"
```

---

### Task 2: Add tribute UI to DiplomacyScreen

**Files:**
- Modify: `src/ui/DiplomacyScreen.cpp` — add tribute amount + send button per player

- [ ] **Step 1: Read current DiplomacyScreen rendering**

Understand the layout. Add below each player's stance buttons: resource selector (Food/Wood/Stone/Gold), amount (100), and "Send" button.

- [ ] **Step 2: Implement tribute buttons**

Each player row gets 4 small resource buttons (F/W/S/G) and a "Send 100" button. On click, create GameCommand:

```cpp
GameCommand cmd;
cmd.type = CommandType::Tribute;
cmd.playerId = humanPlayerId;
cmd.targetId = targetPlayerId;
cmd.resourceType = selectedResourceType; // 0-3
cmd.amount = 100;
// dispatch via lockstep
```

- [ ] **Step 3: Highlight current stance button**

When rendering stance buttons, check `player->diplomaticStanceTo(targetId)` and highlight the active button (e.g., brighter color or border).

- [ ] **Step 4: Build, test, commit**

```bash
git add src/ui/DiplomacyScreen.h src/ui/DiplomacyScreen.cpp
git commit -m "$(cat <<'EOF'
feat: diplomacy tribute UI and stance highlighting
EOF
)"
```
