#include "ActionGather.h"

#include "IAction.h"
#include "ActionAttack.h"
#include "ActionMove.h"
#include "core/Logger.h"
#include "core/ResourceMap.h"
#include "mechanics/Player.h"
#include "mechanics/UnitManager.h"

#include <genie/dat/Unit.h>
#include <genie/dat/UnitCommand.h>
#include <genie/dat/unit/Action.h>
#include <algorithm>
#include <limits>
#include <utility>

ActionGather::ActionGather(const std::shared_ptr<Unit> &unit, const Task &task) :
    IAction(Type::Gather, unit, task)
{
    Unit::Ptr target = task.target.lock();
    if (!target) {
        WARN << "no target target";
        return;
    }
    m_target = target;
    m_resourceType = genie::ResourceType(m_task.data->ResourceIn);
    DBG << unit->debugName << "gathering from" << target->debugName;

    if (m_task.data->ResourceOut >= 0 && m_task.data->ResourceOut < int(genie::ResourceType::NumberOfTypes)) {
        m_resourceType = genie::ResourceType(m_task.data->ResourceOut);
    }
}

IAction::UpdateResult ActionGather::update(Time time)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) {
        WARN << "Missed my own unit";
        return UpdateResult::Completed;
    }

    Unit::Ptr target = m_target.lock();

    if (!target) {
        WARN << "gather target gone";
        if (unit->resources[m_resourceType] == 0) {
            return UpdateResult::Completed;
        }

        return maybeDropOff(unit);
    }


    if (target->healthLeft() > 0 &&
        (target->playerId() != unit->playerId() || target->data()->Class == genie::Unit::DomesticAnimal)) {
        DBG << "Unit isn't dead, attacking first";
        unit->actions.prependAction(std::make_shared<ActionAttack>(unit, m_task));
        return UpdateResult::NotUpdated;
    }

    if (!m_prevTime) {
        ScreenPos screenPosition = unit->position().toScreen();
        ScreenPos targetScreenPosition = target->position().toScreen();
        unit->setAngle(screenPosition.angleTo(targetScreenPosition));

        m_prevTime = time;
        return UpdateResult::NotUpdated;
    }

    if (unit->resources[m_resourceType] >= unit->data()->ResourceCapacity || target->resources[m_resourceType] == 0) {
        if (target->resources[m_resourceType] == 0) {
            DBG << target->debugName << "is empty" << target->resources[m_resourceType];
            const int16_t deadId = target->data()->DeadUnitID;
            if (deadId >= 0) {
                auto owner = target->player().lock();
                if (owner) {
                    const genie::Unit &deadData = owner->civilization.unitData(deadId);
                    target->setUnitData(deadData);
                }
            }
        } else {
            DBG << unit->debugName << "is full" << unit->resources[m_resourceType] << "/" << unit->data()->ResourceCapacity;
        }

        return maybeDropOff(unit);
    }


    // Dead animal carcasses decay at 0.25 food/sec (AoE2 behavior)
    if (target->healthLeft() <= 0 && m_resourceType == genie::ResourceType::FoodStorage) {
        const float decayRate = 0.25f;
        const float elapsed = (time - m_prevTime) * 0.001f;
        const float decayAmount = decayRate * elapsed;
        target->resources[m_resourceType] = std::max(target->resources[m_resourceType] - decayAmount, 0.f);
    }

    float amount = unit->data()->Action.WorkRate * m_task.data->WorkValue1;
    if (m_task.data->ResourceMultiplier >= 0) {
        Player::Ptr player = unit->player().lock();
        if (!player) {
            WARN << "player gone";
            return UpdateResult::Completed;
        }

        amount *= player->resourcesAvailable(genie::ResourceType(m_task.data->ResourceMultiplier));
    }

    amount *= (time - m_prevTime) * 0.0015;
    m_prevTime = time;

    amount = std::min(amount, target->resources[m_resourceType]);

    target->resources[m_resourceType] -= amount;
    unit->resources[m_resourceType] += amount;

    return UpdateResult::Updated;
}

IAction::UnitState ActionGather::unitState() const
{
    if (m_task.data->ActionType == genie::ActionType::Hunt) {
        return UnitState::Working;
    } else {
        return UnitState::Proceeding;
    }
}

genie::ActionType ActionGather::taskType() const
{
    return m_task.data->ActionType;
}

