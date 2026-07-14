#include "ActionAttackMove.h"
#include "ActionAttack.h"
#include "ActionMove.h"

#include "core/Logger.h"
#include "mechanics/UnitManager.h"
#include "mechanics/Player.h"

#include <genie/dat/Unit.h>

ActionAttackMove::ActionAttackMove(const Unit::Ptr &unit, const MapPos &destination)
    : IAction(Type::Move, unit, Task())
    , m_destination(destination)
{
}

ActionAttackMove::UpdateResult ActionAttackMove::update(Time /*time*/)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) return UpdateResult::Completed;

    // If we were attacking, check if the sub-action is done
    if (m_attacking) {
        m_attacking = false;
        m_moveQueued = false; // force re-queue of move
    }

    // Scan for nearby enemies
    Unit::Ptr enemy = findEnemyInRange(unit);
    if (enemy) {
        m_moveQueued = false;
        m_attacking = true;

        Task attackTask;
        attackTask.target = enemy;
        unit->actions.prependAction(std::make_shared<ActionAttack>(unit, attackTask));
        return UpdateResult::Updated;
    }

    // No enemy — move toward destination
    float dist = unit->position().distance(m_destination);
    if (dist < 8.f) {
        return UpdateResult::Completed;
    }

    if (!m_moveQueued) {
        auto move = ActionMove::moveUnitTo(unit, m_destination);
        if (move) {
            unit->actions.queueAction(move);
            m_moveQueued = true;
        }
    }

    return UpdateResult::Updated;
}

Unit::Ptr ActionAttackMove::findEnemyInRange(const Unit::Ptr &unit) const
{
    const float losRange = unit->data()->LineOfSight * 48.f; // tiles to pixels
    const int myPlayerId = unit->playerId();

    Unit::Ptr closest;
    float closestDist = losRange;

    for (const Unit::Ptr &other : unit->unitManager().units()) {
        if (!other || !other->isAlive()) continue;
        if (other->playerId() == myPlayerId) continue;
        if (other->playerId() == 0) continue; // skip Gaia

        const float dist = unit->position().distance(other->position());
        if (dist < closestDist) {
            closestDist = dist;
            closest = other;
        }
    }

    return closest;
}
