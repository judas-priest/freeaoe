# Economy & Gathering Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete economy -- trade cogs, fish traps, farm queue, villager auto-return after building, shore fish verification

**Architecture:**
- `ActionTrade` (`src/actions/ActionTrade.cpp`) handles trade cart logic: bounce between home position and target market, deposit gold using AoE2 distance formula. However, it is **never wired into the task system** -- `IAction::assignTask()` has no `case genie::ActionType::Trade`, and `ActionTrade.h` is not included anywhere except its own `.cpp`. Trade carts currently cannot be assigned trade routes via right-click.
- `ActionGather` (`src/actions/ActionGather.cpp`) is generic and data-driven. It already handles fish (ocean, deep-sea, shore) via the same gather loop. `findNextGatherTarget()` auto-seeks the next same-class resource after depletion.
- `Farm` (`src/mechanics/Farm.cpp`) extends `Building`. Auto-reseed is already implemented (deducts 60 wood, resets food). Fish traps (ID 199) already get `Farm` treatment because `UnitFactory` checks `gunit.Class == genie::Unit::Farm` (class 10).
- `ActionBuild` (`src/actions/ActionBuild.cpp`) auto-gathers nearest resource after completing a drop-off building (lumber camp, mill, mining camp) but does **not** return villagers to their previous task for other buildings (houses, barracks, etc.).
- Key IDs: Trade Cart class=19, Trade Cog ID=17, Market ID=84, Dock ID=45, Fish Trap ID=199, Farm ID=50.
- `IAction::assignTask()` dispatches by `genie::ActionType`. Missing: `Trade` case. The `findMatchingTask()` in `UnitActionHandler.cpp` resolves tasks by matching target unit ID/class against the dat file's task entries, so trade cogs right-clicking a dock should produce a `Trade` task from the dat file.

**Tech Stack:** C++20, SDL2

---

## Task 1: Wire ActionTrade into IAction::assignTask

**Files:**
- `src/actions/IAction.cpp`

**Problem:** `IAction::assignTask()` switch statement has no `case genie::ActionType::Trade`. When a trade cart (or trade cog) right-clicks a market (or dock), the task system finds a Trade task via `findMatchingTask()`, but `assignTask()` falls through to the default "Unhandled action type" warning. The trade action is never created.

**Changes in** `src/actions/IAction.cpp`:

Add include at top (after existing includes around line 31):
```cpp
#include "ActionTrade.h"
```

Add new case in the switch statement at line 119 (before `default:`):
```cpp
    case genie::ActionType::Trade: {
        if (!target) {
            DBG << "Can't trade with nothing";
            return;
        }

        if (assignType == AssignType::Replace) {
            unit->actions.clearActionQueue();
        }

        unit->actions.queueAction(std::make_shared<ActionTrade>(unit, target));
        break;
    }
```

**Commit message:** `feat: wire ActionTrade into task assignment for trade carts and cogs`

---

## Task 2: Fix ActionTrade to use home dock/market instead of spawn position

**File:** `src/actions/ActionTrade.h`, `src/actions/ActionTrade.cpp`

**Problem:** `ActionTrade` stores `m_homePos = cart->position()` as the "home market". This is wherever the unit happened to be when the trade action was created, not the nearest friendly market/dock. For trade cogs, the home should be the nearest dock owned by the same player.

**Changes in** `src/actions/ActionTrade.h`:

Add a new member and helper:
```cpp
private:
    std::weak_ptr<Unit> m_targetMarket;
    std::weak_ptr<Unit> m_homeMarket;  // ADD: nearest own market/dock
    MapPos m_homePos;
    bool m_goingToTarget = true;
    bool m_isMoving = false;
    static constexpr float TRADE_RANGE = 32.f;

    std::shared_ptr<Unit> findNearestOwnMarketOrDock(const Unit::Ptr &unit);  // ADD
```

**Changes in** `src/actions/ActionTrade.cpp`:

Replace the constructor to find the nearest own market/dock:
```cpp
ActionTrade::ActionTrade(const Unit::Ptr &tradeCart, const Unit::Ptr &targetMarket)
    : IAction(Type::Trade, tradeCart, Task())
    , m_targetMarket(targetMarket)
{
    Unit::Ptr cart = m_unit.lock();
    if (!cart) return;

    // Find nearest own market or dock as "home"
    Unit::Ptr home = findNearestOwnMarketOrDock(cart);
    if (home) {
        m_homeMarket = home;
        m_homePos = home->position();
    } else {
        m_homePos = cart->position();
    }
}
```

