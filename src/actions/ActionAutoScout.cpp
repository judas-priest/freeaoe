#include "ActionAutoScout.h"
#include "ActionMove.h"

#include "core/Logger.h"
#include "core/Constants.h"
#include "mechanics/Unit.h"
#include "mechanics/Player.h"
#include "mechanics/UnitManager.h"
#include "mechanics/Map.h"

#include <cmath>
#include <algorithm>

ActionAutoScout::ActionAutoScout(const Unit::Ptr &unit)
    : IAction(Type::Patrol, unit, Task()) // reuse Patrol type for animation
{
    DBG << unit->debugName << "starting auto-scout";
}

IAction::UpdateResult ActionAutoScout::update(Time time)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) {
        return UpdateResult::Completed;
    }

    // If we queued a move, wait for it to finish
    if (m_moveQueued) {
        m_moveQueued = false;
        // Move completed, find new target
    }

    // Throttle retargeting to every 2 seconds
    if (m_hasTarget && time - m_lastRetarget < 2000) {
        return UpdateResult::NotUpdated;
    }

    // Find new unexplored target
    MapPos target = findUnexploredTarget(unit);
    if (target.x < 0) {
        DBG << unit->debugName << "auto-scout: no unexplored tiles left";
        return UpdateResult::Completed;
    }

    m_currentTarget = target;
    m_hasTarget = true;
    m_lastRetarget = time;

    // Queue a move to the target
    auto move = ActionMove::moveUnitTo(unit, target);
    if (move) {
        unit->actions.queueAction(move);
        m_moveQueued = true;
    }

    return UpdateResult::Updated;
}

MapPos ActionAutoScout::findUnexploredTarget(const Unit::Ptr &unit)
{
    Player::Ptr player = unit->player().lock();
    if (!player || !player->visibility) {
        return MapPos(-1, -1);
    }

    const auto &visMap = player->visibility;
    const MapPtr &map = unit->unitManager().map();
    if (!map) {
        return MapPos(-1, -1);
    }

    const int cols = map->columnCount();
    const int rows = map->rowCount();

    // Current tile position of the scout
    const int startCol = std::clamp(int(unit->position().x / Constants::TILE_SIZE), 0, cols - 1);
    const int startRow = std::clamp(int(unit->position().y / Constants::TILE_SIZE), 0, rows - 1);

    // Spiral search outward from scout position
    MapPos bestTarget(-1, -1);
    float bestScore = -1.f;

    const int maxRadius = std::max(cols, rows);

    for (int radius = 3; radius < maxRadius; radius += 4) {
        bool foundInRing = false;

        for (int angle = 0; angle < 8; angle++) {
            // 8 directions around the ring
            int col = startCol + static_cast<int>(radius * std::cos(angle * M_PI / 4.0));
            int row = startRow + static_cast<int>(radius * std::sin(angle * M_PI / 4.0));

            // Clamp to map bounds
            col = std::clamp(col, 1, cols - 2);
            row = std::clamp(row, 1, rows - 2);

            // Count unexplored tiles in a 5x5 area around this point
            int unexploredCount = 0;
            for (int dc = -2; dc <= 2; dc++) {
                for (int dr = -2; dr <= 2; dr++) {
                    int c = col + dc;
                    int r = row + dr;
                    if (c < 0 || c >= cols || r < 0 || r >= rows) continue;
                    if (visMap->visibilityAt(c, r) == VisibilityMap::Unexplored) {
                        unexploredCount++;
                    }
                }
            }

            if (unexploredCount > 5) {
                // Score: more unexplored = better, closer = better
                float dist = std::sqrt(float((col - startCol) * (col - startCol) +
                                             (row - startRow) * (row - startRow)));
                float score = unexploredCount / (dist + 1.f);

                if (score > bestScore) {
                    bestScore = score;
                    bestTarget = MapPos(col * Constants::TILE_SIZE + Constants::TILE_SIZE / 2,
                                        row * Constants::TILE_SIZE + Constants::TILE_SIZE / 2);
                    foundInRing = true;
                }
            }
        }

        if (foundInRing) {
            break; // Found a good target in this ring, don't search further
        }
    }

    return bestTarget;
}
