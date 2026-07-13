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

ActionPickupRelic::UpdateResult ActionPickupRelic::update(Time time)
{
    (void)time;
    Unit::Ptr monk = m_unit.lock();
    Unit::Ptr relic = m_relic.lock();
    if (!monk) return UpdateResult::Completed;

    if (m_pickedUp) {
        // Relic already picked up — monk carries it (action stays active)
        // Player needs to manually garrison monk in monastery to deposit
        return UpdateResult::NotUpdated;
    }

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
        // Pick up the relic — remove it from map, monk "carries" it
        m_pickedUp = true;
        relic->kill(); // Remove relic from world
        DBG << "Monk picked up relic";

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
