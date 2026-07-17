# Module 32: Double-Click Select Same Type — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Double-clicking a unit selects all visible units of the same type on screen, matching AoE2 behavior.

**Architecture:** Track last click time and clicked unit type in Engine. On second click within 500ms on same unit type, expand selection to all visible units of that type. Uses existing `isVisible` flag set by UnitsRenderer during render pass.

**Tech Stack:** C++, SDL2

---

### Task 1: Implement double-click detection and same-type selection

**Files:**
- Modify: `src/Engine.h` (add double-click tracking state)
- Modify: `src/Engine.cpp` (handle double-click in mouse release)
- Modify: `src/mechanics/UnitManager.h` (add selectAllOfType method)
- Modify: `src/mechanics/UnitManager.cpp` (implement selectAllOfType)

- [ ] **Step 1: Add double-click tracking to Engine.h**

In `Engine.h`, add member variables near other mouse state (around line 250):
```cpp
int64_t m_lastLeftClickTime = 0;
int m_lastClickedUnitId = -1;
```

- [ ] **Step 2: Add selectAllVisibleOfType to UnitManager**

In `UnitManager.h`, add declaration:
```cpp
void selectAllVisibleOfType(int unitTypeId, int playerId);
```

In `UnitManager.cpp`, implement:
```cpp
void UnitManager::selectAllVisibleOfType(int unitTypeId, int playerId)
{
    clearSelections();
    for (const Unit::Ptr &unit : m_units) {
        if (!unit || unit->isDead()) {
            continue;
        }
        if (unit->playerId() != playerId) {
            continue;
        }
        if (unit->data()->ID != unitTypeId) {
            continue;
        }
        if (!unit->isVisible) {
            continue;
        }
        selectUnit(unit);
    }
}
```

- [ ] **Step 3: Handle double-click in Engine::handleMouseRelease**

In `Engine.cpp`, in `handleMouseRelease()` for left button release, find the selection logic (where `onLeftClick` or `selectUnitsInRect` is called for single click). Before or after the existing single-click selection, add:

```cpp
if (event.button == input::MouseButton::Left) {
    const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    // Check for double-click (< 500ms since last click)
    if (now - m_lastLeftClickTime < 500) {
        // Get currently selected units
        const auto &selected = state->unitManager()->selectedUnits();
        if (!selected.empty()) {
            const auto &firstUnit = selected.front();
            if (firstUnit && firstUnit->data()->ID == m_lastClickedUnitId) {
                state->unitManager()->selectAllVisibleOfType(
                    m_lastClickedUnitId,
                    state->humanPlayer()->playerId
                );
                m_lastClickedUnitId = -1;
                m_lastLeftClickTime = 0;
                return true;
            }
        }
    }

    // Track for next potential double-click
    m_lastLeftClickTime = now;
    const auto &selected = state->unitManager()->selectedUnits();
    if (!selected.empty()) {
        m_lastClickedUnitId = selected.front()->data()->ID;
    } else {
        m_lastClickedUnitId = -1;
    }
}
```

Note: Use the same time source used elsewhere in Engine (check if `SDL_GetTicks64()` or `std::chrono` is the convention). Adjust accordingly.

- [ ] **Step 4: Build and verify**

Run: `cd /home/dima/Projects/freeaoe/build && make -j$(nproc)`
Expected: Clean compilation.

- [ ] **Step 5: Manual test**

1. Start a game with multiple villagers
2. Single-click one villager — only that villager selected
3. Double-click a villager — all visible villagers of that type selected
4. Double-click a military unit — all visible units of that type selected

- [ ] **Step 6: Commit**

```bash
git add src/Engine.h src/Engine.cpp src/mechanics/UnitManager.h src/mechanics/UnitManager.cpp
git commit -m "feat: double-click to select all visible units of same type"
```
