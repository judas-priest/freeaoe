#pragma once

#include "global/EventListener.h"
#include "global/EventManager.h"
#include "core/Types.h"

#include <unordered_map>

struct Player;

namespace genie {
enum class ResourceType : int16_t;
}

/// Plays voice notifications for key game events with cooldowns to prevent spam.
class NotificationManager : public EventListener
{
public:
    enum class Type {
        TownUnderAttack,
        ResearchComplete,
        AgeAdvance
    };

    NotificationManager();
    ~NotificationManager() override;

    /// Access the singleton instance (created by Engine)
    static NotificationManager &instance();

    /// Set which player ID is the human player (only play notifications for them)
    void setHumanPlayerId(int id) { m_humanPlayerId = id; }

    /// Notify of a game event. Checks cooldown and plays the appropriate sound.
    void notify(Type type, int civId);

    /// Called when a building belonging to the human player takes damage
    void onBuildingAttacked(int ownerPlayerId, int civId);

protected:
    // EventListener overrides
    void onResearchCompleted(Player *player, int researchId) override;
    void onPlayerResourceChanged(Player *player, const genie::ResourceType type, float newValue) override;

private:
    int m_humanPlayerId = -1;

    // Cooldown tracking: type -> last time notification was played (milliseconds)
    std::unordered_map<int, int64_t> m_lastNotifyTime;

    // Cooldown durations in milliseconds
    static constexpr int64_t ATTACK_COOLDOWN_MS  = 20000; // 20 seconds
    static constexpr int64_t RESEARCH_COOLDOWN_MS = 5000;  // 5 seconds
    static constexpr int64_t AGE_COOLDOWN_MS      = 30000; // 30 seconds

    int64_t cooldownFor(Type type) const;
    bool canNotify(Type type) const;
    void markNotified(Type type);

    int m_lastKnownAge = -1; // track age to detect changes
};
