#include "AiPlayer.h"
#include "BasicAI.h"

#include "core/Constants.h"
#include "resource/DataManager.h"

void AiPlayer::setDifficulty(ai::DifficultyLevel level)
{
    difficultyLevel = level;
    m_params = ai::DifficultyParams::forLevel(level);
    if (m_basicAI) m_basicAI->applyParams(m_params);
}

void AiPlayer::addResource(const genie::ResourceType type, float amount)
{
    // Apply difficulty gather bonus for economy resources only
    if (m_params.gatherBonus > 0.f) {
        switch (type) {
        case genie::ResourceType::FoodStorage:
        case genie::ResourceType::WoodStorage:
        case genie::ResourceType::GoldStorage:
        case genie::ResourceType::StoneStorage:
            amount *= (1.f + m_params.gatherBonus);
            break;
        default:
            break;
        }
    }

    float toEscrow = amount * m_escrowPercentages[type] / 100.;
    m_reserves[type] += toEscrow;

    Player::addResource(type, amount - toEscrow);

    EventManager::registerListener(this, EventManager::ChatMessage);
}

bool AiPlayer::canAffordUnitWithEscrow(const int unitId) const
{
    const genie::Unit &unit = civilization.unitData(unitId);
    if (unit.ID == -1 || !unit.Enabled || unit.Creatable.TrainLocationID == -1) {
        return false;
    }

    for (const genie::Unit::ResourceStorage &res : unit.ResourceStorages) {
        if (res.Type == -1) {
            continue;
        }
        switch (res.Type) {
        case genie::ResourceStoreMode::GiveResourceType:
        case genie::ResourceStoreMode::GiveAndTakeResourceType:
        case genie::ResourceStoreMode::BuildingResourceType:
            break;
        default:
            continue;
        }

        const genie::ResourceType resourceType = genie::ResourceType(res.Type);

        int available = resourcesAvailableWithEscrow(genie::ResourceType(resourceType)) - resourcesUsed(genie::ResourceType(res.Type));
        if (available < res.Amount) {
            DBG << unit.Name << "Not affordable" << available << res.Amount;
            return false;
        }
    }

    for (const genie::Resource<short, short> &cost : unit.Creatable.ResourceCosts) {
        if (!cost.Paid) {
            continue;
        }

        const genie::ResourceType type = genie::ResourceType(cost.Type);
        if (resourcesAvailableWithEscrow(type) < cost.Amount) {
            return false;
        }
    }

    return true;
}

bool AiPlayer::canAffordResearchWithEscrow(const int researchId) const
{
    const genie::Tech &research = DataManager::Inst().getTech(researchId);
    for (const genie::Tech::ResearchResourceCost &cost : research.ResourceCosts) {
        const genie::ResourceType resourceType = genie::ResourceType(cost.Type);
        if (resourcesAvailableWithEscrow(resourceType) < cost.Amount) {
            return false;
        }
    }

    return true;

}

void AiPlayer::onChatMessage(const int sourcePlayer, const int targetPlayer, const std::string &message)
{
    WARN << "todo, got chat message, handle that ai trigger" << sourcePlayer << message;

    if (targetPlayer != playerId) {
        return;
    }
}

void AiPlayer::reportThreat(const MapPos &pos, int attackerPlayerId, Time time)
{
    // Merge with existing threat if within 8 tiles distance
    const float mergeDistance = 8.f * Constants::TILE_SIZE;
    for (ThreatInfo &threat : m_activeThreats) {
        if (threat.attackerPlayerId == attackerPlayerId &&
            pos.distance(threat.location) < mergeDistance) {
            threat.location = pos;
            threat.lastSeen = time;
            threat.severity++;
            return;
        }
    }

    // Add new threat
    ThreatInfo info;
    info.location = pos;
    info.lastSeen = time;
    info.attackerPlayerId = attackerPlayerId;
    info.severity = 1;
    m_activeThreats.push_back(info);
}

void AiPlayer::clearStaleThreats(Time time)
{
    // Remove threats older than 30 seconds (30000 ms)
    m_activeThreats.erase(
        std::remove_if(m_activeThreats.begin(), m_activeThreats.end(),
            [time](const ThreatInfo &t) { return (time - t.lastSeen) > 30000; }),
        m_activeThreats.end());
}
