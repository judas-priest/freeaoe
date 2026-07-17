#pragma once

#include "IAction.h"
#include "mechanics/Unit.h"

class ActionDepositRelic : public IAction
{
public:
    ActionDepositRelic(const Unit::Ptr &monk, const Unit::Ptr &monastery, const Task &task);

    genie::ActionType taskType() const override { return genie::ActionType::DepositRelic; }
    UnitState unitState() const override { return UnitState::Moving; }
    UpdateResult update(Time time) override;

private:
    std::weak_ptr<Unit> m_monastery;
    bool m_moving = false;
};
