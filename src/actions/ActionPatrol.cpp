#include "ActionPatrol.h"
#include "ActionMove.h"

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
