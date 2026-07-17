# Wall Segments Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** When walls are placed in a line, select direction-specific sprites and insert pillar units at corners/junctions.
**Architecture:** The wall placement code in UnitManager::onMouseMove() already computes orientation values (0-4) per tile. The placeBuilding() function needs to use these orientations to select the correct unit ID variant from the dat file. AoE2 walls have multiple unit IDs per direction: e.g., palisade wall ID 72 is the pillar, IDs 789-792 are directional segments. After placement, adjacent walls must update their orientation.
**Tech Stack:** C++, SDL2
---

## Task 1: Define Wall Segment ID Mapping

**File:** `src/mechanics/UnitManager.cpp`

Add a helper struct near the top (after the includes, around line 52):

```cpp
namespace WallSegments {
    struct WallInfo {
        int pillarId;     // Corner/end piece (the "base" wall unit)
        int horizontal;   // E-W segment  (orientation 1)
        int vertical;     // N-S segment  (orientation 0)
        int diagRight;    // NW-SE segment (orientation 3)
        int diagLeft;     // NE-SW segment (orientation 4)
    };

    // Palisade Wall variants
    static constexpr WallInfo Palisade  = { 72,  789, 790, 791, 792 };
    // Stone Wall variants
    static constexpr WallInfo Stone     = { 117, 793, 794, 795, 796 };
    // Fortified Wall variants
    static constexpr WallInfo Fortified = { 155, 797, 798, 799, 800 };

    static const WallInfo* infoForId(int baseId) {
        switch (baseId) {
        case 72:  case 789: case 790: case 791: case 792: return &Palisade;
        case 117: case 793: case 794: case 795: case 796: return &Stone;
        case 155: case 797: case 798: case 799: case 800: return &Fortified;
        default: return nullptr;
        }
    }

    static int segmentForOrientation(const WallInfo &info, int orientation) {
        switch (orientation) {
        case 0: return info.vertical;     // N-S
        case 1: return info.horizontal;   // E-W
        case 2: return info.pillarId;     // Corner/pillar
        case 3: return info.diagRight;    // NW-SE diagonal
        case 4: return info.diagLeft;     // NE-SW diagonal
        default: return info.pillarId;
        }
    }
}
```

## Task 2: Modify placeBuilding() to Use Directional Segments

**File:** `src/mechanics/UnitManager.cpp`

Find the `placeBuilding()` function. It currently creates a unit using the base wall ID regardless of orientation. Modify it to select the correct segment.

Find the function (search for `void UnitManager::placeBuilding`):

```cpp
void UnitManager::placeBuilding(const UnplacedBuilding &building)
```

Inside this function, before the call to `UnitFactory::createUnit()`, add orientation-based ID selection:

```cpp
    int unitId = building.unitID;

    // For walls, select the correct directional segment
    if (building.isWall) {
        const WallSegments::WallInfo *wallInfo = WallSegments::infoForId(unitId);
        if (wallInfo) {
            unitId = WallSegments::segmentForOrientation(*wallInfo, building.orientation);
        }
    }

    Unit::Ptr unit = UnitFactory::createUnit(unitId, player, *this);
```

Replace the existing `UnitFactory::createUnit(building.unitID, ...)` call with the above.

## Task 3: Update Adjacent Walls When Placing

**File:** `src/mechanics/UnitManager.cpp`

After placing each wall segment in `placeBuilding()`, scan adjacent tiles for existing wall units and update their segment type if they now have a new neighbor.

Add a helper function before `placeBuilding()`:

