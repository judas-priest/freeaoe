#include "ActionTrade.h"
#include "ActionMove.h"
#include "mechanics/Map.h"
#include "mechanics/Player.h"

#include <genie/dat/ResourceType.h>

#include <algorithm>

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
            // Arrived back home — deposit gold using AoE2 formula:
            //   gold = 0.46 * d_tiles * (d_tiles / mapSize + 0.3)
            const float pixelsPerTile = 48.f;
            const float tradeDist = m_homePos.distance(market->position());
            const float distTiles = tradeDist / pixelsPerTile;

            float mapSize = 120.f;
            if (cart->map()) {
                mapSize = float(std::max(cart->map()->columnCount(), cart->map()->rowCount()));
            }

            const float goldEarned = 0.46f * distTiles * (distTiles / mapSize + 0.3f);

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
