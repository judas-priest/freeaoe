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
    Unit::Ptr findNearestOwnMarketOrDock(const Unit::Ptr &cart, const Unit::Ptr &excludeMarket);

    std::weak_ptr<Unit> m_targetMarket;
    std::weak_ptr<Unit> m_homeMarket;  // Nearest own market/dock as home endpoint
    MapPos m_homePos;                   // Fallback if home market is destroyed
    bool m_goingToTarget = true;
    bool m_isMoving = false;
    static constexpr float TRADE_RANGE = 32.f;
    static constexpr int MARKET_ID = 84;
    static constexpr int DOCK_ID = 45;
};