Add the helper function:
```cpp
std::shared_ptr<Unit> ActionTrade::findNearestOwnMarketOrDock(const Unit::Ptr &unit)
{
    constexpr int MARKET_ID = 84;
    constexpr int DOCK_ID = 45;

    float closestDist = std::numeric_limits<float>::max();
    Unit::Ptr closest;

    for (const Unit::Ptr &other : unit->unitManager().units()) {
        if (other->playerId() != unit->playerId()) continue;
        int id = other->data()->ID;
        if (id != MARKET_ID && id != DOCK_ID) continue;
        if (other->creationProgress() < 1.f) continue;

        float dist = unit->position().distance(other->position());
        if (dist < closestDist) {
            closestDist = dist;
            closest = other;
        }
    }
    return closest;
}
```

Add `#include <limits>` at top if not already present.

**Also update** `update()` to use `m_homeMarket` position dynamically (in case the building was destroyed and rebuilt):
```cpp
ActionTrade::UpdateResult ActionTrade::update(Time time)
{
    (void)time;
    Unit::Ptr cart = m_unit.lock();
    Unit::Ptr market = m_targetMarket.lock();
    if (!cart || !market) return UpdateResult::Completed;

    // Update home position from home market if still alive
    Unit::Ptr home = m_homeMarket.lock();
    if (home) {
        m_homePos = home->position();
    } else {
        // Home market destroyed -- try to find a new one
        home = findNearestOwnMarketOrDock(cart);
        if (home) {
            m_homeMarket = home;
            m_homePos = home->position();
        } else {
            return UpdateResult::Completed; // no home to return to
        }
    }

    MapPos targetPos = m_goingToTarget ? market->position() : m_homePos;
    float dist = cart->position().distance(targetPos);

    if (dist > TRADE_RANGE && !m_isMoving) {
        m_isMoving = true;
        auto move = ActionMove::moveUnitTo(cart, targetPos);
        if (move) {
            cart->actions.queueAction(move);
        }
        return UpdateResult::Updated;
    }

    if (dist <= TRADE_RANGE) {
        m_isMoving = false;

        if (!m_goingToTarget) {
            // Arrived back home -- deposit gold
            const float pixelsPerTile = 48.f;
            const float tradeDist = m_homePos.distance(market->position());
            const float distTiles = tradeDist / pixelsPerTile;

            float mapSize = 120.f;
            if (cart->map()) {
                mapSize = float(std::max(cart->map()->columnCount(), cart->map()->rowCount()));
            }

            const float goldEarned = 0.46f * distTiles * (distTiles / mapSize + 0.3f);

            auto owner = cart->player().lock();
            if (owner) {
                owner->setAvailableResource(genie::ResourceType::GoldStorage,
                    owner->resourcesAvailable(genie::ResourceType::GoldStorage) + goldEarned);
            }
        }

        m_goingToTarget = !m_goingToTarget;
        return UpdateResult::Updated;
    }

    return UpdateResult::NotUpdated;
}
```

**Commit message:** `feat: trade action finds nearest own market/dock as home endpoint`

---

## Task 3: Support trade cogs (water trade between docks)

**File:** `src/actions/ActionTrade.cpp`

**Problem:** Trade cogs (ID 17, class `TradeBoat`=2) trade between docks (ID 45) on water. The `ActionTrade` logic is already generic enough -- it bounces between home and target, deposits gold. With Task 1 and Task 2, right-clicking an allied/own dock with a trade cog selected will create an `ActionTrade` action, find the nearest own dock as home, and trade.

**Verification needed:** The dat file must have a `Trade` task for trade cog units. The `findMatchingTask()` system matches by `ActionType` + `UnitID`/`ClassID` from the dat file. If the trade cog has a `Trade` task targeting Dock class/ID, it will just work.

**No code change needed** beyond Tasks 1 and 2. The only difference between land trade (cart<->market) and water trade (cog<->dock) is the unit IDs/classes, which are data-driven from the dat file.

**Testing:** Build the project, start a game with water, build two docks (one own, one allied), train a trade cog, right-click the allied dock. Verify the cog bounces between docks and generates gold.

**Commit message:** (no commit -- verification only)

