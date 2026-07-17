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
    : m_player(player), m_unitManager(unitManager),
      m_params(ai::DifficultyParams::forLevel(player->difficultyLevel))
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

void BasicAI::chooseStrategy()
{
    int roll = rand() % 100;
    if (roll < 25) {
        m_strategy = Strategy::Rush;
    } else if (roll < 50) {
        m_strategy = Strategy::Boom;
    } else if (roll < 70) {
        m_strategy = Strategy::Turtle;
    } else {
        m_strategy = Strategy::Balanced;
    }

    const char *name = "Balanced";
    switch (m_strategy) {
        case Strategy::Rush:     name = "Rush"; break;
        case Strategy::Boom:     name = "Boom"; break;
        case Strategy::Turtle:   name = "Turtle"; break;
        case Strategy::Balanced: name = "Balanced"; break;
    }
    DBG << "AI player" << m_player->playerId << "chose strategy:" << name;
}

void BasicAI::update(Time time)
{
    if (time - m_lastUpdate < static_cast<Time>(m_params.updateIntervalMs)) return;
    m_lastUpdate = time;

    if (!m_player || !m_player->alive) return;

    if (!m_strategyChosen) {
        chooseStrategy();
        m_strategyChosen = true;
    }

    scoutMap();
    defendAgainstThreats();
    retreatInjuredUnits();
    garrisonVillagersUnderAttack();
    ungarrisonWhenSafe();
    trainVillagers();
    buildHouses();
    buildDropOffSites();
    buildNaval();
    buildDefenses();
    assignIdleVillagers();
    researchLoom();
    advanceAge();
    if (m_params.researchTechs) researchTechs();
    trainMilitary();
    useMonksOffensively();
    attackWithArmy();
}

void BasicAI::scoutMap()
{
    // Send idle scouts to random map positions for exploration
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 448 && unit->data()->ID != 546) continue; // Scout/LightCav
        if (unit->actions.currentAction()) continue; // Already moving

        // Pick a random position on the map
        float mapW = m_unitManager->map()->pixelWidth();
        float mapH = m_unitManager->map()->pixelHeight();
        float margin = Constants::TILE_SIZE * 5;
        MapPos scoutTarget(
            margin + (rand() % int(mapW - margin * 2)),
            margin + (rand() % int(mapH - margin * 2))
        );

        unit->actions.setCurrentAction(ActionMove::moveUnitTo(unit, scoutTarget));
        return; // One scout command per update
    }
}

void BasicAI::trainVillagers()
{
    // Train villagers up to cap, max 1 in queue at a time
    int villagerCount = countUnitsOfType(83);
    int cap = m_params.villagerCap;

    // Rush: cap villagers at 15 to focus on military
    if (m_strategy == Strategy::Rush) {
        cap = std::min(cap, 15);
    }
    // Boom: increase villager cap by 20 (up to 130) to maximize economy
    else if (m_strategy == Strategy::Boom) {
        cap = std::min(cap + 20, 130);
    }

    if (villagerCount >= cap) return;

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
        if (unit->data()->ID != 83 && unit->data()->ID != 293) continue; // Male/Female Villager
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

        // Create house (ID 70) as foundation and assign villager to build it
        Unit::Ptr house = UnitFactory::createUnit(70, std::const_pointer_cast<Player>(
            std::static_pointer_cast<const Player>(unit->player().lock())), *m_unitManager);
        if (house) {
            house->setCreationProgress(0); // Start as foundation
            m_unitManager->add(house, housePos);
            // Find the Build task for the villager and assign it
            Task buildTask = unit->actions.findTaskWithTarget(house);
            if (buildTask.isValid()) {
                IAction::assignTask(buildTask, unit, IAction::AssignType::Replace);
            } else {
                // Fallback: just move villager to the building
                m_unitManager->moveUnitTo(unit, housePos);
            }
        }
        return;
    }
}

void BasicAI::assignIdleVillagers()
{
    // Find idle villagers and assign them to gather nearest resource
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 83 && unit->data()->ID != 293) continue; // Male/Female Villager
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
        // Boom: prioritize age advancement — advance even while producing villagers
        if (m_strategy == Strategy::Boom) {
            if (building->isResearching()) continue;
        } else {
            if (building->isProducing() || building->isResearching()) continue;
        }

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

