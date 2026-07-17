#include "Building.h"

#include <cmath>
#include <genie/Types.h>
#include <genie/dat/Research.h>
#include <genie/dat/TerrainRestriction.h>
#include <genie/dat/Unit.h>
#include <genie/dat/unit/../ResourceUsage.h>
#include <genie/dat/unit/Creatable.h>
#include <stdint.h>
#include <algorithm>
#include <utility>

#include "Map.h"
#include "MapTile.h"
#include "Player.h"
#include "UnitFactory.h"
#include "actions/ActionMove.h"
#include "actions/ActionGather.h"
#include "actions/ActionAttack.h"
#include "actions/ActionGarrison.h"
#include "audio/AudioPlayer.h"
#include "core/Constants.h"
#include "core/Logger.h"
#include "mechanics/Civilization.h"
#include "mechanics/UnitManager.h"
#include "resource/DataManager.h"
#include "resource/LanguageManager.h"

Building::Building(const genie::Unit &data_, const std::shared_ptr<Player> &player_, UnitManager &unitManager) :
    Unit(data_, player_, unitManager, Entity::Type::Building)
{

}

Building::~Building()
{
    Player::Ptr owner = player().lock();
    if (owner) {
        for (const std::unique_ptr<Product> &toAbort : m_productionQueue) {
            for (const std::pair<const genie::ResourceType, float> &cost : toAbort->cost) {
                owner->addResource(cost.first, cost.second);
            }
        }
        m_productionQueue.clear();
    }
}

bool Building::ungarrison(const std::shared_ptr<Unit> &unit)
{
    std::vector<std::weak_ptr<Unit>>::iterator it = garrisonedUnits.begin();
    for (; it != garrisonedUnits.end(); it++) {
        Unit::Ptr garrisoned = it->lock();
        if (!garrisoned) {
            WARN << "we have dead garrisoned";
            it = garrisonedUnits.erase(it);
            continue;
        }

        if (garrisoned == unit) {
            // TOOD: find a nice position to put the unit
            // Works ish because the unit preserves the position it was at when getting garrisoned
            unit->garrisonedIn.reset();
            it = garrisonedUnits.erase(it);
            return true;
        }
    }

    return false;
}

void Building::ungarrisonAll()
{
    for (auto &weakUnit : garrisonedUnits) {
        Unit::Ptr unit = weakUnit.lock();
        if (unit) {
            unit->garrisonedIn.reset();
        }
    }
    garrisonedUnits.clear();
}

std::shared_ptr<Building> Building::fromUnit(const Unit::Ptr &unit) noexcept
{
    if (!unit) {
        return nullptr;
    }

    if (!unit->isBuilding()) {
        return nullptr;
    }
    return std::static_pointer_cast<Building>(unit);
}

std::shared_ptr<Building> Building::fromUnit(const std::weak_ptr<Unit> &unit) noexcept
{
    return fromUnit(unit.lock());
}

bool Building::enqueueProduceUnit(const genie::Unit *data) noexcept
{
    if (!data) {
        WARN << "trying to enqueue null unit";
        return false;
    }

    Player::Ptr owner = player().lock();
    if (!owner) {
        WARN << "building owner went away";
        return false;
    }

    if (!owner->canAffordUnit(data->ID)) { // also checks housing TODO: make this more obvious
        DBG << "Can't afford" << data->Name;
        return false;
    }

    DBG << debugName << "enqueueing production of unit" << data->Name;

    std::unique_ptr<Product> product = std::make_unique<Product>();
    product->type = Product::Unit;
    product->unit = data;

    for (const genie::Resource<short, short> &cost : data->Creatable.ResourceCosts) {
        if (!cost.Paid) {
            continue;
        }

        const genie::ResourceType type = genie::ResourceType(cost.Type);
        owner->removeResource(type, cost.Amount);
        product->cost[type] = cost.Amount;
    }

    m_productionQueue.push_back(std::move(product));

    if (!m_currentProduct) {
        attemptStartProduction();
    }


    return true;
}

