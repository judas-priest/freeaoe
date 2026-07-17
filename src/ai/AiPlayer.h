#pragma once

#include "mechanics/Player.h"
#include "core/Types.h"
#include "gen/enums.h"
#include "DifficultyParams.h"

#include <memory>
#include <vector>

namespace ai { struct AiScript; }
class BasicAI;

struct ThreatInfo {
    MapPos location;
    Time lastSeen = 0;
    int attackerPlayerId = 0;
    int severity = 0;
};

struct AiPlayer : public Player
{
    AiPlayer(const int id, const int civId, const std::shared_ptr<Map> &map, const ResourceMap &startingResources = {}) :
        Player(id, civId, map, startingResources),
        m_params(ai::DifficultyParams::forLevel(difficultyLevel))
    {}

    ai::DifficultyLevel difficultyLevel = ai::DifficultyLevel::Moderate;
    ai::DifficultyParams m_params;

    void setDifficulty(ai::DifficultyLevel level);

    std::shared_ptr<ai::AiScript> m_aiScript;
    std::shared_ptr<BasicAI> m_basicAI;

    // Held in escrow
    ResourceMap m_reserves;

    // How much to hold in escrow
    ResourceMap m_escrowPercentages;

    void addResource(const genie::ResourceType type, float amount) override;

    float resourcesAvailableWithEscrow(const genie::ResourceType type) const {
        float ret = Player::resourcesAvailable(type);

        ResourceMap::const_iterator it = m_reserves.find(type);
        if (it != m_reserves.end()) {
            ret += it->second;
        }

        return ret;
    }

    // meh, duplicating code ish
    bool canAffordUnitWithEscrow(const int unitId) const;
    bool canAffordResearchWithEscrow(const int researchId) const;

    void onChatMessage(const int sourcePlayer, const int targetPlayer, const std::string &message) override;

    // Threat tracking for AI defensive reactions
    std::vector<ThreatInfo> m_activeThreats;
    void reportThreat(const MapPos &pos, int attackerPlayerId, Time time);
    void clearStaleThreats(Time time);
};

