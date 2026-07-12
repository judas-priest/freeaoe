#include "ActionFollow.h"
#include "ActionMove.h"

ActionFollow::ActionFollow(const Unit::Ptr &unit, const Unit::Ptr &target)
    : IAction(Type::Follow, unit, Task())
    , m_followTarget(target)
{
}

ActionFollow::UpdateResult ActionFollow::update(Time time)
{
    (void)time;
    Unit::Ptr unit = m_unit.lock();
    Unit::Ptr target = m_followTarget.lock();
    if (!unit || !target) return UpdateResult::Completed;

    float dist = unit->position().distance(target->position());

    if (dist > FOLLOW_DISTANCE && !m_isMoving) {
        m_isMoving = true;
        auto move = ActionMove::moveUnitTo(unit, target->position());
        if (move) {
            unit->actions.queueAction(move);
        }
        return UpdateResult::Updated;
    }

    if (dist <= FOLLOW_DISTANCE) {
        m_isMoving = false;
    }

    return UpdateResult::NotUpdated;
}
