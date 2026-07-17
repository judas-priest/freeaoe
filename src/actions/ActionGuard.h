#pragma once

#include "IAction.h"
#include "core/Types.h"
#include "mechanics/Unit.h"

class ActionGuard : public IAction
{
public:
    ActionGuard(const Unit::Ptr &unit, const Unit::Ptr &target);

    UpdateResult update(Time time) override;
    genie::ActionType taskType() const override { return genie::ActionType::Guard; }

private:
    Unit::Ptr findEnemyNearTarget(const Unit::Ptr &unit, const Unit::Ptr &guardTarget) const;

    std::weak_ptr<Unit> m_guardTarget;
    bool m_isFollowing = false;
    bool m_isAttacking = false;
    static constexpr float FOLLOW_DISTANCE = 48.f;
};
