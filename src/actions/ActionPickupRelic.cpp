#include "ActionPickupRelic.h"
#include "ActionMove.h"
#include "mechanics/Player.h"
#include "core/Logger.h"

#include <genie/dat/ResourceType.h>

ActionPickupRelic::ActionPickupRelic(const Unit::Ptr &monk, const Unit::Ptr &relic)
    : IAction(Type::PickupRelic, monk, Task())
    , m_relic(relic)
{
}

ActionPickupRelic::~ActionPickupRelic()
{
    // If monk dies while carrying relic, drop it
    if (m_pickedUp && m_relicShared) {
        Unit::Ptr monk = m_unit.lock();
        MapPos dropPos = monk ? monk->position() : MapPos(0, 0);
        dropRelic(dropPos);
    }
}

ActionPickupRelic::UpdateResult ActionPickupRelic::update(Time time)
{
    (void)time;
    Unit::Ptr monk = m_unit.lock();
    if (!monk) return UpdateResult::Completed;

    if (m_pickedUp) {
        // Relic already picked up — monk carries it (action stays active)
        return UpdateResult::NotUpdated;
    }

    Unit::Ptr relic = m_relic.lock();
    if (!relic) return UpdateResult::Completed;

    float dist = monk->position().distance(relic->position());

    if (dist > PICKUP_RANGE && !m_isMoving) {
        m_isMoving = true;
        auto move = ActionMove::moveUnitTo(monk, relic->position());
        if (move) {
            monk->actions.queueAction(move);
        }
        return UpdateResult::Updated;
    }

    if (dist <= PICKUP_RANGE) {
        m_isMoving = false;
        // Pick up the relic — hide it but keep it alive
        m_pickedUp = true;
        m_relicShared = relic; // take shared ownership
        relic->setHidden(true);
        DBG << "Monk picked up relic (hidden, carried)";

        // Track relic for player
        auto owner = monk->player().lock();
        if (owner) {
            owner->setAvailableResource(genie::ResourceType::RelicsCaptured,
                owner->resourcesAvailable(genie::ResourceType::RelicsCaptured) + 1);
        }
        return UpdateResult::Updated;
    }

    return UpdateResult::NotUpdated;
}

void ActionPickupRelic::dropRelic(const MapPos &deathPosition)
{
    if (!m_relicShared) return;

    Unit::Ptr relic = m_relicShared;
    m_relicShared.reset();
    m_pickedUp = false;

    // Put relic back on the map
    relic->setHidden(false);
    relic->setPosition(deathPosition, false);
    DBG << "Relic dropped at" << deathPosition;

    // Decrement player's relic count
    Unit::Ptr monk = m_unit.lock();
    if (monk) {
        auto owner = monk->player().lock();
        if (owner) {
            float current = owner->resourcesAvailable(genie::ResourceType::RelicsCaptured);
            if (current > 0) {
                owner->setAvailableResource(genie::ResourceType::RelicsCaptured, current - 1.f);
            }
        }
    }
}