---

## Task 4: Verify fish trap mechanics

**File:** `src/mechanics/Farm.cpp`, `src/mechanics/UnitFactory.cpp`

**Problem:** Fish traps (ID 199) share `genie::Unit::Farm` class (class 10). The `UnitFactory::createUnit()` already checks `gunit.Class == genie::Unit::Farm` at line 197, so fish traps get the `Farm` subclass. The `Farm::update()` auto-reseeds when food hits 0 (deducts 60 wood). Fishing ships should be able to gather from fish traps the same way villagers gather from farms -- via `ActionGather` with `GatherRebuild` action type.

**Verification:** The fishing ship's dat file tasks must include a `GatherRebuild` task that matches fish trap class/ID. The `IAction::assignTask()` already handles `GatherRebuild`. The `ActionBuild` already queues a gather action after building a Farm-class structure (line 71-76 in `IAction.cpp`).

**Potential issue:** Fish traps are built on water. The auto-reseed cost of 60 wood in `Farm::update()` is hardcoded. In AoE2, fish trap reseed cost is 100 wood (not 60 like farms). We should differentiate.

**Changes in** `src/mechanics/Farm.cpp`, `update()` function, lines 59-68:

Replace the hardcoded 60 with a lookup:
```cpp
bool Farm::update(Time time) noexcept
{
    bool updated = Unit::update(time); // NOLINT

    if (util::floatsEquals(resources[genie::ResourceType::FoodStorage], 0)) {
        // Auto-reseed: if player can afford, reset farm/fish trap
        Player::Ptr owner = player().lock();
        // Fish traps (ID 199) cost 100 wood to reseed, farms cost 60
        const float reseedCost = (data()->ID == 199) ? 100.f : 60.f;
        if (owner && owner->resourcesAvailable(genie::ResourceType::WoodStorage) >= reseedCost) {
            owner->setAvailableResource(genie::ResourceType::WoodStorage,
                owner->resourcesAvailable(genie::ResourceType::WoodStorage) - reseedCost);
            resources[genie::ResourceType::FoodStorage] = data()->ResourceStorages[0].Amount;
            setTerrain(FarmFinished);
            DBG << "Auto-reseeded farm/fish trap (cost=" << reseedCost << ")";
        } else {
            setTerrain(FarmDead);
        }
    }

    if (m_updated) {
        m_updated = false;
        return true;
    }

    return updated;
}
```

**Commit message:** `fix: fish traps use correct 100 wood reseed cost instead of 60`

---

## Task 5: Farm queue at TC/Mill (auto-rebuild when farm expires)

**Files:**
- `src/mechanics/Farm.h`
- `src/mechanics/Farm.cpp`

**Problem:** In AoE2, players can queue farms at the TC or Mill. When a farm expires (food depleted and auto-reseed fails due to insufficient wood), a queued farm is built automatically when resources become available. Currently, the auto-reseed in `Farm::update()` either immediately reseeds or sets terrain to `FarmDead` -- there is no queue.

**Approach:** Rather than a full queue system (which would require UI changes to TC/Mill), implement a simpler "auto-rebuild" flag on Farm objects. When a farm dies and the player lacks wood, mark it as "pending reseed". On each `update()`, check if the player now has enough wood to reseed. This approximates farm queuing without UI changes.

**Changes in** `src/mechanics/Farm.h`:

Add a member:
```cpp
private:
    void setTerrain(const TerrainTypes terrainToSet) noexcept;

    int m_currentTerrain = -1;
    bool m_updated = true;
    bool m_pendingReseed = false;  // ADD: waiting for wood to reseed
    FarmRender m_farmRenderer;
```

**Changes in** `src/mechanics/Farm.cpp`, `update()`:

```cpp
bool Farm::update(Time time) noexcept
{
    bool updated = Unit::update(time); // NOLINT

    if (util::floatsEquals(resources[genie::ResourceType::FoodStorage], 0)) {
        Player::Ptr owner = player().lock();
        const float reseedCost = (data()->ID == 199) ? 100.f : 60.f;
        if (owner && owner->resourcesAvailable(genie::ResourceType::WoodStorage) >= reseedCost) {
            owner->setAvailableResource(genie::ResourceType::WoodStorage,
                owner->resourcesAvailable(genie::ResourceType::WoodStorage) - reseedCost);
            resources[genie::ResourceType::FoodStorage] = data()->ResourceStorages[0].Amount;
            setTerrain(FarmFinished);
            m_pendingReseed = false;
            DBG << "Auto-reseeded farm/fish trap (cost=" << reseedCost << ")";
        } else {
            if (!m_pendingReseed) {
                setTerrain(FarmDead);
                m_pendingReseed = true;
                DBG << "Farm depleted, waiting for wood to reseed";
            }
            // Keep checking each update -- will reseed when player gets enough wood
        }
    }

    if (m_updated) {
        m_updated = false;
        return true;
    }

    return updated;
}
```