IAction::UpdateResult ActionGather::maybeDropOff(const std::shared_ptr<Unit> &unit)
{
    const Unit::Ptr dropSite = findDropSite(unit);
    if (!dropSite) {
        WARN << "Couldn't even find a drop site!";
        return UpdateResult::Completed;
    }

    DBG << "moving to" << dropSite->position() << "to drop off, then returning to" << unit->position();

    // Bleh, will be fucked if there's more in the queue, but I'm lazy
    unit->actions.queueAction(ActionMove::moveUnitTo(unit, dropSite));
    Task dropoffTask = m_task;
    dropoffTask.target = dropSite;
    unit->actions.queueAction(std::make_shared<ActionDropOff>(unit, dropoffTask));
    unit->actions.queueAction(ActionMove::moveUnitTo(unit, unit->position(), m_task));

    Unit::Ptr target = m_target.lock();
    if (target && target->resources[m_resourceType] > 0) {
        // Target still has resources -- return to it after drop-off
        unit->actions.queueAction(std::make_shared<ActionGather>(unit, m_task));
    } else {
        // Target depleted -- find nearest fish/resource of same type
        Unit::Ptr nextTarget = findNextGatherTarget(unit);
        if (nextTarget) {
            Task newTask = m_task;
            newTask.target = nextTarget;
            unit->actions.queueAction(ActionMove::moveUnitTo(unit, nextTarget));
            unit->actions.queueAction(std::make_shared<ActionGather>(unit, newTask));
        }
    }

    return UpdateResult::Completed;
}

std::shared_ptr<Unit> ActionGather::findDropSite(const std::shared_ptr<Unit> &unit)
{
    float closestDistance = std::numeric_limits<float>::max();
    MapPos closestPos = unit->position(); // fallback
    Unit::Ptr closestUnit;

    for (const Unit::Ptr &other : unit->unitManager().units()) {
        bool foundSite = false;
        for (const uint16_t dropUnitId : unit->data()->Action.DropSites) {
            if (other->data()->ID == dropUnitId) {
                foundSite = true;
                break;
            }
        }
        if (!foundSite) {
            continue;
        }

        const float distance = unit->position().distance(other->position());
        if (distance > closestDistance) {
            continue;
        }

        closestDistance = distance;
        closestPos = other->position();
        closestUnit = other;
    }

    return closestUnit;
}

std::shared_ptr<Unit> ActionGather::findNextGatherTarget(const std::shared_ptr<Unit> &unit)
{
    Unit::Ptr oldTarget = m_target.lock();
    const int targetClass = oldTarget ? oldTarget->data()->Class : -1;

    float closestDistance = std::numeric_limits<float>::max();
    Unit::Ptr closestUnit;

    for (const Unit::Ptr &other : unit->unitManager().units()) {
        if (other == oldTarget) {
            continue; // skip the depleted one
        }

        // Must be same class (e.g. OceanFish, DeepSeaFish, ShoreFish)
        if (targetClass >= 0 && other->data()->Class != targetClass) {
            continue;
        }

        // Must have the resource we're gathering
        if (other->resources[m_resourceType] <= 0) {
            continue;
        }

        // Must be alive (not a dead fish carcass)
        if (other->isDead() || other->isDying()) {
            continue;
        }

        const float distance = unit->position().distance(other->position());
        if (distance < closestDistance) {
            closestDistance = distance;
            closestUnit = other;
        }
    }

    return closestUnit;
}


ActionDropOff::ActionDropOff(const std::shared_ptr<Unit> &unit, const Task &task) :
    IAction(Type::DropOff, unit, task)
{
    Unit::Ptr target = task.target.lock();
    if (!target) {
        WARN << "no dropoff target";
        return;
    }
    m_target = target;
    m_resourceType = genie::ResourceType(m_task.data->ResourceIn);
    DBG << unit->debugName << "dropping off" << target->debugName;

    if (m_task.data->ResourceOut >= 0 && m_task.data->ResourceOut < int(genie::ResourceType::NumberOfTypes)) {
        m_resourceType = genie::ResourceType(m_task.data->ResourceOut);
    }
}

IAction::UpdateResult ActionDropOff::update(Time /*time*/)
{
    // TODO check if we need to move closer

    Unit::Ptr unit = m_unit.lock();
    if (!unit) {
        WARN << "Unit gone";
        return UpdateResult::Completed;
    }

    Unit::Ptr target = m_target.lock();
    if (!target) {
        WARN << "dropoff target gone";
        return UpdateResult::Completed;
    }

    Player::Ptr targetPlayer = target->player().lock();
    if (!targetPlayer) {
        WARN << "player gone";
        return UpdateResult::Completed;
    }

    DBG << "dropping off" << unit->resources[m_resourceType] << "resource of type" << m_resourceType;

    targetPlayer->totalResourcesGathered += unit->resources[m_resourceType];
    targetPlayer->addResource(m_resourceType, unit->resources[m_resourceType]);
    unit->resources[m_resourceType] = 0;

    return UpdateResult::Completed;
}

genie::ActionType ActionDropOff::taskType() const
{
    return m_task.data->ActionType;
}