bool Building::enqueueProduceResearch(const genie::Tech *data, int techIndex) noexcept
{
    if (!data) {
        WARN << "trying to enqueue null unit";
        return false;
    }

    Player::Ptr owner = player().lock();
    if (!owner) {
        WARN << "building owner went away";
        return false;
    }

    std::unique_ptr<Product> product = std::make_unique<Product>();
    product->type = Product::Research;
    product->tech = data;
    product->techIndex = techIndex;

    for (const genie::Resource<int16_t, int8_t> &cost : data->ResourceCosts) {
        if (!cost.Paid) {
            continue;
        }

        const genie::ResourceType type = genie::ResourceType(cost.Type);
        if (owner->resourcesAvailable(type) < cost.Amount) {
            return false;
        }
    }

    for (const genie::Resource<int16_t, int8_t> &r : data->ResourceCosts) {
        if (!r.Paid) {
            continue;
        }

        const genie::ResourceType type = genie::ResourceType(r.Type);

        owner->removeResource(type, r.Amount);
        product->cost[type] = r.Amount;
    }

    m_productionQueue.push_back(std::move(product));

    if (!m_currentProduct) {
        attemptStartProduction();
    }

    return true;
}

void Building::abortProduction(size_t index) noexcept
{
    if (m_currentProduct) {
        if (index == 0) {
            m_currentProduct.reset();
            return;

        }
        index--;
    }


    if (index >= m_productionQueue.size()) {
        WARN << "index for abort" << index << "is out of range" << m_productionQueue.size();
        return;
    }

    Player::Ptr owner = player().lock();
    if (!owner) {
        WARN << "building owner went away";
        return;
    }
    const Product &toAbort = *m_productionQueue.at(index);

    for (const std::pair<const genie::ResourceType, float> &cost : toAbort.cost) {
        owner->addResource(cost.first, cost.second);
    }

    m_productionQueue.erase(m_productionQueue.begin() + index);
}

float Building::productionProgress() const noexcept
{
    if (!m_currentProduct) {
        return 0;
    }

    float maximum = 0;
    if (m_currentProduct->type == Product::Unit) {
        maximum = m_currentProduct->unit->Creatable.TrainTime;
    } else {
        maximum = m_currentProduct->tech->ResearchTime;
    }

    return std::min(m_productionProgress / maximum, 1.f);
}

int Building::productIcon(size_t index) noexcept
{
    if (m_currentProduct) {
        if (index == 0) {
            if (m_currentProduct->type == Product::Unit) {
                return m_currentProduct->unit->IconID;
            } else {
                return m_currentProduct->tech->IconID;
            }
        }

        index--;
    }


    if (index >= m_productionQueue.size()) {
        WARN << "index for abort" << index << "is out of range" << m_productionQueue.size();
        return 0;
    }

    if (m_productionQueue[index]->type == Product::Unit) {
        return m_productionQueue[index]->unit->IconID;
    } else {
        return m_productionQueue[index]->tech->IconID;
    }
}

std::string Building::currentProductName() noexcept
{
    if (!m_currentProduct) {
        if (m_productionQueue.empty()) {
            return "";
        }
        if (m_productionQueue.front()->type == Product::Unit) {
            return LanguageManager::Inst()->getString(m_productionQueue.front()->unit->LanguageDLLName);
        } else {
            return LanguageManager::Inst()->getString(m_productionQueue.front()->tech->LanguageDLLName);
        }
    }

    if (m_currentProduct->type == Product::Unit) {
        return LanguageManager::Inst()->getString(m_currentProduct->unit->LanguageDLLName);
    } else {
        return LanguageManager::Inst()->getString(m_currentProduct->tech->LanguageDLLName);
    }
}



