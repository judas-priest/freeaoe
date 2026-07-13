#include "BasicAI.h"

#include "AiPlayer.h"
#include "actions/IAction.h"
#include "actions/ActionMove.h"
#include "mechanics/Unit.h"
#include "mechanics/Building.h"
#include "mechanics/UnitManager.h"
#include "mechanics/UnitFactory.h"
#include "mechanics/Map.h"
#include "core/Constants.h"
#include "core/Logger.h"

#include <genie/dat/Unit.h>
#include <genie/dat/ResourceType.h>

#include <cmath>
#include <cstdlib>

BasicAI::BasicAI(AiPlayer *player, UnitManager *unitManager)
    : m_player(player), m_unitManager(unitManager)
{
}

int BasicAI::countUnitsOfType(int unitId) const
{
    int count = 0;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (unit && unit->playerId() == m_player->playerId && unit->data()->ID == unitId) {
            count++;
        }
    }
    return count;
}

int BasicAI::countBuildingsOfType(int buildingId) const
{
    int count = 0;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (unit && unit->playerId() == m_player->playerId &&
            unit->data()->ID == buildingId && unit->isBuilding()) {
            count++;
        }
    }
    return count;
}

void BasicAI::update(Time time)
{
    if (time - m_lastUpdate < 5000) return; // Every 5 seconds
    m_lastUpdate = time;

    if (!m_player || !m_player->alive) return;

    trainVillagers();
    buildHouses();
    buildDropOffSites();
    assignIdleVillagers();
    researchLoom();
    advanceAge();
    trainMilitary();
    attackWithArmy();
}

void BasicAI::trainVillagers()
{
    // Train villagers up to 20, max 1 in queue at a time
    int villagerCount = countUnitsOfType(83);
    if (villagerCount >= 20) return;

    // Find TC that isn't already producing
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 109) continue; // Town Center
        auto building = Building::fromUnit(unit);
        if (!building) continue;
        if (building->isProducing()) continue; // Already producing

        const genie::Unit &villagerData = m_player->civilization.unitData(83);
        building->enqueueProduceUnit(&villagerData);
        return;
    }
}

void BasicAI::buildHouses()
{
    // Check population headroom
    float popCurrent = m_player->resourcesAvailable(genie::ResourceType::CurrentPopulation);
    float popCap = m_player->resourcesAvailable(genie::ResourceType::PopulationHeadroom);

    if (popCap - popCurrent > 5) return; // Enough room

    // Need more houses. Find a villager and build a house near TC
    float wood = m_player->resourcesAvailable(genie::ResourceType::WoodStorage);
    if (wood < 30) return; // Can't afford

    // Find TC position
    MapPos tcPos;
    bool foundTC = false;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (unit && unit->playerId() == m_player->playerId && unit->data()->ID == 109) {
            tcPos = unit->position();
            foundTC = true;
            break;
        }
    }
    if (!foundTC) return;

    // Find idle villager
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 83) continue; // Villager
        if (unit->actions.currentAction()) continue; // Busy

        // Place house near TC at random offset
        float ox = (rand() % 10 - 5) * Constants::TILE_SIZE;
        float oy = (rand() % 10 - 5) * Constants::TILE_SIZE;
        MapPos housePos(tcPos.x + ox, tcPos.y + oy);

        // Check for existing buildings at this position
        int tileX = housePos.x / Constants::TILE_SIZE;
        int tileY = housePos.y / Constants::TILE_SIZE;
        bool blocked = false;
        for (int dy = -1; dy <= 1 && !blocked; dy++) {
            for (int dx = -1; dx <= 1 && !blocked; dx++) {
                const auto &entities = m_unitManager->map()->entitiesAt(tileX + dx, tileY + dy);
                for (const auto &e : entities) {
                    if (e.lock()) { blocked = true; break; }
                }
            }
        }
        if (blocked) continue;

        // Create house (ID 70)
        Unit::Ptr house = UnitFactory::createUnit(70, std::const_pointer_cast<Player>(
            std::static_pointer_cast<const Player>(unit->player().lock())), *m_unitManager);
        if (house) {
            m_unitManager->add(house, housePos);
            // Task villager to build
            Task buildTask;
            buildTask.data = &unit->data()->Action.TaskList[0]; // rough
            IAction::assignTask(buildTask, unit, IAction::AssignType::Replace);
        }
        return;
    }
}

