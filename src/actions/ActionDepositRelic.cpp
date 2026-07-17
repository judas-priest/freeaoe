#include "ActionDepositRelic.h"
#include "ActionMove.h"
#include "ActionPickupRelic.h"
#include "mechanics/Building.h"
#include "mechanics/Player.h"
#include "core/Constants.h"
#include "core/Logger.h"

ActionDepositRelic::ActionDepositRelic(const Unit::Ptr &monk, const Unit::Ptr &monastery, const Task &task)
    : IAction(Type::PickupRelic, monk, task)
    , m_monastery(monastery)
{
}

IAction::UpdateResult ActionDepositRelic::update(Time time)
{
    (void)time;
    auto monk = m_unit.lock();
    if (!monk) return UpdateResult::Failed;

    auto monastery = m_monastery.lock();
    if (!monastery || !monastery->isAlive()) {
        WARN << "No monastery to deposit relic";
        return UpdateResult::Failed;
    }

    float dist = monk->distanceTo(monastery);
    if (dist > 2.f * Constants::TILE_SIZE && !m_moving) {
        m_moving = true;
        auto moveAction = ActionMove::moveUnitTo(monk, monastery);
        if (moveAction) {
            monk->actions.prependAction(moveAction);
        }
        return UpdateResult::NotUpdated;
    }

    if (dist > 2.f * Constants::TILE_SIZE) {
        return UpdateResult::NotUpdated; // still moving
    }

    // Find the PickupRelic action that holds the relic
    ActionPickupRelic *pickupAction = nullptr;
    auto currentAction = monk->actions.currentAction();
    // Check queued actions for the pickup action
    for (auto &queued : monk->actions.m_actionQueue) {
        if (queued->type == Type::PickupRelic) {
            pickupAction = dynamic_cast<ActionPickupRelic*>(queued.get());
            if (pickupAction) break;
        }
    }

    if (!pickupAction) {
        WARN << "Monk has no PickupRelic action (no relic carried)";
        return UpdateResult::Failed;
    }

    // Deposit: transfer relic to monastery garrison
    auto building = Building::fromUnit(monastery);
    if (!building) return UpdateResult::Failed;

    // Drop relic at monastery position (unhides it) then garrison it
    pickupAction->dropRelic(monastery->position());

    // The relic is now visible at monastery position — find and garrison it
    // For simplicity, just mark the deposit as complete.
    // The monastery's gold generation counts relics from garrisonedUnits,
    // so we need a different approach: increment the relic counter directly.
    auto owner = monk->player().lock();
    if (owner) {
        // The dropRelic already decremented RelicsCaptured, so re-add it
        // as a "deposited relic" (monastery generates gold based on RelicsCaptured)
        owner->setAvailableResource(genie::ResourceType::RelicsCaptured,
            owner->resourcesAvailable(genie::ResourceType::RelicsCaptured) + 1);
    }

    // Remove the pickup action from queue
    monk->actions.removeAction(
        *std::find_if(monk->actions.m_actionQueue.begin(),
                      monk->actions.m_actionQueue.end(),
                      [](const ActionPtr &a) { return a->type == Type::PickupRelic; }));

    DBG << "Relic deposited in monastery";
    return UpdateResult::Completed;
}
