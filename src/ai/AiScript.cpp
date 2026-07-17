#include "AiScript.h"

#include "AiPlayer.h"
#include "AiRule.h"

namespace ai
{

bool AiScript::update(const Time time)
{
    m_currentTime = time;

    std::vector<Timer>::iterator it = m_activeTimers.begin();

    bool foundExpired = false;
    while (it != m_activeTimers.end()) {
        if (it->time >= time) {
            ++it;
            continue;
        }
        m_expiredTimers.insert(it->id);
        it = m_activeTimers.erase(it);
        foundExpired = true;
    }

    if (foundExpired) {
        emit(TimerTriggered);
    }

    // Evaluate all rules — check conditions and fire actions
    bool anyFired = false;
    for (const std::shared_ptr<AiRule> &rule : rules) {
        if (rule) {
            rule->onConditionSatisfied(); // checks all conditions, fires actions if all satisfied
            anyFired = true;
        }
    }

    return anyFired;
}

void AiScript::setEscrow(const genie::ResourceType resource, float amount)
{
    m_player->m_escrowPercentages[resource] = amount;

}

float AiScript::escrowAmount(const genie::ResourceType resource) const
{
    ResourceMap::const_iterator escrowIt = m_player->m_reserves.find(resource);
    if (escrowIt != m_player->m_reserves.end()) {
        return escrowIt->second;
    }

    return 0.f;
}

void AiScript::showDebugMessage(const std::string &message)
{
    // TODO: for now just print it to the terminal
    // Should probably show it on screen or separate window or something
    WARN << " ========== AI " << m_player->name << message;
}

void AiScript::addTimer(const int id, const Time targetTime)
{
    Timer timer;
    timer.id = id;
    timer.time = targetTime;
    m_activeTimers.push_back(timer);
    m_expiredTimers.erase(id);
}

void AiScript::disableTimer(const int id)
{
    // holy shit I hate STL, why can't this be simple and readable...
    std::erase_if(m_activeTimers, [=](const Timer &timer){
        return timer.id == id;
    });
}

} // namespace ai
