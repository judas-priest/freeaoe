#include "ActionGarrison.h"
#include "ActionMove.h"

#include "mechanics/Building.h"
#include "mechanics/Unit.h"

#include "global/EventManager.h"

#include <genie/dat/Unit.h>

ActionGarrison::ActionGarrison(const std::shared_ptr<Unit> &unit, const Task &task) :
    IAction(Type::Garrison, unit, task)
{
    Building::Ptr building = Building::fromUnit(task.target);
    if (building) {
        m_buildingTarget = building;
        m_isUnitTarget = false;
    } else {
        m_unitTarget = task.target;
        m_isUnitTarget = true;
    }
}

IAction::UpdateResult ActionGarrison::update(Time /*time*/)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) {
        WARN << "impossible, lost own unit";
        return UpdateResult::Failed;
    }

    if (m_isUnitTarget) {
        // Non-building garrison (transport ship, ram, etc.)
        Unit::Ptr target = m_unitTarget.lock();
        if (!target) {
            WARN << "garrison target lost";
            return UpdateResult::Failed;
        }

        if (target->data()->GarrisonCapacity <= 0) {
            WARN << "Unit has no garrison capacity";
            return UpdateResult::Failed;
        }

        if (unit->distanceTo(target) > 1.) {
            DBG << "Out of range, moving closer";
            unit->actions.prependAction(ActionMove::moveUnitTo(unit, target));
            return UpdateResult::Updated;
        }

        if (static_cast<int>(target->garrisonedUnits.size()) >= target->data()->GarrisonCapacity) {
            WARN << "Unit full, can't garrison";
            return UpdateResult::Failed;
        }

        target->garrisonedUnits.push_back(unit);
        unit->garrisonedInUnit = target;

        EventManager::unitGarrisoned(unit.get(), target.get());

        return UpdateResult::Completed;
    }

    // Building garrison (original path)
    Building::Ptr target = m_buildingTarget.lock();
    if (!target) {
        WARN << "garrison target lost";
        return UpdateResult::Failed;
    }

    if (target->data()->GarrisonCapacity <= 0) {
        WARN << "Building has no garrison capacity";
        return UpdateResult::Failed;
    }

    if (unit->distanceTo(target) > 1.) {
        DBG << "Out of range, moving closer";
        unit->actions.prependAction(ActionMove::moveUnitTo(unit, target));
        return UpdateResult::Updated;
    }

    if (static_cast<int>(target->garrisonedUnits.size()) >= target->data()->GarrisonCapacity) {
        WARN << "Building full, can't garrison";
        return UpdateResult::Failed;
    }

    target->garrisonedUnits.push_back(unit);
    unit->garrisonedIn = target;

    EventManager::unitGarrisoned(unit.get(), target.get());

    return UpdateResult::Completed;
}