This way depleted farms automatically reseed as soon as the player accumulates enough wood, acting as an implicit queue.

**Commit message:** `feat: farms auto-reseed when player accumulates enough wood (implicit queue)`

---

## Task 6: Villager auto-return to previous task after building

**Files:**
- `src/actions/ActionBuild.h`
- `src/actions/ActionBuild.cpp`

**Problem:** When a villager is pulled off a task (e.g., chopping wood) to build a house, after the house finishes the villager goes idle. In AoE2, villagers return to their previous task after building non-drop-off buildings. The current code only handles drop-off buildings (lumber camp, mill, mining camp) by finding nearby resources.

**Approach:** Before starting a build action, save the villager's current action type and target position. After the building completes (and it's not a drop-off building), re-assign the saved task.

**Changes in** `src/actions/ActionBuild.h`:

Add members to save previous task state:
```cpp
class ActionBuild : public IAction
{
public:
    ActionBuild(const UnitPtr &builder, const Task &task);
    ~ActionBuild();

    UpdateResult update(Time time) override;
    UnitState unitState() const override;
    genie::ActionType taskType() const override { return genie::ActionType::Build; }

    // Save previous task so villager can return after building
    void setPreviousTask(const Task &task, const MapPos &targetPos);

private:
    std::weak_ptr<Building> m_targetBuilding;

    // Previous task restoration
    Task m_previousTask;
    MapPos m_previousTargetPos;
    bool m_hasPreviousTask = false;
};
```

**Changes in** `src/actions/ActionBuild.cpp`:

Add the setter:
```cpp
void ActionBuild::setPreviousTask(const Task &task, const MapPos &targetPos)
{
    if (task.isValid()) {
        m_previousTask = task;
        m_previousTargetPos = targetPos;
        m_hasPreviousTask = true;
    }
}
```

In `update()`, after the building finishes (around line 60, inside the `if (building->creationProgress() >= 1.)` block), after the existing drop-off building logic, add previous task restoration for non-drop-off buildings:

```cpp
        // For non-drop-off buildings, return to previous task
        if (buildingId != 562 && buildingId != 68 && buildingId != 584 && buildingId != 109) {
            if (m_hasPreviousTask && m_previousTask.isValid()) {
                Unit::Ptr prevTarget = m_previousTask.target.lock();
                if (prevTarget && prevTarget->isAlive()) {
                    // Return to the previous gather/work target
                    unit->actions.queueAction(ActionMove::moveUnitTo(unit, prevTarget));
                    IAction::assignTask(m_previousTask, unit, IAction::AssignType::Queue);
                } else {
                    // Target gone -- move back to where we were working
                    unit->actions.queueAction(ActionMove::moveUnitTo(unit, m_previousTargetPos));
                }
            }
        }
```

**Changes in** `src/actions/IAction.cpp`:

In the `Build` case of `assignTask()` (around line 54-77), capture the current task before clearing the queue:

```cpp
    case genie::ActionType::Build: {
        if (!target) {
            DBG << "Can't build nothing";
            return;
        }

        // Capture current task before clearing queue
        Task savedTask;
        MapPos savedPos;
        if (unit->actions.m_currentAction) {
            if (unit->actions.m_currentAction->type == IAction::Type::Gather) {
                savedTask = unit->actions.m_currentAction->m_task;  // Note: m_task is protected
                Unit::Ptr savedTarget = savedTask.target.lock();
                savedPos = savedTarget ? savedTarget->position() : unit->position();
            }
        }

        if (assignType == AssignType::Replace) {
            unit->actions.clearActionQueue();
        }

        unit->actions.queueAction(ActionMove::moveUnitTo(unit, target->position(), task));

        auto buildAction = std::make_shared<ActionBuild>(unit, task);
        buildAction->requiredUnitID = task.unitId;
        if (savedTask.isValid()) {
            buildAction->setPreviousTask(savedTask, savedPos);
        }
        unit->actions.queueAction(buildAction);

        if (target->data()->Class == genie::Unit::Farm) {
            Task farmTask = unit->actions.findAnyTask(genie::ActionType::GatherRebuild, target->data()->ID);
            ActionPtr farmAction = std::make_shared<ActionGather>(unit, farmTask);
            farmAction->requiredUnitID = farmTask.unitId;
            unit->actions.queueAction(farmAction);
        }
        break;
    }
```

