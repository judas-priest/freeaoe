#include "ActionTrade.h"
#include "ActionMove.h"
#include "mechanics/Map.h"
#include "mechanics/Player.h"
#include "mechanics/UnitManager.h"

#include <genie/dat/ResourceType.h>

#include <algorithm>

ActionTrade::ActionTrade(const Unit::Ptr &tradeCart, const Unit::Ptr &targetMarket)
    : IAction(Type::Trade, tradeCart, Task())
    , m_targetMarket(targetMarket)
{
    Unit::Ptr cart = m_unit.lock();
    if (cart) {
        Unit::Ptr home = findNearestOwnMarketOrDock(cart, targetMarket);
        if (home) {
            m_homeMarket = home;
            m_homePos = home->position();
        } else {
            m_homePos = cart->position();
        }
    }
}

Unit::Ptr ActionTrade::findNearestOwnMarketOrDock(const Unit::Ptr &cart, const Unit::Ptr &excludeMarket)
{
    Unit::Ptr best;
    float bestDist = 999999.f;

    for (const Unit::Ptr &u : cart->unitManager().units()) {
        if (!u || u->isDead() || u->isDying()) continue;
        if (u->playerId() != cart->playerId()) continue;
        if (u == excludeMarket) continue;

        int id = u->data()->ID;
        if (id != MARKET_ID && id != DOCK_ID) continue;

        float dist = cart->distanceTo(u);
        if (dist < bestDist) {
            bestDist = dist;
            best = u;
        }
    }

    return best;
}

ActionTrade::UpdateResult ActionTrade::update(Time time)
{
    (void)time;
    Unit::Ptr cart = m_unit.lock();
    Unit::Ptr market = m_targetMarket.lock();
    if (!cart || !market) return UpdateResult::Completed;

    // Update home position dynamically from home market if still alive
    Unit::Ptr homeMarket = m_homeMarket.lock();
    if (homeMarket && !homeMarket->isDead()) {
        m_homePos = homeMarket->position();
    }

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
