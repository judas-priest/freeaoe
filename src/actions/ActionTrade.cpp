#include "ActionTrade.h"
#include "ActionMove.h"
#include "mechanics/Player.h"

#include <genie/dat/ResourceType.h>

ActionTrade::ActionTrade(const Unit::Ptr &tradeCart, const Unit::Ptr &targetMarket)
    : IAction(Type::Trade, tradeCart, Task())
    , m_targetMarket(targetMarket)
{
    Unit::Ptr cart = m_unit.lock();
    if (cart) {
        m_homePos = cart->position();
    }
}

ActionTrade::UpdateResult ActionTrade::update(Time time)
{
    (void)time;
    Unit::Ptr cart = m_unit.lock();
    Unit::Ptr market = m_targetMarket.lock();
    if (!cart || !market) return UpdateResult::Completed;

    MapPos targetPos = m_goingToTarget ? market->position() : m_homePos;
    float dist = cart->position().distance(targetPos);

    if (dist > TRADE_RANGE && !m_isMoving) {
        m_isMoving = true;
        auto move = ActionMove::moveUnitTo(cart, targetPos);
        if (move) {
            cart->actions.queueAction(move);
        }
        return UpdateResult::Updated;
    }

    if (dist <= TRADE_RANGE) {
        m_isMoving = false;

        if (!m_goingToTarget) {
            // Arrived back home — deposit gold
            // Gold earned = distance between markets / 10
            float tradeDist = m_homePos.distance(market->position());
            float goldEarned = tradeDist / 10.f;

            auto owner = cart->player().lock();
            if (owner) {
                owner->setAvailableResource(genie::ResourceType::GoldStorage,
                    owner->resourcesAvailable(genie::ResourceType::GoldStorage) + goldEarned);
            }
        }

        m_goingToTarget = !m_goingToTarget;
        return UpdateResult::Updated;
    }

    return UpdateResult::NotUpdated;
}
