#pragma once

#include "IAction.h"
#include "mechanics/Unit.h"

class ActionConvert : public IAction
{
public:
    ActionConvert(const Unit::Ptr &monk, const Unit::Ptr &target);
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Convert; }

private:
    std::weak_ptr<Unit> m_target;
    bool m_isMoving = false;
    Time m_convertStartTime = 0;
    Time m_convertDuration = 7000; // randomized per attempt
    bool m_converting = false;
    static constexpr float CONVERT_RANGE = 48.f;
};
