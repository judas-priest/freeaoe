#pragma once

#include "IAction.h"
#include "mechanics/Unit.h"

class ActionRepair : public IAction
{
public:
    ActionRepair(const Unit::Ptr &unit, const Unit::Ptr &building);
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Repair; }

private:
    std::weak_ptr<Unit> m_target;
    bool m_isMoving = false;
    static constexpr float REPAIR_RANGE = 16.f;
    Time m_lastRepairTime = 0;
    static constexpr Time REPAIR_INTERVAL = 1000; // 1 second between repairs
};
