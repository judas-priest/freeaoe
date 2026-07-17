#include "ActionPatrol.h"
#include "ActionAttack.h"
#include "ActionMove.h"
#include "mechanics/UnitManager.h"

#include <genie/dat/Unit.h>

ActionPatrol::ActionPatrol(const Unit::Ptr &unit, const MapPos &destination)
    : IAction(Type::Patrol, unit, Task())
    , m_destPos(destination)
{
    Unit::Ptr u = m_unit.lock();
    if (u) {
        m_startPos = u->position();
    }
}

ActionPatrol::UpdateResult ActionPatrol::update(Time time)
{
    (void)time;
    Unit::Ptr unit = m_unit.lock();
    if (!unit) return UpdateResult::Completed;

    // If we were attacking, reset and resume patrol movement
    if (m_isAttacking) {
        m_isAttacking = false;
        m_moveQueued = false;
    }

    // Scan for nearby enemies and engage
    Unit::Ptr enemy = findEnemyInRange(unit);
    if (enemy) {
        m_isAttacking = true;
        m_moveQueued = false;

        Task attackTask;
        attackTask.target = enemy;
        unit->actions.prependAction(std::make_shared<ActionAttack>(unit, attackTask));
        return UpdateResult::Updated;
    }

    if (m_moveQueued) {
        m_moveQueued = false;
        m_returning = !m_returning;
    }

    MapPos target = m_returning ? m_startPos : m_destPos;
    auto move = ActionMove::moveUnitTo(unit, target);
    if (move) {
        unit->actions.queueAction(move);
        m_moveQueued = true;
    }

    return UpdateResult::Updated;
}

Unit::Ptr ActionPatrol::findEnemyInRange(const Unit::Ptr &unit) const
{
    const float losRange = unit->data()->LineOfSight * 48.f;
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
