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
    void doConvert(const Unit::Ptr &monk, const Unit::Ptr &target);

    std::weak_ptr<Unit> m_target;
    bool m_isMoving = false;
    Time m_convertStartTime = 0;
    bool m_converting = false;
    Time m_prevRollTime = 0;

    // Per-target resistance windows (milliseconds)
    Time m_minConvertTime = 4000;
    Time m_maxConvertTime = 10000;

    static constexpr float CONVERT_RANGE = 48.f;
    static constexpr float FAITH_RECHARGE_PER_MS = 0.0016f; // 1.6 per second
    static constexpr float FAITH_MAX = 100.f;
};
