#pragma once

#include "IAction.h"
#include "mechanics/Unit.h"

class ActionHeal : public IAction
{
public:
    ActionHeal(const Unit::Ptr &monk, const Unit::Ptr &target);
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Heal; }

private:
    std::weak_ptr<Unit> m_target;
    bool m_isMoving = false;
    Time m_lastHealTime = 0;
    static constexpr float HEAL_RANGE = 32.f;
    static constexpr Time HEAL_INTERVAL = 1000; // 1 HP per second
};
