#include "ActionRepair.h"
#include "ActionMove.h"

#include <genie/dat/Unit.h>

ActionRepair::ActionRepair(const Unit::Ptr &unit, const Unit::Ptr &building)
    : IAction(Type::Repair, unit, Task())
    , m_target(building)
{
}

ActionRepair::UpdateResult ActionRepair::update(Time time)
{
    Unit::Ptr unit = m_unit.lock();
    Unit::Ptr target = m_target.lock();
    if (!unit || !target) return UpdateResult::Completed;

    // Done if fully repaired
    if (target->hitpointsLeft() >= target->data()->HitPoints) {
        return UpdateResult::Completed;
    }

    float dist = unit->position().distance(target->position());

    // Move closer if too far
    if (dist > REPAIR_RANGE && !m_isMoving) {
        m_isMoving = true;
        auto move = ActionMove::moveUnitTo(unit, target->position());
        if (move) {
            unit->actions.queueAction(move);
        }
        return UpdateResult::Updated;
    }

    if (dist <= REPAIR_RANGE) {
        m_isMoving = false;
    }

    // Repair tick — heal 1 HP per interval (negative damage = heal)
    if (!m_isMoving && time - m_lastRepairTime >= REPAIR_INTERVAL) {
        m_lastRepairTime = time;
        target->takeDamage(-1.f);
        return UpdateResult::Updated;
    }

    return UpdateResult::NotUpdated;
}
