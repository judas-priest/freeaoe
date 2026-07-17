# Module 6: Building Construction Stages & Rubble

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show progressive construction stages instead of instant appearance. Show rubble when buildings are destroyed.

**Architecture:** Genie `data()->Building.ConstructionGraphicID` has the construction graphic. During building construction (`m_creationProgress < 1.0`), render the construction graphic with frame based on progress. On death, spawn a `DecayingEntity(graphicId, decayTime, Size)` at the building position using `data()->DyingGraphic`. `DecayingEntity` is defined in Entity.h:146 with constructor `(int graphicId, float decayTime, Size size)`.

**Tech Stack:** C++20, modify existing rendering + death handling.

---

### Task 1: Construction stage rendering

**Files:**
- Modify: `src/render/GraphicRender.cpp` or `src/render/UnitsRenderer.cpp` — switch graphic during construction

- [ ] **Step 1: Find where building graphics are selected during rendering**

Search for `ConstructionGraphicID` or `m_creationProgress` or `setCreationProgress` in the render code. Understand how buildings currently appear during construction.

- [ ] **Step 2: Use ConstructionGraphicID during building construction**

When rendering a building with `creationProgress() < 1.0`:

```cpp
int graphicId;
if (building->creationProgress() < 1.0f && building->data()->Building.ConstructionGraphicID > 0) {
    graphicId = building->data()->Building.ConstructionGraphicID;
} else {
    graphicId = building->data()->StandingGraphic.first;
}
```

The construction graphic typically has multiple frames representing build stages. Select frame based on progress:

```cpp
// Frame selection based on progress:
int totalFrames = constructionGraphic->frameCount();
int frame = static_cast<int>(building->creationProgress() * (totalFrames - 1));
```

- [ ] **Step 3: Build and test**

Build a house — should show foundation/frame graphics progressing to completion.

- [ ] **Step 4: Commit**

```bash
git add src/render/GraphicRender.cpp src/render/UnitsRenderer.cpp
git commit -m "$(cat <<'EOF'
feat: progressive building construction using ConstructionGraphicID
EOF
)"
```

---

### Task 2: Building rubble on destruction

**Files:**
- Modify: `src/mechanics/Unit.cpp` — spawn DecayingEntity when building dies

- [ ] **Step 1: Find where unit death is processed**

Search Unit.cpp for `kill()` or the death sequence. When a building's `isDead()` becomes true and it's about to be removed from the map, check if it should spawn rubble.

- [ ] **Step 2: Spawn rubble DecayingEntity on building death**

In the building death/removal logic:

```cpp
// When a building dies:
if (unit->isBuilding()) {
    int rubbleGraphicId = unit->data()->DyingGraphic;
    if (rubbleGraphicId > 0) {
        auto rubble = std::make_shared<DecayingEntity>(
            rubbleGraphicId,
            30.f,  // 30 seconds decay
            unit->tileSize()
        );
        rubble->setPosition(unit->position());
        // Add to map
        MapPtr map = unit->map();
        if (map) {
            int col = unit->position().x / Constants::TILE_SIZE;
            int row = unit->position().y / Constants::TILE_SIZE;
            map->addEntityAt(col, row, rubble, -1);
        }
    }
}
```

Note: `DecayingEntity` constructor is `(int graphicId, float decayTime, Size size)`. Position must be set separately via `setPosition()`. Map insertion uses `addEntityAt(col, row, entity, foundationTerrain)` — use `-1` for no foundation terrain change.

- [ ] **Step 3: Build and test**

Destroy a building. Rubble/ruins should appear at its position and fade after 30 seconds.

- [ ] **Step 4: Commit**

```bash
git add src/mechanics/Unit.cpp
git commit -m "$(cat <<'EOF'
feat: building rubble on destruction using DyingGraphic + DecayingEntity
EOF
)"
```