void BasicAI::assignIdleVillagers()
{
    // Find idle villagers and assign them to gather nearest resource
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 83) continue; // Male villager
        if (unit->actions.currentAction()) continue; // Already busy

        // Find nearest gatherable resource (tree=349, berry=59, gold=66, stone=102, sheep=594)
        Unit::Ptr bestTarget;
        float bestDist = 999999;

        for (const Unit::Ptr &target : m_unitManager->units()) {
            if (!target || target->isDead()) continue;
            // Gatherable Gaia resources or own dead animals
            bool isGatherableGaia = (target->playerId() == 0) && target->data()->CanBeGathered;
            bool isOwnDeadAnimal = (target->playerId() == m_player->playerId) &&
                                    target->data()->Class == genie::Unit::DomesticAnimal;
            if (!isGatherableGaia && !isOwnDeadAnimal) continue;

            float dist = unit->distanceTo(target);
            if (dist < bestDist) {
                bestDist = dist;
                bestTarget = target;
            }
        }

        if (bestTarget) {
            Task task = unit->actions.findTaskWithTarget(bestTarget);
            if (task.isValid()) {
                IAction::assignTask(task, unit, IAction::AssignType::Replace);
            }
        }
    }
}

void BasicAI::buildDropOffSites()
{
    float wood = m_player->resourcesAvailable(genie::ResourceType::WoodStorage);
    if (wood < 100) return;

    // Build Lumber Camp (562, costs 100W) if we don't have one
    if (countBuildingsOfType(562) == 0) {
        // Find nearest tree cluster to TC
        MapPos tcPos;
        bool foundTC = false;
        for (const Unit::Ptr &unit : m_unitManager->units()) {
            if (unit && unit->playerId() == m_player->playerId && unit->data()->ID == 109) {
                tcPos = unit->position();
                foundTC = true;
                break;
            }
        }
        if (!foundTC) return;

        // Find nearest tree
        Unit::Ptr nearestTree;
        float bestDist = 999999;
        for (const Unit::Ptr &unit : m_unitManager->units()) {
            if (!unit || unit->isDead()) continue;
            if (unit->data()->Class != genie::Unit::Tree) continue;
            float dist = std::abs(unit->position().x - tcPos.x) + std::abs(unit->position().y - tcPos.y);
            if (dist < bestDist && dist > Constants::TILE_SIZE * 3) { // Not too close to TC
                bestDist = dist;
                nearestTree = unit;
            }
        }

        if (nearestTree) {
            // Build lumber camp near the tree
            MapPos buildPos = nearestTree->position();
            buildPos.x += Constants::TILE_SIZE * 2;
            buildStructure(562, 100);
        }
    }

    // Build Mining Camp (584, costs 100W) near gold if we don't have one
    if (countBuildingsOfType(584) == 0 && wood >= 100) {
        buildStructure(584, 100);
    }
}

void BasicAI::researchLoom()
{
    // Research Loom (tech 22 in HD dat) at TC — makes villagers harder to kill
    // In HD Edition, tech 22 = Loom (same ID as Feudal in some versions)
    // Try both common IDs
    int loomId = -1;
    for (int id : {22, 8}) {
        const genie::Tech &t = m_player->civilization.tech(id);
        if (t.Type == 0 && t.ResearchLocation == 109 && t.ResearchTime > 0) {
            loomId = id;
            break;
        }
    }
    if (loomId < 0 || !m_player->canAffordResearch(loomId)) return;

    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 109) continue;
        auto building = Building::fromUnit(unit);
        if (!building) continue;
        if (building->isProducing() || building->isResearching()) continue;

        const genie::Tech &loom = m_player->civilization.tech(loomId);
        if (loom.ResearchTime > 0) {
            building->enqueueProduceResearch(&loom);
        }
        return;
    }
}

void BasicAI::advanceAge()
{
    // Research age advance at TC when affordable
    // Feudal=tech 22 (500F), Castle=tech 102 (800F,200G), Imperial=tech 103 (1000F,800G)
    static const int ageTechs[] = { 22, 102, 103 };

    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 109) continue; // Town Center
        auto building = Building::fromUnit(unit);
        if (!building) continue;
        if (building->isProducing() || building->isResearching()) continue;

        for (int techId : ageTechs) {
            if (m_player->canAffordResearch(techId)) {
                const genie::Tech &tech = m_player->civilization.tech(techId);
                if (tech.ResearchTime > 0) { // valid tech
                    building->enqueueProduceResearch(&tech);
                    return;
                }
            }
        }
        return;
    }
}

