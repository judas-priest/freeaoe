#pragma once

#include "IAction.h"
#include "mechanics/Unit.h"

// Monk picks up a relic and carries it. When deposited in monastery, generates gold.
class ActionPickupRelic : public IAction
{
public:
    ActionPickupRelic(const Unit::Ptr &monk, const Unit::Ptr &relic);
    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::PickupRelic; }

private:
    std::weak_ptr<Unit> m_relic;
    bool m_isMoving = false;
    bool m_pickedUp = false;
    static constexpr float PICKUP_RANGE = 16.f;
};
