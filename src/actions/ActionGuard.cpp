#include "ActionGuard.h"
#include "ActionAttack.h"
#include "ActionMove.h"
#include "mechanics/UnitManager.h"

#include <genie/dat/Unit.h>

ActionGuard::ActionGuard(const Unit::Ptr &unit, const Unit::Ptr &target)
    : IAction(Type::Guard, unit, Task())
    , m_guardTarget(target)
{
}

ActionGuard::UpdateResult ActionGuard::update(Time time)
{
    (void)time;
    Unit::Ptr unit = m_unit.lock();
    Unit::Ptr target = m_guardTarget.lock();
    if (!unit || !target) return UpdateResult::Completed;

    // If we were attacking, reset follow state so we re-evaluate
    if (m_isAttacking) {
        m_isAttacking = false;
        m_isFollowing = false;
    }

    // Scan for enemies threatening the guarded unit
    Unit::Ptr enemy = findEnemyNearTarget(unit, target);
    if (enemy) {
        m_isAttacking = true;
        m_isFollowing = false;

        Task attackTask;
        attackTask.target = enemy;
        unit->actions.prependAction(std::make_shared<ActionAttack>(unit, attackTask));
        return UpdateResult::Updated;
    }

    // No enemies — follow the guarded unit
    float dist = unit->position().distance(target->position());

    if (dist > FOLLOW_DISTANCE && !m_isFollowing) {
        m_isFollowing = true;
        auto move = ActionMove::moveUnitTo(unit, target->position());
        if (move) {
            unit->actions.queueAction(move);
        }
        return UpdateResult::Updated;
    }

    if (dist <= FOLLOW_DISTANCE) {
        m_isFollowing = false;
    }

    return UpdateResult::NotUpdated;
}

Unit::Ptr ActionGuard::findEnemyNearTarget(const Unit::Ptr &unit, const Unit::Ptr &guardTarget) const
{
    const float losRange = unit->data()->LineOfSight * 48.f;
    const int myPlayerId = unit->playerId();
    const MapPos &guardPos = guardTarget->position();

    Unit::Ptr closest;
    float closestDist = losRange;

    for (const Unit::Ptr &other : unit->unitManager().units()) {
        if (!other || !other->isAlive()) continue;
        if (other->playerId() == myPlayerId) continue;
        if (other->playerId() == 0) continue; // skip Gaia

        // Check distance from the guarded unit, not from us
        const float dist = guardPos.distance(other->position());
        if (dist < closestDist) {
            closestDist = dist;
            closest = other;
        }
    }

    return closest;
}
