#pragma once

#include <stddef.h>
#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>

#include "core/Constants.h"
#include "core/ResourceMap.h"
#include "core/Types.h"
#include "core/Utility.h"
#include "global/EventManager.h"
#include "global/EventListener.h"
#include "mechanics/Civilization.h"

struct Unit;
class Map;

namespace genie {
class EffectCommand;
}

struct VisibilityMap
{
    bool isDirty = true; // needs re-render

    enum Visibility : int {
        Unexplored = std::numeric_limits<int>::min(),
        Explored = 0,
        Visible
    };

    VisibilityMap(int playerID);

    inline Visibility visibilityAt(const MapPos &pos) const {
        return visibilityAt(pos.x / Constants::TILE_SIZE, pos.y / Constants::TILE_SIZE);
    }

    inline Visibility visibilityAt(const int tileX, const int tileY, const Visibility def = Unexplored) const {
        const size_t index = tileY * Constants::MAP_MAX_SIZE + tileX;
        if (IS_UNLIKELY(index >= m_visibility.size())) {
            return def;
        }

        if (m_visibility[index] > 0) {
            return Visible;
        } else if (m_visibility[index] == Unexplored) {
            return Unexplored;
        } else {
            return Explored;
        }
    }

    void setExplored(const int tileX, const int tileY) {
        const size_t index = tileY * Constants::MAP_MAX_SIZE + tileX;
        if (IS_UNLIKELY(index >= m_visibility.size())) {
            return;
        }
        m_visibility[index] = Explored;
        isDirty = true;

        EventManager::tileDiscovered(m_playerId, tileX, tileY);
    }

    void addUnitLookingAt(const int tileX, const int tileY) {
        const size_t index = tileY * Constants::MAP_MAX_SIZE + tileX;
        if (IS_UNLIKELY(index >= m_visibility.size())) {
            return;
        }

        if (m_visibility[index] == Unexplored) {
            m_visibility[index] = Visible;
            isDirty = true;
        } else {
            m_visibility[index]++;

            if (m_visibility[index] == Visible) {
                isDirty = true;
            }
        }

        if (m_visibility[index] == Visible) {
            EventManager::tileDiscovered(m_playerId, tileX, tileY);
        }
    }

    void removeUnitLookingAt(const int tileX, const int tileY) {
        const size_t index = tileY * Constants::MAP_MAX_SIZE + tileX;
        if (IS_UNLIKELY(index >= m_visibility.size())) {
            return;
        }

        if (IS_UNLIKELY(m_visibility[index] == Unexplored)) {
            return;
        }

        if (m_visibility[index] == Visible) {
            isDirty = true;
        }

        m_visibility[index]--;

        if (m_visibility[index] == Explored) {
            EventManager::tileHidden(m_playerId, tileX, tileY);
        }
    }
    int edgeTileNum(const int tileX, const int tileY, const Visibility type) const;

    void revealAll() {
        m_visibility.fill(Visible);
        isDirty = true;
    }

private:
    int m_playerId;
    std::array<int, Constants::MAP_MAX_SIZE * Constants::MAP_MAX_SIZE> m_visibility;
};

struct Player : public EventListener
{
    enum Age {
        DarkAge,
        FeudalAge,
        CastleAge,
        ImperialAge
    };
    enum DiplomaticStance {
        Neutral = 0,
        Allied,
        Enemy
    };

    Player(const int id, const int civId, const std::shared_ptr<Map> &map, const ResourceMap &startingResources = {});
    virtual ~Player() = default;


    ///////////////////
    /// Basics
    const int playerId;
    int playerColor = 0;

    Civilization civilization;

    typedef std::shared_ptr<Player> Ptr;
    std::string name = "Player";

    bool alive = true;
    std::shared_ptr<VisibilityMap> visibility;

    // Dynamic market prices (AoE2: base price shifts by 3 per transaction)
    struct MarketPrices {
        // Base price for each resource: 0=Food, 1=Wood, 2=Stone
        int basePrice[3] = {100, 100, 100};

        int buyPrice(int res) const { return std::max(20, basePrice[res] * 130 / 100); }
        int sellPrice(int res) const { return std::max(20, basePrice[res] * 70 / 100); }

