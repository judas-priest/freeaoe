#pragma once

#include "IAction.h"
#include "core/Types.h"
#include "mechanics/Unit.h"

struct VisibilityMap;

class ActionAutoScout : public IAction
{
public:
    ActionAutoScout(const Unit::Ptr &unit);

    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Explore; }

private:
    MapPos findUnexploredTarget(const Unit::Ptr &unit);

    MapPos m_currentTarget;
    bool m_hasTarget = false;
    bool m_moveQueued = false;
    Time m_lastRetarget = 0;
};
