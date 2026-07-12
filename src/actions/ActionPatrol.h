#pragma once

#include "IAction.h"
#include "core/Types.h"
#include "mechanics/Unit.h"

class ActionPatrol : public IAction
{
public:
    ActionPatrol(const Unit::Ptr &unit, const MapPos &destination);

    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::MoveTo; }

private:
    MapPos m_startPos;
    MapPos m_destPos;
    bool m_returning = false;
    bool m_moveQueued = false;
};