        void onBuy(int res) { basePrice[res] = std::min(9999, basePrice[res] + 3); }
        void onSell(int res) { basePrice[res] = std::max(20, basePrice[res] - 3); }
    } marketPrices;

    // Score tracking
    int unitsKilled = 0;
    int unitsLost = 0;
    int buildingsRazed = 0;
    float totalResourcesGathered = 0;
    int techsResearched = 0;

    int score() const {
        return unitsKilled * 20 + buildingsRazed * 50 + techsResearched * 30
               + static_cast<int>(totalResourcesGathered / 100);
    }

    void resign();

    ///////////////////
    /// Tech
    bool researchAvailable(const int researchId) { return m_currentlyAvailableTechs.count(researchId); }
    bool hasResearched(const int researchId) const { return m_researchedTechs.count(researchId); }
    void applyResearch(const int researchId);
    void applyTechEffect(const int effectId);
    void applyTechEffectCommand(const genie::EffectCommand &effect);
    void setAge(const Age age);
    inline Age currentAge() {
        return Age(int(m_resourcesAvailable[genie::ResourceType::CurrentAge]));
    }
    bool canAffordResearch(const int researchId) const;

    ///////////////////
    /// Units
    bool canAffordUnit(const int unitId) const;
    void payForUnit(const int unitId);

    void addUnit(Unit *unit);
    void removeUnit(Unit *unit);

    void setUnitGroup(Unit *unit, int group);
    int canSeeUnitsFor(const int otherID);

    ////////////////////
    /// Diplomacy
    void setDiplomaticStance(const uint8_t playerId, const DiplomaticStance stance);
    DiplomaticStance diplomaticStanceTo(uint8_t playerId) const;
    bool isAllied(uint8_t playerId);

    ////////////////////
    /// Resources
    void removeResource(const genie::ResourceType type, float amount) {
        setAvailableResource(type, m_resourcesAvailable[type] - amount);
    }
    virtual void addResource(const genie::ResourceType type, float amount) {
        setAvailableResource(type, m_resourcesAvailable[type] + amount);
    }
    void setAvailableResource(const genie::ResourceType type, float newValue);

    float resourcesAvailable(const genie::ResourceType type) const {
        ResourceMap::const_iterator it = m_resourcesAvailable.find(type);
        if (it == m_resourcesAvailable.end()) {
            return 0.f;
        }

        return it->second;
    }

    float resourcesUsed(const genie::ResourceType type) const {
        ResourceMap::const_iterator it = m_resourcesUsed.find(type);
        if (it == m_resourcesUsed.end()) {
            return 0.f;
        }
        return it->second;
    }
    void sendTribute(const std::shared_ptr<Player> &player, const genie::ResourceType type, const int amount);

    static constexpr int UngroupedGroupID = 0;
    const std::unordered_set<Unit*> &unitsInGroup(int group) {
        if (size_t(group) >= m_unitGroups.size()) {
            WARN << "invalid group" << group;
            static const std::unordered_set<Unit*> nullgroup;
            return nullgroup;

        }

        return m_unitGroups[group];
    }
    int unitGroupCount() const { return m_unitGroups.size(); }

    Unit *findUnitByTypeID(const int type) const;
    std::vector<Unit*> findUnitsByTypeID(const int type) const;

    void onTileHidden(const int playerID, const int tileX, const int tileY) override;
    void onTileDiscovered(const int playerID, const int tileX, const int tileY) override;
    void onUnitMoved(Unit *unit, const MapPos &oldTile, const MapPos &newTile) override;
    void onUnitGarrisoned(Unit *unit, Unit *garrisonedIn) override;
    void onUnitDying(Unit *unit) override;
    void onUnitCreated(Unit *unit) override;

private:
    ResourceMap resourcesNeeded(const genie::Unit &unit) const;

    void updateAvailableTechs();

    // group 0 == ungrouped
    std::vector<std::unordered_set<Unit*>> m_unitGroups;

    ResourceMap m_resourcesUsed;
    ResourceMap m_resourcesAvailable;
    std::unordered_set<Unit*> m_units;
    std::unordered_set<int> m_activeTechs;
    std::unordered_set<int> m_researchedTechs;
    std::vector<DiplomaticStance> m_diplomaticStances;
    std::unordered_map<int, genie::Tech> m_currentlyAvailableTechs;

    std::weak_ptr<Map> m_map;
};
