# Module 3: Market Buy/Sell UI

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow human players to buy/sell Food, Wood, Stone at the Market building using gold, with dynamic pricing.

**Architecture:** Market prices fully implemented in `Player::MarketPrices` (Player.h:154-163) with `buyPrice()`, `sellPrice()`, `onBuy()`, `onSell()`. `CommandType::BuyResource` (GameCommand.h:20) and `CommandType::SellResource` (GameCommand.h:21) are defined but have NO handler in `GameState::executeCommands()` (GameState.cpp:355). Need to: (1) add command handlers, (2) show buy/sell buttons in ActionPanel when Market is selected.

**Tech Stack:** C++20, modify 2 existing files.

---

### Task 1: Add BuyResource/SellResource command handlers

**Files:**
- Modify: `src/mechanics/GameState.cpp:479` — add cases before `default:`

- [ ] **Step 1: Add BuyResource handler**

Insert before the `default:` case in `GameState::executeCommands()`:

```cpp
case CommandType::BuyResource: {
    Player::Ptr owner = player(cmd.playerId);
    if (!owner) break;
    int resType = cmd.resourceType; // 0=Food, 1=Wood, 2=Stone
    if (resType < 0 || resType > 2) break;

    int buyPrice = owner->marketPrices.buyPrice(resType);
    float goldAvailable = owner->resourcesAvailable(genie::ResourceType::GoldStorage);

    if (goldAvailable >= buyPrice) {
        owner->removeResource(genie::ResourceType::GoldStorage, buyPrice);
        genie::ResourceType targetRes;
        switch (resType) {
            case 0: targetRes = genie::ResourceType::FoodStorage; break;
            case 1: targetRes = genie::ResourceType::WoodStorage; break;
            case 2: targetRes = genie::ResourceType::StoneStorage; break;
        }
        owner->addResource(targetRes, 100);
        owner->marketPrices.onBuy(resType);
    }
    break;
}
```

- [ ] **Step 2: Add SellResource handler**

```cpp
case CommandType::SellResource: {
    Player::Ptr owner = player(cmd.playerId);
    if (!owner) break;
    int resType = cmd.resourceType;
    if (resType < 0 || resType > 2) break;

    genie::ResourceType sourceRes;
    switch (resType) {
        case 0: sourceRes = genie::ResourceType::FoodStorage; break;
        case 1: sourceRes = genie::ResourceType::WoodStorage; break;
        case 2: sourceRes = genie::ResourceType::StoneStorage; break;
    }

    float available = owner->resourcesAvailable(sourceRes);
    if (available >= 100) {
        owner->removeResource(sourceRes, 100);
        int sellPrice = owner->marketPrices.sellPrice(resType);
        owner->addResource(genie::ResourceType::GoldStorage, sellPrice);
        owner->marketPrices.onSell(resType);
    }
    break;
}
```

- [ ] **Step 3: Build**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/GameState.cpp
git commit -m "$(cat <<'EOF'
feat: market buy/sell command handlers with dynamic pricing
EOF
)"
```

---

### Task 2: Show buy/sell buttons when Market is selected

**Files:**
- Modify: `src/ui/ActionPanel.cpp` — add market trading buttons

- [ ] **Step 1: Find where building-specific buttons are rendered in ActionPanel**

Search for how ActionPanel decides which buttons to show based on selected building type. When a Market (ID 84 = `Unit::HardcodedTypes::Market`) is selected, add buy/sell buttons.

- [ ] **Step 2: Add 6 market buttons**

Use the existing Command enum values: `BuyFood=22`, `SellFood=24`. Check if BuyWood/BuyStone/SellWood/SellStone enums exist; if not, add them to the Command enum.

Each button should show the current price. The buy buttons show the gold cost (from `player->marketPrices.buyPrice(resType)`), sell buttons show gold gain (from `sellPrice(resType)`).

- [ ] **Step 3: Wire button clicks to GameCommand dispatch**

When a buy/sell button is clicked, create a GameCommand:

```cpp
GameCommand cmd;
cmd.type = (isBuy ? CommandType::BuyResource : CommandType::SellResource);
cmd.playerId = humanPlayerId;
cmd.resourceType = resIndex; // 0=food, 1=wood, 2=stone
// dispatch via lockstep manager
```

- [ ] **Step 4: Build, test on device, commit**

Select Market building → buy/sell buttons appear with prices. Buy 100 food → gold decreases, food increases, buy price goes up by 3.

```bash
git add src/ui/ActionPanel.cpp
git commit -m "$(cat <<'EOF'
feat: market buy/sell buttons with dynamic price display
EOF
)"
```
