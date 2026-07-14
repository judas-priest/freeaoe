#pragma once

#include "actions/IAction.h"
#include "core/Types.h"

#include <memory>

class ActionTransform : public IAction
{
public:
    /// @param unit The unit transforming
    /// @param task The task context
    /// @param targetUnitId The genie unit ID to transform into
    /// @param durationMs How long the transform takes in milliseconds
    ActionTransform(const std::shared_ptr<Unit> &unit, const Task &task,
                    int targetUnitId, Time durationMs);

    UpdateResult update(Time time) override;
    UnitState unitState() const override { return UnitState::Working; }
    genie::ActionType taskType() const override { return genie::ActionType::Pack; }

private:
    int m_targetUnitId;
    Time m_durationMs;
    Time m_startTime = 0;
};
