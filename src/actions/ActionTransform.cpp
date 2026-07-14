#include "ActionTransform.h"

#include "core/Logger.h"
#include "mechanics/Unit.h"
#include "mechanics/Player.h"
#include "mechanics/Civilization.h"

#include <genie/dat/Unit.h>

ActionTransform::ActionTransform(const std::shared_ptr<Unit> &unit, const Task &task,
                                 int targetUnitId, Time durationMs)
    : IAction(Type::Transform, unit, task),
      m_targetUnitId(targetUnitId),
      m_durationMs(durationMs)
{
    DBG << unit->debugName << "starting transform to unit" << targetUnitId
        << "duration" << durationMs << "ms";
}

IAction::UpdateResult ActionTransform::update(Time time)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) {
        return UpdateResult::Completed;
    }

    // Record start time on first update
    if (m_startTime == 0) {
        m_startTime = time;
        return UpdateResult::Updated;
    }

    // Check if duration has elapsed
    if (time - m_startTime < m_durationMs) {
        return UpdateResult::NotUpdated;
    }

    // Transform complete -- swap unit data
    Player::Ptr owner = unit->player().lock();
    if (!owner) {
        WARN << "No player for transforming unit";
        return UpdateResult::Completed;
    }

    const genie::Unit &newData = owner->civilization.unitData(m_targetUnitId);
    if (newData.ID == -1) {
        WARN << "Invalid transform target unit ID" << m_targetUnitId;
        return UpdateResult::Completed;
    }

    DBG << unit->debugName << "transform complete, becoming unit" << m_targetUnitId;
    unit->setUnitData(newData);

    return UpdateResult::Completed;
}
