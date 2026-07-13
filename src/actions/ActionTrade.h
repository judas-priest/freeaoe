#pragma once

#include "IAction.h"
#include "mechanics/Unit.h"

// Trade cart moves between two markets, generating gold based on distance
class ActionTrade : public IAction
{
public:
    ActionTrade(const Unit::Ptr &tradeCart, const Unit::Ptr &targetMarket);
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Trade; }

private:
    std::weak_ptr<Unit> m_targetMarket;
    MapPos m_homePos;          // Starting market position
    bool m_goingToTarget = true;
    bool m_isMoving = false;
    static constexpr float TRADE_RANGE = 32.f;
};
