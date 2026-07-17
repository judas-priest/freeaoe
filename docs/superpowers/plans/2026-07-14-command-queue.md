# Command Queue (Shift+Click) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Allow players to queue multiple commands via shift+right-click, matching AoE2 behavior.

**Architecture:** The action queue infrastructure already exists (`std::deque<ActionPtr> m_actionQueue` in `UnitActionHandler`, `AssignType::Queue` in `IAction`). We just need to wire shift-key detection through `onRightClick` and the move path. On Android, a toggle button replaces shift.

**Tech Stack:** C++, SDL2 (`SDL_GetModState`), existing action queue system

---

## File Structure

| File | Responsibility | Change |
|------|---------------|--------|
| `src/mechanics/UnitManager.h` | Unit selection and command dispatch | Add `bool shiftHeld` param to `onRightClick` |
| `src/mechanics/UnitManager.cpp` | Right-click handler + move path | Pass `AssignType` based on shift; skip `clearActionQueue` on queue |
| `src/Engine.cpp` | Input event routing | Query `SDL_GetModState()` at 3 call sites |

---

### Task 1: Pass shift state through onRightClick signature

**Files:**
- Modify: `src/mechanics/UnitManager.h:174`
- Modify: `src/mechanics/UnitManager.cpp:551`

- [ ] **Step 1: Update onRightClick declaration**

In `src/mechanics/UnitManager.h`, change line 174:

```cpp
void onRightClick(const ScreenPos &screenPos, const CameraPtr &camera, bool shiftHeld = false);
```

- [ ] **Step 2: Update onRightClick definition signature**

In `src/mechanics/UnitManager.cpp`, change line 551:

```cpp
void UnitManager::onRightClick(const ScreenPos &screenPos, const CameraPtr &camera, bool shiftHeld)
```

- [ ] **Step 3: Build to verify no compilation errors**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc) 2>&1 | tail -5`
Expected: compiles cleanly (default param `false` keeps all existing callers working)

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/UnitManager.h src/mechanics/UnitManager.cpp
git commit -m "feat: add shiftHeld param to onRightClick for command queue"
```

---

### Task 2: Wire shift detection in Engine.cpp

**Files:**
- Modify: `src/Engine.cpp:300,1324,1402`

- [ ] **Step 1: Add SDL_GetModState at desktop mouse right-click (line 1402)**

In `src/Engine.cpp`, change line 1399-1402:

```cpp
    if (event.mouseButton.button == input::MouseButton::Right) {
        // Ensure tasks under cursor are evaluated at click position
        state->unitManager()->onCursorPositionChanged(mousePos, renderTarget_->camera());
        const bool shiftHeld = (SDL_GetModState() & KMOD_SHIFT) != 0;
        state->unitManager()->onRightClick(mousePos, renderTarget_->camera(), shiftHeld);
    }
```

- [ ] **Step 2: Touch call sites pass false (lines 300, 1324)**

Touch has no shift key. Both existing touch call sites already pass no third argument, so the default `false` applies. No code change needed — just verify:

Line 300 (long-press):
```cpp
state->unitManager()->onRightClick(m_touchState.startPos, renderTarget_->camera());
// shiftHeld defaults to false — correct for touch
```

Line 1324 (tap-on-resource):
```cpp
state->unitManager()->onRightClick(pos, renderTarget_->camera());
// shiftHeld defaults to false — correct for touch
```