**Note:** `m_task` is `protected` in `IAction`. Since `assignTask` is a static method of `IAction`, it can access `m_task` on the action object. However, the action is accessed via `unit->actions.m_currentAction` which is a `shared_ptr<IAction>`. We need to access `m_task` which is protected. Options:
1. Make `m_task` public (simplest, matches the existing code style where `m_actionQueue` and `m_currentAction` are already public).
2. Add a getter `const Task& task() const { return m_task; }` to IAction.

**Preferred approach -- add a public getter to** `src/actions/IAction.h`:

```cpp
    const Task& task() const { return m_task; }
```

Then in `IAction.cpp`, use `unit->actions.m_currentAction->task()` instead of `unit->actions.m_currentAction->m_task`.

**Commit message:** `feat: villagers return to previous task after completing a building`

---

## Task 7: Shore fish gathering verification

**Files:** No code changes expected -- verification only.

**Problem:** Shore fish (class 33 = `genie::Unit::ShoreFish`) should be gatherable by villagers walking along the shore. The gather system matches tasks by class ID from the dat file. Villagers have `GatherRebuild` tasks that should match shore fish class.

**Verification steps:**
1. Build the project: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
2. Start a game with a coastal map
3. Right-click a villager on shore fish
4. Check logs for `findMatchingTask` debug output confirming a match
5. Verify the villager walks to the shore fish and gathers food

**Potential issue:** Shore fish are on water-adjacent tiles. The pathfinder may not route villagers to them if the tile is water terrain. In AoE2, villagers can gather shore fish from adjacent land tiles (they stand on land, reach into water).

**If pathfinding fails**, the fix would be in `ActionMove` or pathfinding to allow units to move to a tile adjacent to the target (not on the target's tile) when the target is on impassable terrain. This would be:

In `src/actions/ActionMove.cpp`, when creating a move-to-unit action, check if the target tile is impassable for the unit. If so, find the nearest passable adjacent tile and move there instead. This is a larger change that should be a separate task if needed.

**Commit message:** (no commit -- verification only, unless pathfinding fix is needed)

---

## Summary of Changes

| Task | File(s) | Change |
|------|---------|--------|
| 1 | `src/actions/IAction.cpp` | Add `#include "ActionTrade.h"` and `case genie::ActionType::Trade` to `assignTask()` |
| 2 | `src/actions/ActionTrade.h`, `src/actions/ActionTrade.cpp` | Find nearest own market/dock as home endpoint, add `findNearestOwnMarketOrDock()` |
| 3 | (none) | Verification: trade cogs work via Tasks 1+2 (data-driven from dat file) |
| 4 | `src/mechanics/Farm.cpp` | Fish trap reseed cost 100 wood (not 60) |
| 5 | `src/mechanics/Farm.h`, `src/mechanics/Farm.cpp` | Implicit farm queue: retry reseed each update when wood becomes available |
| 6 | `src/actions/ActionBuild.h`, `src/actions/ActionBuild.cpp`, `src/actions/IAction.h`, `src/actions/IAction.cpp` | Save/restore previous task for villager auto-return after building |
| 7 | (none) | Verification: shore fish gathering works via existing gather system |

## Build & Test

```bash
cd /home/dima/Projects/freeaoe/build && make -j$(nproc)
```

Run the game and test:
1. Train a trade cart, right-click an allied market -- should bounce and generate gold
2. Train a trade cog, right-click an allied dock -- should bounce and generate gold
3. Build a fish trap with a fishing ship -- should auto-gather after construction
4. Let a farm deplete when player has < 60 wood -- should auto-reseed when wood arrives
5. Pull a lumberjack to build a house -- should return to chopping after house finishes
6. Right-click a villager on shore fish -- should gather food
