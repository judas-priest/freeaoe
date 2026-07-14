#pragma once

#include "IAction.h"
#include "mechanics/Unit.h"

// Monk picks up a relic and carries it. When deposited in monastery, generates gold.
class ActionPickupRelic : public IAction
{
public:
    ActionPickupRelic(const Unit::Ptr &monk, const Unit::Ptr &relic);
    ~ActionPickupRelic() override;
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::PickupRelic; }

    // Called when monk dies to drop the relic at a position
    void dropRelic(const MapPos &deathPosition);

private:
    std::weak_ptr<Unit> m_relic;
    std::shared_ptr<Unit> m_relicShared; // keeps relic alive while carried
    bool m_isMoving = false;
    bool m_pickedUp = false;
    static constexpr float PICKUP_RANGE = 16.f;
};
