#pragma once

#include "IAction.h"
#include "core/Types.h"
#include "mechanics/Unit.h"

class ActionAttackMove : public IAction
{
public:
    ActionAttackMove(const Unit::Ptr &unit, const MapPos &destination);

    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Attack; }
    UnitState unitState() const override { return UnitState::Moving; }

private:
    Unit::Ptr findEnemyInRange(const Unit::Ptr &unit) const;

    MapPos m_destination;
    bool m_moveQueued = false;
    bool m_attacking = false;
};
