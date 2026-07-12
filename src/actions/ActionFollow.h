#pragma once

#include "IAction.h"
#include "mechanics/Unit.h"

class ActionFollow : public IAction
{
public:
    ActionFollow(const Unit::Ptr &unit, const Unit::Ptr &target);
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Follow; }

private:
    std::weak_ptr<Unit> m_followTarget;
    bool m_isMoving = false;
    static constexpr float FOLLOW_DISTANCE = 64.f;
};