bool BasicAI::isMilitaryUnit(int unitId) const
{
    // Barracks: Militia=74, MenAtArms=75, Spearman=93, Pikeman=358, LongSword=77, TwoHanded=473, Champion=567
    // Archery: Archer=4, Crossbow=24, Skirmisher=7, EliteSkirmisher=6, CavArcher=39
    // Stable: Scout=448, LightCav=546, Knight=38, Cavalier=283, Paladin=569, Camel=329
    // Siege: Mangonel=280, Scorpion=279, BatteringRam=35, Onager=550, SiegeRam=422
    static const int ids[] = {
        74, 75, 77, 473, 567, 93, 358,   // barracks
        4, 24, 7, 6, 39,                   // archery
        448, 546, 38, 283, 569, 329,       // stable
        280, 279, 35, 550, 422,            // siege
        125,                                // monk
        21, 442, 539,                       // galley line
        529, 532,                           // fire ship line
        527, 528,                           // demolition ship line
        420, 691                            // cannon galleon line
    };
    for (int id : ids) {
        if (unitId == id) return true;
    }
    return false;
}

void BasicAI::attackWithArmy()
{
    // Attack when we have 8+ idle military units
    int idleMilitary = 0;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (!isMilitaryUnit(unit->data()->ID)) continue;
        if (unit->actions.currentAction()) continue;
        idleMilitary++;
    }
    // Rush: attack with fewer units (half threshold)
    int threshold = m_params.attackThreshold;
    if (m_strategy == Strategy::Rush) {
        threshold = std::max(threshold / 2, 2);
    }
    if (idleMilitary < threshold) return;

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
        if (!isMilitaryUnit(unit->data()->ID)) continue;
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

void BasicAI::trainFromBuilding(int buildingId, int unitId)
{
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != buildingId) continue;
        auto building = Building::fromUnit(unit);
        if (!building || building->isProducing()) continue;

        const genie::Unit &data = m_player->civilization.unitData(unitId);
        building->enqueueProduceUnit(&data);
        return;
    }
}

void BasicAI::trainMilitary()
{
    // Boom: no military until Castle Age — focus on economy
    if (m_strategy == Strategy::Boom && m_player->currentAge() < Player::CastleAge) {
        return;
    }

    float food = m_player->resourcesAvailable(genie::ResourceType::FoodStorage);
    float wood = m_player->resourcesAvailable(genie::ResourceType::WoodStorage);
    float gold = m_player->resourcesAvailable(genie::ResourceType::GoldStorage);

    // Rush: build 2 barracks immediately
    int targetBarracks = (m_strategy == Strategy::Rush) ? 2 : 1;
    if (countBuildingsOfType(12) < targetBarracks) {
        if (wood >= 175) buildStructure(12, 175);
        if (countBuildingsOfType(12) == 0) return; // Need at least one before proceeding
    }

    // Build archery range (87, 175W) after barracks (not for Rush in Dark/Feudal)
    if (countBuildingsOfType(87) == 0 && countBuildingsOfType(12) > 0) {
        if (m_strategy != Strategy::Rush || m_player->currentAge() >= Player::CastleAge) {
            if (wood >= 175) buildStructure(87, 175);
        }
    }

    // Build stable (101, 175W) in Castle Age
    if (countBuildingsOfType(101) == 0 && m_player->currentAge() >= Player::CastleAge) {
        if (wood >= 175) buildStructure(101, 175);
    }

    // Build siege workshop (49, 200W) in Castle Age
    if (m_params.buildSiege && countBuildingsOfType(49) == 0 && m_player->currentAge() >= Player::CastleAge) {
        if (wood >= 200) buildStructure(49, 200);
    }

    // Count all military
    int totalMilitary = 0;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (unit && unit->playerId() == m_player->playerId && isMilitaryUnit(unit->data()->ID)) {
            totalMilitary++;
        }
    }
    if (totalMilitary >= m_params.militaryCap) return;

    // Train from barracks: militia (74, 60F) or spearman (93, 35F 25W)
    // Rush: train militia aggressively from all barracks
    if (food >= 60) {
        if (m_strategy == Strategy::Rush) {
            // Train militia from every barracks
            for (const Unit::Ptr &unit : m_unitManager->units()) {
                if (!unit || unit->playerId() != m_player->playerId) continue;
                if (unit->data()->ID != 12) continue; // Barracks
                auto building = Building::fromUnit(unit);
                if (!building || building->isProducing()) continue;
                const genie::Unit &data = m_player->civilization.unitData(74);
                building->enqueueProduceUnit(&data);
            }
        } else {
            int unitId = (countUnitsOfType(74) > countUnitsOfType(93)) ? 93 : 74;
            trainFromBuilding(12, unitId);
        }
    }

    // Train from archery range: archer (4, 25W 45G) or skirmisher (7, 25F 35W)
    if (wood >= 25 && gold >= 45) {
        trainFromBuilding(87, 4); // Archer
    } else if (food >= 25 && wood >= 35) {
        trainFromBuilding(87, 7); // Skirmisher (no gold)
    }

    // Train from stable: scout (448, 80F) or knight (38, 60F 75G)
    if (countBuildingsOfType(101) > 0) {
        if (food >= 60 && gold >= 75) {
            trainFromBuilding(101, 38); // Knight
        } else if (food >= 80) {
            trainFromBuilding(101, 448); // Scout
        }
    }

    // Train from siege workshop: battering ram (35, 160W 75G)
    if (m_params.buildSiege && countBuildingsOfType(49) > 0 && wood >= 160 && gold >= 75) {
        if (countUnitsOfType(35) < 3) { // Max 3 rams
            trainFromBuilding(49, 35);
        }
    }

    // Build monastery (104, 175W) in Castle Age
    if (countBuildingsOfType(104) == 0 && m_player->currentAge() >= Player::CastleAge) {
        if (wood >= 175) buildStructure(104, 175);
    }

    // Train monks (125, 100G) from monastery — max 3
    if (countBuildingsOfType(104) > 0 && gold >= 100) {
        if (countUnitsOfType(125) < 3) {
            trainFromBuilding(104, 125);
        }
    }
}

