#include "ActionGuard.h"
#include "ActionMove.h"

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
