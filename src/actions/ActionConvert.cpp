#include "ActionConvert.h"
#include "ActionMove.h"

#include <genie/dat/Unit.h>
#include <genie/dat/ResourceType.h>

#include <algorithm>
#include <cstdlib>

#include "core/Logger.h"
#include "mechanics/Player.h"
#include "net/SyncRandom.h"

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

    // Cannot convert without faith
    const float faith = monk->resources[genie::ResourceType::Faith];
    if (!m_converting && faith < 1.f) {
        // Recharge faith passively
        float &f = monk->resources[genie::ResourceType::Faith];
        if (f < FAITH_MAX) {
            // Illumination (tech 233): monk's owner has researched — double recharge rate
            float rechargeRate = FAITH_RECHARGE_PER_MS;
            auto monkOwner = monk->player().lock();
            if (monkOwner && monkOwner->hasResearched(233)) {
                rechargeRate *= 2.0f;
            }
            const float delta = (time - m_prevTime) * rechargeRate;
            f = std::min(FAITH_MAX, f + delta);
        }
        m_prevTime = time;
        return UpdateResult::NotUpdated;
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

    // Start conversion attempt
    if (!m_converting && !m_isMoving) {
        m_convertStartTime = time;
        m_converting = true;
        m_prevRollTime = time;

        // Base window: 4s min, 10s max
        m_minConvertTime = 4000;
        m_maxConvertTime = 10000;

        // Cavalry resistance: +3s
        if (target->data()->Class == genie::Unit::Cavalry ||
            target->data()->Class == genie::Unit::CavalryArcher) {
            m_minConvertTime += 3000;
            m_maxConvertTime += 3000;
        }

        // Siege/building resistance: +5s
        if (target->data()->Class == genie::Unit::SiegeWeapon ||
            target->data()->Type == genie::Unit::BuildingType) {
            m_minConvertTime += 5000;
            m_maxConvertTime += 5000;
        }

        // Faith (tech 45): target's owner has researched — 50% longer conversion
        {
            auto targetOwner = target->player().lock();
            if (targetOwner && targetOwner->hasResearched(45)) {
                m_minConvertTime = m_minConvertTime * 3 / 2;
                m_maxConvertTime = m_maxConvertTime * 3 / 2;
            }
        }

        DBG << monk->debugName << "starting conversion of" << target->debugName
            << "window=[" << m_minConvertTime << "," << m_maxConvertTime << "]ms";
        return UpdateResult::Updated;
    }

    // Probability ramp during conversion
    if (m_converting) {
        const Time elapsed = time - m_convertStartTime;

        // Grace period — no chance yet
        if (elapsed < m_minConvertTime) {
            return UpdateResult::NotUpdated;
        }

        // Past max — guaranteed
        if (elapsed >= m_maxConvertTime) {
            doConvert(monk, target);
            return UpdateResult::Completed;
        }

        // Linear ramp: roll every ~200ms
        if (time - m_prevRollTime >= 200) {
            m_prevRollTime = time;
            const float window = float(m_maxConvertTime - m_minConvertTime);
            const float progress = float(elapsed - m_minConvertTime) / window;
            const int roll = SyncRandom::inst().nextInt(1000);
            if (roll < int(progress * 1000.f)) {
                doConvert(monk, target);
                return UpdateResult::Completed;
            }
        }
    }

    return UpdateResult::NotUpdated;
}

void ActionConvert::doConvert(const Unit::Ptr &monk, const Unit::Ptr &target)
{
    auto monkOwner = monk->player().lock();
    if (!monkOwner) return;

    // Heresy (tech 439): target's owner has researched — unit dies instead of converting
    auto targetOwner = target->player().lock();
    if (targetOwner && targetOwner->hasResearched(439)) {
        DBG << "Heresy: killing" << target->debugName << "instead of converting";
        target->kill();
        monk->resources[genie::ResourceType::Faith] = 0.f;
        return;
    }

    DBG << "Converted" << target->debugName << "to player" << monkOwner->playerId;
    target->setPlayer(monkOwner);

    // Drain monk's faith to 0
    monk->resources[genie::ResourceType::Faith] = 0.f;
}