```cpp
static void updateAdjacentWalls(UnitManager &mgr, const MapPtr &map, const MapPos &pos)
{
    // Check 4 cardinal + 4 diagonal neighbors
    static const std::pair<int,int> offsets[] = {
        {0, -1}, {0, 1}, {-1, 0}, {1, 0},
        {-1, -1}, {1, -1}, {-1, 1}, {1, 1}
    };

    const int tileX = static_cast<int>(pos.x / Constants::TILE_SIZE);
    const int tileY = static_cast<int>(pos.y / Constants::TILE_SIZE);

    for (const auto &[dx, dy] : offsets) {
        int nx = tileX + dx;
        int ny = tileY + dy;
        if (!map->isValidTile(nx, ny)) continue;

        // Check entities at this tile for walls
        for (const auto &weakEntity : map->entitiesAt(nx, ny)) {
            auto entity = weakEntity.lock();
            if (!entity) continue;
            Unit::Ptr neighbor = Unit::fromEntity(entity);
            if (!neighbor) continue;

            const WallSegments::WallInfo *info = WallSegments::infoForId(neighbor->data()->ID);
            if (!info) continue;

            // Count neighbors of this wall to determine if it's a pillar or segment
            int neighborCount = 0;
            int lastDx = 0, lastDy = 0;
            for (const auto &[ddx, ddy] : offsets) {
                int nnx = nx + ddx;
                int nny = ny + ddy;
                if (!map->isValidTile(nnx, nny)) continue;
                for (const auto &weakEnt2 : map->entitiesAt(nnx, nny)) {
                    auto ent2 = weakEnt2.lock();
                    if (!ent2) continue;
                    Unit::Ptr u2 = Unit::fromEntity(ent2);
                    if (!u2) continue;
                    if (WallSegments::infoForId(u2->data()->ID)) {
                        neighborCount++;
                        lastDx = ddx;
                        lastDy = ddy;
                    }
                }
            }

            // Determine new orientation based on neighbor layout
            int newOrientation = 2; // default pillar
            if (neighborCount == 2) {
                // Line segment -- determine direction
                // This is simplified; full logic would check both neighbors
            } else if (neighborCount >= 3) {
                newOrientation = 2; // junction = pillar
            }

            // For now, only update isolated walls to match their single neighbor
            if (neighborCount == 1) {
                if (lastDx != 0 && lastDy == 0) newOrientation = 1;      // horizontal
                else if (lastDx == 0 && lastDy != 0) newOrientation = 0; // vertical
                else if (lastDx == lastDy) newOrientation = 3;           // NW-SE
                else newOrientation = 4;                                  // NE-SW
            }

            int newId = WallSegments::segmentForOrientation(*info, newOrientation);
            if (newId != neighbor->data()->ID) {
                Player::Ptr owner = neighbor->player().lock();
                if (owner) {
                    const genie::Unit &newData = owner->civilization.unitData(newId);
                    if (newData.ID != -1) {
                        neighbor->setUnitData(newData);
                    }
                }
            }
        }
    }
}
```

Call this at the end of `placeBuilding()`, after the unit has been added to the map:

```cpp
    if (building.isWall) {
        updateAdjacentWalls(*this, m_map, building.position);
    }
```

## Task 4: Ensure Wall Flag Is Set During Placement

**File:** `src/mechanics/UnitManager.cpp`

Check that `UnplacedBuilding::isWall` is properly set when walls are placed. In `startPlaceBuilding()`:

```cpp
void UnitManager::startPlaceBuilding(const int unitId, const std::shared_ptr<Player> &player)
```

Find where `m_buildingsToPlace` is populated. The `isWall` field should be set based on the unit class:

```cpp
    building.isWall = (gunit.Class == genie::Unit::Wall);
```

If `isWall` is already being set, no change needed. If not, add this line after `building.data = &gunit;`.

Also verify that `m_state` is set to `State::PlacingWall` when placing walls. The existing code in `startPlaceBuilding` should handle this:

```cpp
    if (building.isWall) {
        m_state = State::PlacingWall;
        m_wallPlacingStart = building.position;
    }
```

## Task 5: Handle Missing Segment IDs Gracefully

**File:** `src/mechanics/UnitManager.cpp`

The segment IDs (789-800) must exist in the dat file. If they don't (e.g., in some mod scenarios), fall back to the base wall ID. Update `segmentForOrientation`:

```cpp
    static int segmentForOrientation(const WallInfo &info, int orientation) {
        int id;
        switch (orientation) {
        case 0: id = info.vertical; break;
        case 1: id = info.horizontal; break;
        case 2: id = info.pillarId; break;
        case 3: id = info.diagRight; break;
        case 4: id = info.diagLeft; break;
        default: id = info.pillarId; break;
        }
        // Validate the ID exists; if not, fall back to pillar
        return (id > 0) ? id : info.pillarId;
    }
```

## Summary of Changes

| File | Change |
|------|--------|
| `src/mechanics/UnitManager.cpp` (top) | Add `WallSegments` namespace with ID mapping |
| `src/mechanics/UnitManager.cpp` (`placeBuilding`) | Select directional segment ID based on orientation |
| `src/mechanics/UnitManager.cpp` (new function) | Add `updateAdjacentWalls()` to update neighbors on placement |
| `src/mechanics/UnitManager.cpp` (`startPlaceBuilding`) | Ensure `isWall` flag is set from unit class |
