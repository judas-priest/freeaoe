#include "NotificationManager.h"

#include "audio/AudioPlayer.h"
#include "core/Logger.h"
#include "mechanics/Player.h"
#include "Engine.h"

#include <genie/dat/ResourceUsage.h>

// AoE2 DRS sound IDs for voice notifications.
// These are the standard sound IDs used in the original game data files.
// Sound 647 = "Your town is under attack" taunt/notification
// Sound 5209 = research complete jingle
// Sound 5208 = age advance fanfare
// If these don't resolve, AudioPlayer::playSound will silently fail.
static constexpr int SOUND_TOWN_UNDER_ATTACK = 647;  // TODO: verify from DRS data
static constexpr int SOUND_RESEARCH_COMPLETE = 5209;  // TODO: verify from DRS data
static constexpr int SOUND_AGE_ADVANCE       = 5208;  // TODO: verify from DRS data

static NotificationManager *s_instance = nullptr;

NotificationManager &NotificationManager::instance()
{
    return *s_instance;
}

NotificationManager::NotificationManager()
{
    EventManager::registerListener(this, EventManager::ResearchComplete);
    EventManager::registerListener(this, EventManager::PlayerResourceChanged);
    s_instance = this;
}

NotificationManager::~NotificationManager()
{
    EventManager::deregisterListener(this);
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void NotificationManager::notify(Type type, int civId)
{
    if (!canNotify(type)) {
        return;
    }

    markNotified(type);

    int soundId = -1;
    switch (type) {
    case Type::TownUnderAttack:
        soundId = SOUND_TOWN_UNDER_ATTACK;
        break;
    case Type::ResearchComplete:
        soundId = SOUND_RESEARCH_COMPLETE;
        break;
    case Type::AgeAdvance:
        soundId = SOUND_AGE_ADVANCE;
        break;
    }

    if (soundId >= 0) {
        AudioPlayer::instance().playSound(soundId, civId);
    }
}

void NotificationManager::onBuildingAttacked(int ownerPlayerId, int civId)
{
    if (ownerPlayerId != m_humanPlayerId) {
        return;
    }
    notify(Type::TownUnderAttack, civId);
}

void NotificationManager::onResearchCompleted(Player *player, int researchId)
{
    (void)researchId;
    if (!player || player->playerId != m_humanPlayerId) {
        return;
    }
    notify(Type::ResearchComplete, player->civilization.id());
}

void NotificationManager::onPlayerResourceChanged(Player *player, const genie::ResourceType type, float newValue)
{
    if (!player || player->playerId != m_humanPlayerId) {
        return;
    }

    if (type != genie::ResourceType::CurrentAge) {
        return;
    }

    int newAge = static_cast<int>(newValue);
    if (m_lastKnownAge < 0) {
        // First time seeing the age -- just record it, don't play sound
        m_lastKnownAge = newAge;
        return;
    }

    if (newAge > m_lastKnownAge) {
        m_lastKnownAge = newAge;
        notify(Type::AgeAdvance, player->civilization.id());
    }
}

int64_t NotificationManager::cooldownFor(Type type) const
{
    switch (type) {
    case Type::TownUnderAttack: return ATTACK_COOLDOWN_MS;
    case Type::ResearchComplete: return RESEARCH_COOLDOWN_MS;
    case Type::AgeAdvance: return AGE_COOLDOWN_MS;
    }
    return 0;
}

bool NotificationManager::canNotify(Type type) const
{
    auto it = m_lastNotifyTime.find(static_cast<int>(type));
    if (it == m_lastNotifyTime.end()) {
        return true;
    }
    int64_t now = Engine::currentTimeMs();
    return (now - it->second) >= cooldownFor(type);
}

void NotificationManager::markNotified(Type type)
{
    m_lastNotifyTime[static_cast<int>(type)] = Engine::currentTimeMs();
}
