#include "BasicAI.h"

#include "AiPlayer.h"
#include "mechanics/Unit.h"
#include "mechanics/Building.h"
#include "mechanics/UnitManager.h"
#include "mechanics/UnitFactory.h"
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
    trainMilitary();
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