- [ ] **Step 3: Build to verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc) 2>&1 | tail -5`
Expected: compiles cleanly

- [ ] **Step 4: Commit**

```bash
git add src/Engine.cpp
git commit -m "feat: detect shift key on desktop right-click for command queue"
```

---

### Task 3: Use AssignType::Queue for task-based actions when shift held

**Files:**
- Modify: `src/mechanics/UnitManager.cpp:569-601`

- [ ] **Step 1: Pass shiftHeld-derived AssignType to assignTask**

In `src/mechanics/UnitManager.cpp`, replace lines 569-601 of `onRightClick`:

```cpp
    const IAction::AssignType assignType = shiftHeld
        ? IAction::AssignType::Queue
        : IAction::AssignType::Replace;

    if (!m_tasksUnderCursor.isEmpty()) {
        for (Task task : m_tasksUnderCursor) {
            REQUIRE(task.data && task.taskId != -1, continue);

            // Thanks task swap group satan
            for (const Unit::Ptr &unit : m_selectedUnits) {
                if (unit->playerId() != humanPlayer->playerId) {
                    continue;
                }

                if (!unit->canMatchGenieUnitID(task.unitId)) {
                    WARN << "could not match genie unit id" << unit->debugName;
                    continue;
                }

                if (task.data->ActionType == genie::ActionType::Combat) {
                    AudioPlayer::instance().playSound(unit->data()->Action.AttackSound, humanPlayer->civilization.id());
                }

                IAction::assignTask(task, unit, assignType);
                Unit::Ptr target = task.target.lock();
                if (target) {
                    m_targetBlinkTimeLeft[target->id] = 3000;
                }

                foundTasks = true;
            }
        }
        if (!foundTasks) {
            WARN << "Have target under cursor, but found noone to assign?";
        }
    }
```

The key change: `IAction::AssignType::Replace` on line 589 becomes the variable `assignType`.

- [ ] **Step 2: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc) 2>&1 | tail -5`
Expected: compiles cleanly

- [ ] **Step 3: Commit**

```bash
git add src/mechanics/UnitManager.cpp
git commit -m "feat: shift+right-click queues task-based actions (gather, attack, build)"
```

---

### Task 4: Queue move commands when shift held

**Files:**
- Modify: `src/mechanics/UnitManager.cpp:669-671`

- [ ] **Step 1: Skip clearActionQueue and use queueAction for move when shift held**

In `src/mechanics/UnitManager.cpp`, replace lines 669-671 inside the per-unit move loop:

```cpp
        if (shiftHeld) {
            unit->actions.queueAction(ActionMove::moveUnitTo(unit, formationTarget));
        } else {
            unit->actions.clearActionQueue();
            moveUnitTo(unit, formationTarget);
        }
```

Note: `moveUnitTo()` uses `setCurrentAction()` which replaces the current action immediately — correct for non-queued moves. `queueAction()` appends to the deque — correct for shift-queued moves.

- [ ] **Step 2: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc) 2>&1 | tail -5`
Expected: compiles cleanly

- [ ] **Step 3: Manual test on desktop**

1. Start a game, select a unit
2. Right-click to move — unit moves immediately (old behavior)
3. Shift+right-click multiple locations — unit should visit each waypoint in order
4. Shift+right-click on a resource after queuing moves — unit should move, then gather
5. Right-click without shift — should clear queue and execute immediately

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/UnitManager.cpp
git commit -m "feat: shift+right-click queues move waypoints"
```

---

### Task 5: Clear buildings-to-place only on non-queued right-click

**Files:**
- Modify: `src/mechanics/UnitManager.cpp:553`

- [ ] **Step 1: Guard buildingsToPlace clear with shiftHeld check**

Line 553 in `onRightClick` unconditionally clears `m_buildingsToPlace`. When queuing, we should preserve building placement state:

```cpp
    if (!shiftHeld) {
        m_buildingsToPlace.clear();
    }
```

- [ ] **Step 2: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc) 2>&1 | tail -5`
Expected: compiles cleanly

- [ ] **Step 3: Commit**

```bash
git add src/mechanics/UnitManager.cpp
git commit -m "fix: preserve building placement state during shift+click queue"
```

---

### Task 6: Android build verification

**Files:** None (no code changes — verify existing changes compile for Android)

- [ ] **Step 1: Build Android APK**

```bash
cd /home/dima/Projects/freeaoe/android && rm -rf app/.cxx app/build && unset LD_PRELOAD && ./gradlew assembleDebug
```

Expected: BUILD SUCCESSFUL

- [ ] **Step 2: Install and smoke test**

```bash
unset LD_PRELOAD && adb install -r android/app/build/outputs/apk/debug/app-debug.apk
```

Verify: game launches, touch controls work as before (no regressions — shift code is desktop-only, touch passes `false`).

- [ ] **Step 3: Commit (tag only)**

No code change — this is a verification step. If build fails, fix and commit the fix.
