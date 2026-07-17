#include "ActionRepair.h"
#include "ActionMove.h"

#include <genie/dat/Unit.h>
#include <genie/dat/unit/Creatable.h>
#include <genie/dat/ResourceType.h>

#include "mechanics/Player.h"

ActionRepair::ActionRepair(const Unit::Ptr &unit, const Unit::Ptr &building)
    : IAction(Type::Repair, unit, Task())
    , m_target(building)
{
}

ActionRepair::UpdateResult ActionRepair::update(Time time)
{
    Unit::Ptr unit = m_unit.lock();
    Unit::Ptr target = m_target.lock();
    if (!unit || !target) return UpdateResult::Completed;

    // Done if fully repaired
    if (target->hitpointsLeft() >= target->data()->HitPoints) {
        return UpdateResult::Completed;
    }

    float dist = unit->position().distance(target->position());

    // Move closer if too far
    if (dist > REPAIR_RANGE && !m_isMoving) {
        m_isMoving = true;
        auto move = ActionMove::moveUnitTo(unit, target->position());
        if (move) {
            unit->actions.queueAction(move);
        }
        return UpdateResult::Updated;
    }

    if (dist <= REPAIR_RANGE) {
        m_isMoving = false;
    }

    // Repair tick — heal 1 HP per interval (negative damage = heal)
    if (!m_isMoving && time - m_lastRepairTime >= REPAIR_INTERVAL) {
        m_lastRepairTime = time;

        // Deduct per-HP repair cost from repairer's player
        Player::Ptr owner = unit->player().lock();
        if (!owner) {
            return UpdateResult::Completed;
        }

        const float maxHp = target->data()->HitPoints;
        if (maxHp <= 0) {
            return UpdateResult::Completed;
        }

        // Check if player can afford the per-HP cost for each resource
        for (const auto &cost : target->data()->Creatable.ResourceCosts) {
            if (cost.Type < 0 || cost.Amount <= 0) {
                continue;
            }
            const genie::ResourceType type = genie::ResourceType(cost.Type);
            // Only charge for wood/stone/gold (skip food for buildings, matching AoE2 behavior)
            if (type == genie::ResourceType::FoodStorage) {
                continue;
            }
            float perHpCost = cost.Amount / maxHp;
            if (owner->resourcesAvailable(type) < perHpCost) {
                return UpdateResult::Completed; // Can't afford, stop repairing
            }
        }

        // Actually deduct the cost
        for (const auto &cost : target->data()->Creatable.ResourceCosts) {
            if (cost.Type < 0 || cost.Amount <= 0) {
                continue;
            }
            const genie::ResourceType type = genie::ResourceType(cost.Type);
            if (type == genie::ResourceType::FoodStorage) {
                continue;
            }
            float perHpCost = cost.Amount / maxHp;
            owner->removeResource(type, perHpCost);
        }

        target->takeDamage(-1.f);
        return UpdateResult::Updated;
    }

    return UpdateResult::NotUpdated;
}