void BasicAI::useMonksOffensively()
{
    const float conversionRange = 12.f * Constants::TILE_SIZE;

    // High-value targets for conversion: knights, cavaliers, paladins, war elephants, siege
    static const int highValueIds[] = {
        38, 283, 569, 329,   // Knight, Cavalier, Paladin, Camel
        280, 279, 550, 422,  // Mangonel, Scorpion, Onager, SiegeRam
        35,                  // BatteringRam
    };

    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 125) continue; // Monk
        if (unit->actions.currentAction()) continue; // Already busy

        // Find best enemy target within range — prefer expensive units
        Unit::Ptr bestTarget;
        float bestDist = conversionRange;
        bool bestIsHighValue = false;

        for (const Unit::Ptr &enemy : m_unitManager->units()) {
            if (!enemy || enemy->playerId() == m_player->playerId || enemy->playerId() == 0) continue;
            if (enemy->isDead() || enemy->isDying()) continue;
            if (enemy->isBuilding()) continue; // Can't convert buildings

            float dist = unit->distanceTo(enemy);
            if (dist > conversionRange) continue;

            bool isHighValue = false;
            for (int hvId : highValueIds) {
                if (enemy->data()->ID == hvId) { isHighValue = true; break; }
            }

            // Prefer high-value targets, then closest
            if (isHighValue && !bestIsHighValue) {
                bestTarget = enemy;
                bestDist = dist;
                bestIsHighValue = true;
            } else if (isHighValue == bestIsHighValue && dist < bestDist) {
                bestTarget = enemy;
                bestDist = dist;
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

void BasicAI::buildDefenses()
{
    // Turtle: build Watch Towers and Palisade Walls near TC
    // Balanced: build some defenses in Castle Age
    bool shouldBuild = false;
    if (m_strategy == Strategy::Turtle) {
        shouldBuild = true;
    } else if (m_strategy == Strategy::Balanced && m_player->currentAge() >= Player::CastleAge) {
        shouldBuild = true;
    }
    if (!shouldBuild) return;

    float wood = m_player->resourcesAvailable(genie::ResourceType::WoodStorage);
    float stone = m_player->resourcesAvailable(genie::ResourceType::StoneStorage);

    // Build Watch Towers (ID 79, costs 125 stone + 25 wood) — up to 3
    int towerCount = countBuildingsOfType(79);
    int maxTowers = (m_strategy == Strategy::Turtle) ? 3 : 2;
    if (towerCount < maxTowers && stone >= 125 && wood >= 25) {
        buildStructureWithCost(79, 25, 125);
        return; // One defense structure per update
    }

    // Build Palisade Walls (ID 72, costs 2 wood) — up to 20 segments
    int wallCount = countBuildingsOfType(72);
    int maxWalls = (m_strategy == Strategy::Turtle) ? 20 : 8;
    if (wallCount < maxWalls && wood >= 2) {
        buildStructureWithCost(72, 2, 0);
    }
}

void BasicAI::buildStructureWithCost(int buildingId, int woodCost, int stoneCost)
{
    float wood = m_player->resourcesAvailable(genie::ResourceType::WoodStorage);
    float stone = m_player->resourcesAvailable(genie::ResourceType::StoneStorage);
    if (wood < woodCost || stone < stoneCost) return;

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
                building->setCreationProgress(0);
                m_unitManager->add(building, buildPos);
                m_player->setAvailableResource(genie::ResourceType::WoodStorage, wood - woodCost);
                if (stoneCost > 0) {
                    m_player->setAvailableResource(genie::ResourceType::StoneStorage, stone - stoneCost);
                }
                DBG << "AI built defense" << buildingId << "at" << buildPos.x << buildPos.y;

                // Find idle villager to build it
                for (const Unit::Ptr &vill : m_unitManager->units()) {
                    if (!vill || vill->playerId() != m_player->playerId) continue;
                    if (vill->data()->ID != 83 && vill->data()->ID != 293) continue;
                    if (vill->actions.currentAction()) continue;
                    Task buildTask = vill->actions.findTaskWithTarget(building);
                    if (buildTask.isValid()) {
                        IAction::assignTask(buildTask, vill, IAction::AssignType::Replace);
                    }
                    break;
                }
                return;
            }
        }
    }
}

