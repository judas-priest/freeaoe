# Module 4: Relic Deposit in Monastery

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Monks carrying relics can deposit them into a Monastery. The genie data has `genie::ActionType::DepositRelic` — just needs implementation and wiring.

**Architecture:** `IAction::assignTask()` (IAction.cpp:45) handles task dispatch but has NO case for `DepositRelic`. Monastery gold generation from garrisoned relics already works in Building.cpp. Monk relic carry uses `ActionPickupRelic` which stores the relic in `monk->garrisonedUnits`. Need: (1) ActionDepositRelic that moves monk to monastery and transfers relic, (2) wire in assignTask and GameState command dispatch.

**Tech Stack:** C++20, new action + modify 2 files.

---

### Task 1: Create ActionDepositRelic

**Files:**
- Create: `src/actions/ActionDepositRelic.h`
- Create: `src/actions/ActionDepositRelic.cpp`

- [ ] **Step 1: Read ActionPickupRelic to understand how relics are carried**

Read `src/actions/ActionPickupRelic.cpp` to see how relics end up in `monk->garrisonedUnits`.

- [ ] **Step 2: Create ActionDepositRelic header**

```cpp
// src/actions/ActionDepositRelic.h
#pragma once

#include "IAction.h"

class ActionDepositRelic : public IAction
{
public:
    ActionDepositRelic(const std::shared_ptr<Unit> &monk, const Task &task);

    genie::ActionType taskType() const override { return genie::ActionType::DepositRelic; }
    UnitState unitState() const override { return UnitState::Moving; }
    UpdateResult update(Time time) override;

private:
    std::weak_ptr<Unit> m_monastery;
};
```

- [ ] **Step 3: Create ActionDepositRelic implementation**

```cpp
// src/actions/ActionDepositRelic.cpp
#include "ActionDepositRelic.h"
#include "ActionMove.h"
#include "mechanics/Unit.h"
#include "mechanics/Building.h"
#include "mechanics/Player.h"
#include "core/Constants.h"
#include "core/Logger.h"

ActionDepositRelic::ActionDepositRelic(const std::shared_ptr<Unit> &monk, const Task &task)
    : IAction(Type::PickupRelic, monk, task) // reuse PickupRelic type (no separate DepositRelic in IAction::Type)
{
    // Target monastery comes from task.target
    m_monastery = task.target;
}

IAction::UpdateResult ActionDepositRelic::update(Time time)
{
    auto monk = m_unit.lock();
    if (!monk) return UpdateResult::Failed;

    auto monastery = m_monastery.lock();
    if (!monastery || !monastery->isAlive()) {
        WARN << "No monastery to deposit relic";
        return UpdateResult::Failed;
    }

    float dist = monk->distanceTo(monastery) / Constants::TILE_SIZE;
    if (dist > 1.f) {
        // Need to move closer first — prepend a move action
        auto moveAction = ActionMove::moveUnitTo(monk, monastery);
        monk->actions.prependAction(moveAction);
        return UpdateResult::NotUpdated;
    }

    // Transfer relic from monk to monastery garrison
    auto building = Building::fromUnit(monastery);
    if (!building) return UpdateResult::Failed;

    bool deposited = false;
    for (auto it = monk->garrisonedUnits.begin(); it != monk->garrisonedUnits.end(); ) {
        auto relic = it->lock();
        if (relic && relic->data()->ID == Unit::HardcodedTypes::Relic) {
            building->garrisonedUnits.push_back(*it);
            it = monk->garrisonedUnits.erase(it);
            deposited = true;
            break;
        } else {
            ++it;
        }
    }

    if (!deposited) {
        WARN << "Monk has no relic to deposit";
        return UpdateResult::Failed;
    }

    DBG << "Relic deposited in monastery";
    return UpdateResult::Completed;
}
```

- [ ] **Step 4: Add to CMakeLists.txt**

Add `src/actions/ActionDepositRelic.cpp` to the source file list.

- [ ] **Step 5: Build**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`

- [ ] **Step 6: Commit**

```bash
git add src/actions/ActionDepositRelic.h src/actions/ActionDepositRelic.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: ActionDepositRelic — monks deposit relics into monastery
EOF
)"
```

---

### Task 2: Wire DepositRelic into task assignment and command dispatch

**Files:**
- Modify: `src/actions/IAction.cpp` — add case for `genie::ActionType::DepositRelic` in `assignTask()`
- Modify: `src/mechanics/GameState.cpp` — add command handler if needed

- [ ] **Step 1: Add DepositRelic case to IAction::assignTask()**

In IAction.cpp, in the switch on `task.data->ActionType`, add:

```cpp
case genie::ActionType::DepositRelic: {
    ActionPtr action = std::make_shared<ActionDepositRelic>(unit, task);
    if (assignType == AssignType::Replace) {
        unit->actions.clearActionQueue();
    }
    unit->actions.setCurrentAction(action);
    break;
}
```

Include the header:
```cpp
#include "ActionDepositRelic.h"
```

- [ ] **Step 2: Ensure monk-with-relic right-clicking monastery gives DepositRelic task**

In `UnitActionHandler::findTaskWithTarget()`, when the unit has a DepositRelic task available (from genie data) and the target is a friendly Monastery, the task should match. This should already work if the monk's genie unit data includes a `DepositRelic` task — verify by checking the genie data.

If not matched automatically, add explicit check in `findTaskWithTarget`:

```cpp
// If monk is carrying a relic and target is own Monastery
if (!monk->garrisonedUnits.empty() && targetBuilding &&
    targetBuilding->data()->ID == Unit::HardcodedTypes::Monastery) {
    Task depositTask = unit->actions.findAnyTask(genie::ActionType::DepositRelic, -1);
    if (depositTask.data) {
        depositTask.target = target;
        return depositTask;
    }
}
```

- [ ] **Step 3: Build and test**

Pick up a relic with monk, right-click on own Monastery. Monk should walk there and deposit relic. Monastery should start generating gold.

- [ ] **Step 4: Commit**

```bash
git add src/actions/IAction.cpp src/mechanics/UnitActionHandler.cpp
git commit -m "$(cat <<'EOF'
feat: wire DepositRelic task assignment — right-click monastery with relic monk
EOF
)"
```
