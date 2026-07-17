# Module 24: Double-Click Select Same Type Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Double-clicking a unit selects all visible units of the same type on screen.

**Architecture:** Track click timing in Engine.cpp. On double-click, get the clicked unit's type ID, then iterate all visible units on screen and select those matching the same type. Uses existing `isVisible` flag set during render pass.

**Tech Stack:** C++20, SDL2 events, existing selection system

---

## Background

- Single-click selection works in `Engine.cpp` via mouse event handling
- Units have `isVisible` flag set in `UnitsRenderer` during render pass — valid for next frame's hit test
- Unit type is `unit->data()->ID`
- Selection is managed via `UnitManager::setSelectedUnits()` or similar
- AoE2 behavior: double-click selects all same-type units currently visible on screen (not all on map)

## Key Files

- `src/Engine.cpp` — Mouse click handling, selection logic
- `src/mechanics/UnitManager.h` / `UnitManager.cpp` — Selection management, unit iteration
- `src/mechanics/Unit.h` — `isVisible` flag, `data()->ID`

---

### Task 1: Detect double-click

**Files:**
- Modify: `src/Engine.cpp`

- [ ] **Step 1: Add double-click timing state**

In `Engine.cpp` (or `Engine.h`), add members:

```cpp
uint32_t m_lastClickTime = 0;
MapPos m_lastClickPos;
static constexpr uint32_t DOUBLE_CLICK_THRESHOLD = 400; // ms
static constexpr float DOUBLE_CLICK_DISTANCE = 10.f; // pixels
```

- [ ] **Step 2: Detect double-click in mouse handler**

In the left-click handler (where single unit selection happens), add double-click detection:

```cpp
uint32_t now = SDL_GetTicks();
bool isDoubleClick = (now - m_lastClickTime < DOUBLE_CLICK_THRESHOLD) &&
    (std::abs(clickPos.x - m_lastClickPos.x) < DOUBLE_CLICK_DISTANCE) &&
    (std::abs(clickPos.y - m_lastClickPos.y) < DOUBLE_CLICK_DISTANCE);
m_lastClickTime = now;
m_lastClickPos = clickPos;

if (isDoubleClick && clickedUnit) {
    selectAllVisibleOfType(clickedUnit->data()->ID);
} else {
    // Normal single-click selection
    // ... existing code ...
}
```

- [ ] **Step 3: Build**

```bash
cd build && make -j$(nproc)
```

- [ ] **Step 4: Commit**

```bash
git add src/Engine.cpp src/Engine.h
git commit -m "feat: detect double-click for unit selection"
```

---

### Task 2: Select all visible units of same type

**Files:**
- Modify: `src/mechanics/UnitManager.h`
- Modify: `src/mechanics/UnitManager.cpp`

- [ ] **Step 1: Add selectAllVisibleOfType method**

In `UnitManager.h`:

```cpp
void selectAllVisibleOfType(int unitTypeId, int playerId);
```

In `UnitManager.cpp`:

```cpp
void UnitManager::selectAllVisibleOfType(int unitTypeId, int playerId)
{
    UnitVector matched;
    for (const auto &unit : m_units) {
        if (!unit) continue;
        if (!unit->isVisible) continue;
        if (unit->data()->ID != unitTypeId) continue;
        if (unit->playerId() != playerId) continue;
        if (unit->isDead()) continue;
        matched.push_back(unit);
        if (matched.size() >= 40) break; // AoE2 max selection = 40
    }

    if (!matched.empty()) {
        setSelectedUnits(matched);
    }
}
```

- [ ] **Step 2: Wire up from Engine.cpp**

Replace the `selectAllVisibleOfType()` placeholder call in Task 1 with:

```cpp
m_unitManager->selectAllVisibleOfType(clickedUnit->data()->ID, m_humanPlayer->playerId);
```

- [ ] **Step 3: Build and test**

```bash
cd build && make -j$(nproc)
```

Place multiple villagers on screen. Double-click one — all visible villagers should be selected.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/UnitManager.h src/mechanics/UnitManager.cpp src/Engine.cpp
git commit -m "feat: double-click selects all visible units of same type"
```
