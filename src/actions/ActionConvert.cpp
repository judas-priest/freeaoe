#include "ActionConvert.h"
#include "ActionMove.h"

#include <genie/dat/Unit.h>

#include "core/Logger.h"
#include "mechanics/Player.h"

ActionConvert::ActionConvert(const Unit::Ptr &monk, const Unit::Ptr &target)
    : IAction(Type::Convert, monk, Task())
    , m_target(target)
{
}

ActionConvert::UpdateResult ActionConvert::update(Time time)
{
    Unit::Ptr monk = m_unit.lock();
    Unit::Ptr target = m_target.lock();
    if (!monk || !target) return UpdateResult::Completed;

    // Target already converted or dead
    if (target->playerId() == monk->playerId() || target->hitpointsLeft() <= 0) {
        return UpdateResult::Completed;
    }

    float dist = monk->position().distance(target->position());

    // Move closer if too far
    if (dist > CONVERT_RANGE && !m_isMoving) {
        m_isMoving = true;
        auto move = ActionMove::moveUnitTo(monk, target->position());
        if (move) {
            monk->actions.queueAction(move);
        }
        return UpdateResult::Updated;
    }

    if (dist <= CONVERT_RANGE) {
        m_isMoving = false;
    }

    // Start conversion timer
    if (!m_converting && !m_isMoving) {
        m_converting = true;
        m_convertStartTime = time;

        // Randomize conversion time (4-10 seconds base, longer for siege/buildings)
        m_convertDuration = 4000 + (rand() % 6000); // 4-10 sec
        // Siege units resist longer
        if (target->data()->Class == genie::Unit::SiegeWeapon ||
            target->data()->Type == genie::Unit::BuildingType) {
            m_convertDuration += 8000; // +8 sec for siege/buildings
        }

        DBG << monk->debugName << "starting conversion of" << target->debugName << "duration=" << m_convertDuration;
        return UpdateResult::Updated;
    }

    // Check if conversion complete
    if (m_converting && time - m_convertStartTime >= m_convertDuration) {
        // Convert: change target's owner to monk's owner
        auto monkOwner = monk->player().lock();
        if (monkOwner) {
            DBG << "Converted" << target->debugName << "to player" << monkOwner->playerId;
            target->setPlayer(monkOwner);
        }
        return UpdateResult::Completed;
    }

    return UpdateResult::NotUpdated;
}