void BasicAI::attackWithArmy()
{
    // Attack when we have 5+ idle military units
    int idleMilitary = 0;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 74 && unit->data()->ID != 93 && unit->data()->ID != 4) continue; // militia, spearman, archer
        if (unit->actions.currentAction()) continue;
        idleMilitary++;
    }
    if (idleMilitary < 5) return;

    // Find enemy target — prefer TC, otherwise any enemy building/unit
    Unit::Ptr target;
    for (const Unit::Ptr &enemy : m_unitManager->units()) {
        if (!enemy || enemy->playerId() == m_player->playerId || enemy->playerId() == 0) continue;
        if (enemy->isDead() || enemy->isDying()) continue;
        if (enemy->data()->ID == 109) { target = enemy; break; } // Enemy TC — priority
        if (!target) target = enemy;
    }
    if (!target) return;

    // Send all idle military to attack
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 74 && unit->data()->ID != 93 && unit->data()->ID != 4) continue;
        if (unit->actions.currentAction()) continue;

        Task task = unit->actions.findTaskWithTarget(target);
        if (task.isValid()) {
            IAction::assignTask(task, unit, IAction::AssignType::Replace);
        } else {
            // Just move toward the enemy
            unit->actions.setCurrentAction(ActionMove::moveUnitTo(unit, target->position()));
        }
    }
}

void BasicAI::trainMilitary()
{
    // Train militia (ID 74) from barracks (ID 12)
    int militaryCount = countUnitsOfType(74);
    if (militaryCount >= 10) return;

    float food = m_player->resourcesAvailable(genie::ResourceType::FoodStorage);
    if (food < 60) return;

    // First check if we have a barracks, if not try to build one
    int barracksCount = countBuildingsOfType(12);
    if (barracksCount == 0) {
        float wood = m_player->resourcesAvailable(genie::ResourceType::WoodStorage);
        if (wood >= 175) {
            buildStructure(12, 175); // Barracks costs 175 wood
        }
        return;
    }

    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 12) continue; // Barracks
        auto building = Building::fromUnit(unit);
        if (!building) continue;
        if (building->isProducing()) continue;

        const genie::Unit &militiaData = m_player->civilization.unitData(74);
        building->enqueueProduceUnit(&militiaData);
        return;
    }
}

void BasicAI::buildStructure(int buildingId, int woodCost)
{
    float wood = m_player->resourcesAvailable(genie::ResourceType::WoodStorage);
    if (wood < woodCost) return;

    // Find TC for reference position
    MapPos tcPos;
    bool foundTC = false;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (unit && unit->playerId() == m_player->playerId && unit->data()->ID == 109) {
            tcPos = unit->position();
            foundTC = true;
            break;
        }
    }
    if (!foundTC) return;

    // Find valid placement — spiral outward from TC
    for (int radius = 3; radius < 12; radius++) {
        for (int attempt = 0; attempt < 8; attempt++) {
            float angle = attempt * M_PI / 4.0f + (rand() % 100) / 100.f;
            float ox = cos(angle) * radius * Constants::TILE_SIZE;
            float oy = sin(angle) * radius * Constants::TILE_SIZE;
            MapPos buildPos(tcPos.x + ox, tcPos.y + oy);

            // Check bounds
            if (buildPos.x < Constants::TILE_SIZE * 5 || buildPos.y < Constants::TILE_SIZE * 5) continue;
            if (buildPos.x >= m_unitManager->map()->pixelWidth() - Constants::TILE_SIZE * 5) continue;
            if (buildPos.y >= m_unitManager->map()->pixelHeight() - Constants::TILE_SIZE * 5) continue;

            // Check for existing buildings/units at this position
            int tileX = buildPos.x / Constants::TILE_SIZE;
            int tileY = buildPos.y / Constants::TILE_SIZE;
            bool blocked = false;
            for (int dy = -2; dy <= 2 && !blocked; dy++) {
                for (int dx = -2; dx <= 2 && !blocked; dx++) {
                    const auto &entities = m_unitManager->map()->entitiesAt(tileX + dx, tileY + dy);
                    for (const auto &e : entities) {
                        if (e.lock()) { blocked = true; break; }
                    }
                }
            }
            if (blocked) continue;

            // Place building
            auto owner = std::const_pointer_cast<Player>(
                std::static_pointer_cast<const Player>(
                    std::shared_ptr<Player>(m_player, [](Player*){})));

            Unit::Ptr building = UnitFactory::createUnit(buildingId, owner, *m_unitManager);
            if (building) {
                m_unitManager->add(building, buildPos);
                m_player->setAvailableResource(genie::ResourceType::WoodStorage, wood - woodCost);
                DBG << "AI built building" << buildingId << "at" << buildPos.x << buildPos.y;
                return;
            }
        }
    }
}

void BasicAI::researchTechs()
{
    // Auto-research available techs at buildings
    // TODO: prioritize important techs
}
