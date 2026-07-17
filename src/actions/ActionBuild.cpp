#include "ActionBuild.h"

#include "ActionGather.h"
#include "ActionMove.h"
#include "core/Logger.h"
#include "mechanics/Building.h"
#include "mechanics/Player.h"
#include "mechanics/UnitManager.h"

#include <genie/dat/Unit.h>

ActionBuild::ActionBuild(const Unit::Ptr &builder, const Task &task) :
    IAction(Type::Build, builder, task)
{
    Building::Ptr building = Building::fromUnit(task.target);
    if (!building) {
        WARN << "target building gone";
    }
    m_targetBuilding = building;
    DBG << builder->debugName << "building" << building->debugName;

    // Save previous gathering task so we can restore it after building
    if (builder->actions.currentAction() &&
        builder->actions.currentAction()->taskType() == genie::ActionType::GatherRebuild) {
        m_previousTask = builder->actions.currentAction()->task();
        m_previousTarget = m_previousTask.target;
    }
}

ActionBuild::~ActionBuild()
{
    Building::Ptr building = m_targetBuilding.lock();

    if (building && m_prevTime) {
        building->constructors--;
    }
}

IAction::UpdateResult ActionBuild::update(Time time)
{
    Unit::Ptr unit = m_unit.lock();
    if (!unit) {
        WARN << "Unit gone";
        return UpdateResult::Completed;
    }

    Building::Ptr building = m_targetBuilding.lock();
    if (!building) {
        return UpdateResult::Completed;
    }

    if (!m_prevTime) {
        m_prevTime = time;
        building->constructors++;
        return UpdateResult::NotUpdated;
    }

    if (building->creationProgress() >= 1.) {
        return UpdateResult::Completed;
    }

    float progress = 3. / (building->constructors + 2.);
    progress *= (time - m_prevTime) * 0.0015;
    m_prevTime = time;

    building->increaseCreationProgress(progress);

    if (building->creationProgress() >= 1.) {
        DBG << "building finished";

        // Drop off carried resources when finishing a drop-off building
        int buildingId = building->data()->ID;
        if (buildingId == 562 || buildingId == 68 || buildingId == 584 || buildingId == 109) {
            Player::Ptr owner = unit->player().lock();
            if (owner) {
                for (auto &res : unit->resources) {
                    if (res.second > 0) {
                        owner->setAvailableResource(res.first,
                            owner->resourcesAvailable(res.first) + res.second);
                        res.second = 0;
                    }
                }
            }
        }

        // Auto-gather: after building drop-off site, gather nearest matching resource
        // Lumber Camp=562, Mill=68, Mining Camp=584
        if (buildingId == 562 || buildingId == 68 || buildingId == 584) {
            // Find nearest gatherable resource
            Unit::Ptr bestTarget;
            float bestDist = 999999;
            for (const Unit::Ptr &target : unit->unitManager().units()) {
                if (!target || target->isDead() || target->isDying()) continue;
                if (!target->data()->CanBeGathered) continue;
                if (target->playerId() != 0 && target->playerId() != unit->playerId()) continue;

                // Match resource type to building
                bool match = false;
                if (buildingId == 562) { // Lumber Camp → trees (class 15=Tree)
                    match = (target->data()->Class == genie::Unit::Tree);
                } else if (buildingId == 68) { // Mill → berries/huntables
                    match = (target->data()->Class == genie::Unit::BerryBush ||
                             target->data()->Class == genie::Unit::PreyAnimal ||
                             target->data()->Class == genie::Unit::DomesticAnimal);
                } else if (buildingId == 584) { // Mining Camp → gold/stone
                    match = (target->data()->Class == genie::Unit::GoldMine ||
                             target->data()->Class == genie::Unit::StoneMine);
                }
                if (!match) continue;

                float dist = unit->distanceTo(target);
                if (dist < bestDist) {
                    bestDist = dist;
                    bestTarget = target;
                }
            }

            if (bestTarget) {
                Task gatherTask = unit->actions.findTaskWithTarget(bestTarget);
                if (gatherTask.isValid()) {
                    unit->actions.queueAction(std::make_shared<ActionGather>(unit, gatherTask));
                }
            }
        } else if (m_previousTask.isValid()) {
            // Non-drop-off building: restore previous gathering task
            Unit::Ptr prevTarget = m_previousTarget.lock();
            if (prevTarget && !prevTarget->isDead() && !prevTarget->isDying()) {
                unit->actions.queueAction(ActionMove::moveUnitTo(unit, prevTarget->position(), m_previousTask));
                ActionPtr gatherAction = std::make_shared<ActionGather>(unit, m_previousTask);
                gatherAction->requiredUnitID = m_previousTask.unitId;
                unit->actions.queueAction(gatherAction);
                DBG << "Restoring previous gather task after building";
            }
        }

        return UpdateResult::Completed;
    }

    return UpdateResult::Updated;
}

IAction::UnitState ActionBuild::unitState() const
{
    Building::Ptr target = m_targetBuilding.lock();
    if (!target) {
        WARN << "target lost";
        return Idle;
    }

    if (target->data()->ID == Unit::Farm) {
        return Working;
    }

    return Proceeding;
}