bool Building::update(Time time) noexcept
{
    const Time deltaTime = time - m_lastUpdateTime;
    m_lastUpdateTime = time;

    bool updated = Unit::update(time);

    // Heal garrisoned units (TC/Tower: 0.1 HP/sec, Castle: 0.2 HP/sec)
    if (!garrisonedUnits.empty()) {
        float healRate = (data()->ID == 82 /*Castle*/) ? 0.2f : 0.1f;
        float healAmount = healRate * deltaTime / 1000.f;
        for (auto it = garrisonedUnits.begin(); it != garrisonedUnits.end(); ) {
            Unit::Ptr garrisoned = it->lock();
            if (!garrisoned) {
                it = garrisonedUnits.erase(it);
                continue;
            }
            if (garrisoned->healthLeft() < garrisoned->data()->HitPoints) {
                garrisoned->takeDamage(-healAmount);
            }
            ++it;
        }
    }

    // Monastery (ID 104): generate 0.5 gold/sec per garrisoned relic (30 gold/min)
    if (data()->ID == 104 && !garrisonedUnits.empty()) {
        int relicCount = 0;
        for (const auto &g : garrisonedUnits) {
            Unit::Ptr u = g.lock();
            if (u && u->data()->ID == 285) relicCount++; // Relic ID
        }
        if (relicCount > 0) {
            Player::Ptr owner = player().lock();
            if (owner) {
                float goldPerMs = 0.5f * relicCount; // 0.5 gold/sec per relic
                float gold = goldPerMs * deltaTime / 1000.f;
                owner->setAvailableResource(genie::ResourceType::GoldStorage,
                    owner->resourcesAvailable(genie::ResourceType::GoldStorage) + gold);
                owner->setAvailableResource(genie::ResourceType::RelicsCaptured, relicCount);
            }
        }
    }

    if (m_currentProduct) {
        float productionTime = 0;
        if (m_currentProduct->type == Product::Unit) {
            productionTime = m_currentProduct->unit->Creatable.TrainTime;
        } else {
            productionTime = m_currentProduct->tech->ResearchTime;
        }

        m_productionProgress += deltaTime * 0.0015;
        if (m_productionProgress >= productionTime) {
            if (m_currentProduct->type == Product::Unit) {
                finalizeUnit();
            } else {
                finalizeResearch();
            }

            m_currentProduct.reset();
            m_productionProgress = 0;
        }

        updated = true;
    } else if (!m_productionQueue.empty()) {
        attemptStartProduction();
        updated = true;
    }

    return updated;
}

void Building::setPosition(const MapPos &pos, const bool initial)
{
    std::shared_ptr<Map> map = m_map.lock();
    REQUIRE(map, return);

    Unit::setPosition(map->snapPositionToGrid(pos, clearanceSize()), initial);
}

bool Building::canPlace(const MapPos &position, const MapPtr &map, const genie::Unit *data)
{
    REQUIRE(map, return false);
    REQUIRE(data, return false);

    std::vector<float> passable = DataManager::Inst().getTerrainRestriction(data->TerrainRestriction).PassableBuildableDmgMultiplier;

    const int tileX = position.x / Constants::TILE_SIZE;
    const int tileY = position.y / Constants::TILE_SIZE;

    const int valid1 = data->PlacementTerrain.first;
    const int valid2 = data->PlacementTerrain.first;

    const int width = std::max(static_cast<int>(std::ceil(data->ClearanceSize.x + data->Size.x)), 2);
    const int height = std::max(static_cast<int>(std::ceil(data->ClearanceSize.y + data->Size.y)), 2);


    if (!map->isValidTile(tileX - width/2, tileY - width/2)) {
        return false;
    }

    if (!map->isValidTile(tileX + width/2, tileY + height/2)) {
        return false;
    }

    for (int dx = 0; dx < width; dx++) {
        for (int dy = 0; dy < height; dy++) {
            const int tx = tileX + dx - width/2;
            const int ty = tileY + dy - height/2;
            const int terrainId = map->getTileAt(tx, ty).terrainId;
            if (valid1 != -1 && terrainId != valid1) {
                return false;
            }
            if (valid2 != -1 && terrainId != valid2) {
                return false;
            }
            if (!passable[terrainId]) {
                return false;
            }

            // Check for existing buildings/units occupying this tile
            const auto &entities = map->entitiesAt(tx, ty);
            for (const auto &e : entities) {
                if (e.lock()) {
                    return false;
                }
            }
        }
    }

    return true;
}