bool BasicAI::isWaterMap() const
{
    // A map is considered a water map if there are 3+ fish units (ocean, deep sea, or shore)
    int fishCount = 0;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->isDead()) continue;
        int cls = unit->data()->Class;
        if (cls == genie::Unit::OceanFish || cls == genie::Unit::DeepSeaFish ||
            cls == genie::Unit::ShoreFish) {
            fishCount++;
            if (fishCount >= 3) return true;
        }
    }
    return false;
}

void BasicAI::buildNaval()
{
    if (!isWaterMap()) return;

    float wood = m_player->resourcesAvailable(genie::ResourceType::WoodStorage);
    float gold = m_player->resourcesAvailable(genie::ResourceType::GoldStorage);

    // Build Dock (ID 45, 150W) if none exists
    if (countBuildingsOfType(45) == 0) {
        if (wood >= 150) {
            buildStructure(45, 150);
        }
        return; // Wait until dock is built
    }

    // Train fishing ships (ID 13, 75W) up to 5
    if (countUnitsOfType(13) < 5 && wood >= 75) {
        trainFromBuilding(45, 13);
    }

    // In Castle Age, train war galleys (ID 21, 90W 30G) up to 5
    if (m_player->currentAge() >= Player::CastleAge) {
        if (countUnitsOfType(21) < 5 && wood >= 90 && gold >= 30) {
            trainFromBuilding(45, 21);
        }
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
                building->setCreationProgress(0); // Start as foundation
                m_unitManager->add(building, buildPos);
                m_player->setAvailableResource(genie::ResourceType::WoodStorage, wood - woodCost);
                DBG << "AI built building" << buildingId << "at" << buildPos.x << buildPos.y;

                // Find idle villager to build it
                for (const Unit::Ptr &vill : m_unitManager->units()) {
                    if (!vill || vill->playerId() != m_player->playerId) continue;
                    if (vill->data()->ID != 83 && vill->data()->ID != 293) continue;
                    if (vill->actions.currentAction()) continue;
                    Task buildTask = vill->actions.findTaskWithTarget(building);
                    if (buildTask.isValid()) {
                        IAction::assignTask(buildTask, vill, IAction::AssignType::Replace);
                    }
                    break;
                }
                return;
            }
        }
    }
}

void BasicAI::defendAgainstThreats()
{
    m_player->clearStaleThreats(m_lastUpdate);

    if (m_player->m_activeThreats.empty()) return;

    // Find the most severe threat
    const ThreatInfo *worst = nullptr;
    for (const ThreatInfo &threat : m_player->m_activeThreats) {
        if (!worst || threat.severity > worst->severity) {
            worst = &threat;
        }
    }
    if (!worst) return;

    // Send up to 5 nearby idle military units (within 40 tiles) to defend
    const float defendRadius = 40.f * Constants::TILE_SIZE;
    int sent = 0;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (sent >= 5) break;
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (!isMilitaryUnit(unit->data()->ID)) continue;
        if (unit->actions.currentAction()) continue; // Already busy

        float dist = unit->position().distance(worst->location);
        if (dist > defendRadius) continue;

        unit->actions.setCurrentAction(ActionMove::moveUnitTo(unit, worst->location));
        sent++;
    }
}

