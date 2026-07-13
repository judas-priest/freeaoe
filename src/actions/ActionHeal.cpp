#include "ActionHeal.h"
#include "ActionMove.h"

#include <genie/dat/Unit.h>

ActionHeal::ActionHeal(const Unit::Ptr &monk, const Unit::Ptr &target)
    : IAction(Type::Heal, monk, Task())
    , m_target(target)
{
}

ActionHeal::UpdateResult ActionHeal::update(Time time)
{
    Unit::Ptr monk = m_unit.lock();
    Unit::Ptr target = m_target.lock();
    if (!monk || !target) return UpdateResult::Completed;

    // Done if fully healed
    if (target->hitpointsLeft() >= target->data()->HitPoints) {
        return UpdateResult::Completed;
    }

    // Don't heal dead units
    if (target->hitpointsLeft() <= 0) {
        return UpdateResult::Completed;
    }

    float dist = monk->position().distance(target->position());

    if (dist > HEAL_RANGE && !m_isMoving) {
        m_isMoving = true;
        auto move = ActionMove::moveUnitTo(monk, target->position());
        if (move) {
            monk->actions.queueAction(move);
        }
        return UpdateResult::Updated;
    }

    if (dist <= HEAL_RANGE) {
        m_isMoving = false;
    }

    // Heal tick
    if (!m_isMoving && time - m_lastHealTime >= HEAL_INTERVAL) {
        m_lastHealTime = time;
        target->takeDamage(-1.f); // negative damage = heal
        return UpdateResult::Updated;
    }

    return UpdateResult::NotUpdated;
}