void Building::finalizeUnit() noexcept
{
    Player::Ptr owner = player().lock();
    if (!owner) {
        WARN << "owner went away";
        return;
    }

    // Always spawn adjacent to the building
    const MapPos spawnPos(position().x + 24, position().y + 24);

    Unit::Ptr unit = UnitFactory::Inst().createUnit(m_currentProduct->unit->ID, owner, m_unitManager);
    if (!unit) {
        WARN << "Failed to finalize unit";
        return;
    }
    m_unitManager.add(unit, spawnPos);

    Player::Ptr unitPlayer = unit->player().lock();
    if (unitPlayer && unitPlayer->playerId == m_unitManager.humanPlayerID()) {
        AudioPlayer::instance().playSound(unit->data()->TrainSound, unitPlayer->civilization.id());
    }

    DBG << "Finalized" << unit->debugName;

    if (!hasRallyPoint) {
        return;
    }

    Unit::Ptr target = rallyTarget.lock();

    if (target && target->isAlive()) {
        // Rally on enemy — attack
        if (!owner->isAllied(target->playerId())) {
            Task task = unit->actions.findAnyTask(genie::ActionType::Combat, target->data()->ID);
            if (task.data) {
                task.target = target;
                IAction::assignTask(task, unit, IAction::AssignType::Replace);
                return;
            }
        }

        // Rally on garrisonable building — garrison
        if (target->data()->GarrisonCapacity > 0) {
            Task task = unit->actions.findAnyTask(genie::ActionType::Garrison, target->data()->ID);
            if (task.data) {
                task.target = target;
                IAction::assignTask(task, unit, IAction::AssignType::Replace);
                return;
            }
        }

        // Rally on resource — gather
        Task task = unit->actions.findTaskWithTarget(target);
        if (task.data && (task.data->ActionType == genie::ActionType::GatherRebuild ||
                          task.data->ActionType == genie::ActionType::Hunt)) {
            task.target = target;
            IAction::assignTask(task, unit, IAction::AssignType::Replace);
            return;
        }
    }

    // Bare ground or no usable target — just move there
    unit->actions.setCurrentAction(ActionMove::moveUnitTo(unit, waypoint));
}

void Building::finalizeResearch() noexcept
{
    Player::Ptr owner = player().lock();
    if (!owner) {
        WARN << "building owner went away";
        return;
    }
    if (m_currentProduct->techIndex >= 0) {
        owner->applyResearch(m_currentProduct->techIndex);
    } else {
        // Fallback: apply effect directly
        owner->applyTechEffect(m_currentProduct->tech->EffectID);
    }
}

void Building::attemptStartProduction() noexcept
{
    if (m_productionQueue.empty()) {
        DBG << "empty queue";
        return;
    }

    Player::Ptr owner = player().lock();
    if (!owner) {
        WARN << "building owner went away";
        return;
    }

    const Product &product = *m_productionQueue.front();

    if (product.type == Product::Unit) {
        for (const genie::Resource<short, short> &cost : product.unit->Creatable.ResourceCosts) {
            if (cost.Paid) {
                continue;
            }

            const genie::ResourceType type = genie::ResourceType(cost.Type);
            if (owner->resourcesAvailable(type) < cost.Amount) {
                return;
            }
        }
    } else {
        for (const genie::Resource<int16_t, int8_t> &cost : product.tech->ResourceCosts) {
            if (cost.Paid) {
                continue;
            }

            const genie::ResourceType type = genie::ResourceType(cost.Type);
            if (owner->resourcesAvailable(type) < cost.Amount) {
                return;
            }
        }
    }

    m_productionProgress = 0.f;
    m_currentProduct = std::move(m_productionQueue.front());
    m_productionQueue.erase(m_productionQueue.begin());
}