void BasicAI::retreatInjuredUnits()
{
    // Find TC position for retreat destination
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

    // Retreat military units below 20% HP that are in combat
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (!isMilitaryUnit(unit->data()->ID)) continue;
        if (unit->healthLeft() > 0.2f) continue; // Above 20% HP

        // Check if unit is in combat (current action is Attack)
        const auto &action = unit->actions.currentAction();
        if (!action || action->type != IAction::Type::Attack) continue;

        // Send to TC
        unit->actions.setCurrentAction(ActionMove::moveUnitTo(unit, tcPos));
    }
}

void BasicAI::garrisonVillagersUnderAttack()
{
    if (m_player->m_activeThreats.empty()) return;

    // Find TC (building ID 109)
    Unit::Ptr tc;
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (unit && unit->playerId() == m_player->playerId && unit->data()->ID == 109) {
            tc = unit;
            break;
        }
    }
    if (!tc) return;

    // Check if any threat is within 15 tiles of TC
    const float garrisonRadius = 15.f * Constants::TILE_SIZE;
    bool threatNearTC = false;
    for (const ThreatInfo &threat : m_player->m_activeThreats) {
        if (tc->position().distance(threat.location) < garrisonRadius) {
            threatNearTC = true;
            break;
        }
    }
    if (!threatNearTC) return;

    auto building = Building::fromUnit(tc);
    if (!building) return;

    // Garrison idle/gathering villagers within 15 tiles of TC
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 83 && unit->data()->ID != 293) continue; // Male/Female Villager
        if (unit->garrisonedIn.lock()) continue; // Already garrisoned

        float dist = unit->position().distance(tc->position());
        if (dist > garrisonRadius) continue;

        // Only garrison idle or gathering villagers (not builders)
        const auto &action = unit->actions.currentAction();
        if (action && action->type != IAction::Type::Gather) continue;

        Task task = unit->actions.findTaskWithTarget(tc);
        if (task.isValid()) {
            IAction::assignTask(task, unit, IAction::AssignType::Replace);
        }
    }
}

void BasicAI::ungarrisonWhenSafe()
{
    if (!m_player->m_activeThreats.empty()) return;

    // Ungarrison all TCs owned by this AI player
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (!unit || unit->playerId() != m_player->playerId) continue;
        if (unit->data()->ID != 109) continue; // Town Center
        auto building = Building::fromUnit(unit);
        if (!building) continue;
        if (building->garrisonedUnits.empty()) continue;

        building->ungarrisonAll();
    }
}

void BasicAI::researchTechs()
{
    // Research available techs at idle buildings
    // Priority techs: Blacksmith upgrades, armor, attack upgrades
    static const int priorityTechs[] = {
        67,  // Forging (Blacksmith, +1 melee attack)
        68,  // IronCasting (+1 melee attack)
        75,  // ScaleMailArmor (+1/+1 infantry armor)
        76,  // ChainMailArmor (+1/+1 infantry armor)
        81,  // Fletching (+1 range, +1 attack archery)
        82,  // BodkinArrow (+1 range, +1 attack archery)
        74,  // ScaleBardingArmor (+1/+1 cavalry armor)
        100, // Padded Archer Armor
        140, // Guard Tower
        211, // Wheelbarrow (TC, faster villagers)
        249, // HandCart (TC, even faster)
    };

    for (int techId : priorityTechs) {
        if (m_player->hasResearched(techId)) continue;
        if (!m_player->canAffordResearch(techId)) continue;

        const genie::Tech &tech = m_player->civilization.tech(techId);
        if (tech.ResearchTime <= 0) continue;
        if (tech.ResearchLocation <= 0) continue;

        // Find the building that researches this tech
        for (const Unit::Ptr &unit : m_unitManager->units()) {
            if (!unit || unit->playerId() != m_player->playerId) continue;
            if (unit->data()->ID != tech.ResearchLocation) continue;
            auto building = Building::fromUnit(unit);
            if (!building || building->isProducing() || building->isResearching()) continue;

            building->enqueueProduceResearch(&tech);
            return; // One research at a time
        }
    }
}
