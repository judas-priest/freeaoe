#include "ScenarioController.h"

#include "resource/LanguageManager.h"
#include <genie/dat/Unit.h>
#include <genie/dat/ResourceUsage.h>
#include <genie/script/ScnFile.h>
#include <genie/script/scn/ScnPlayerData.h>
#include <genie/script/scn/Trigger.h>
#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "audio/AudioPlayer.h"
#include "mechanics/Unit.h"
#include "mechanics/UnitManager.h"
#include "mechanics/GameState.h"
#include "mechanics/UnitFactory.h"
#include "mechanics/Player.h"

#include "resource/LanguageManager.h"

#include "Engine.h"
#include "Entity.h"
#include "GameState.h"
#include "Player.h"
#include "Unit.h"
#include "core/Constants.h"
#include "core/Logger.h"
#include "core/ResourceMap.h"
#include "core/Types.h"
#include "global/EventManager.h"
#include "mechanics/Map.h"
#include "mechanics/ScenarioController.h"
#include "mechanics/UnitFactory.h"
#include "mechanics/UnitManager.h"

ScenarioController::ScenarioController(GameState *gameState) :
    m_gameState(gameState)
{
}

void ScenarioController::setScenario(const std::shared_ptr<genie::ScnFile> &scenario)
{
    m_triggers.clear();

    if (!scenario) {
        EventManager::deregisterListener(this);
        WARN << "set null scenario";
        return;
    }
    std::unordered_set<int32_t> missingConditionTypes;
    for (const genie::Trigger &trigger : scenario->triggers) {
        if (trigger.conditions.empty()) {
            m_triggers.emplace_back(trigger);
            continue;
        }

        bool isImplemented = false;
        for (const genie::TriggerCondition &cond : trigger.conditions) {
            switch(cond.type) { // Add here as they are implemented
            case genie::TriggerCondition::OwnObjects:
            case genie::TriggerCondition::OwnFewerObjects:
            case genie::TriggerCondition::ObjectSelected:
            case genie::TriggerCondition::ObjectsInArea:
            case genie::TriggerCondition::BringObjectToArea:
            case genie::TriggerCondition::Timer:
            case genie::TriggerCondition::DestroyObject:
            case genie::TriggerCondition::AccumulateAttribute:
            case genie::TriggerCondition::PlayerDefeated:
            case genie::TriggerCondition::DifficultyLevel:
                isImplemented = true;
                break;
            default:
//                WARN << "Not implemented condition" << cond;
                missingConditionTypes.insert(cond.type);
                continue;
            }
        }

        if (isImplemented) {
            m_triggers.emplace_back(trigger);
        } else {
            DBG << trigger;
        }
    }

    for (const int32_t type : missingConditionTypes) {
        DBG << "Missing support for condition type" << genie::TriggerCondition::Type(type);
    }

    std::unordered_set<int32_t> missingEffectTypes;
    for (const Trigger &trigger : m_triggers) {
        for (const genie::TriggerEffect &effect : trigger.effects) {
            switch(effect.type) { // Add here as they are implemented
            case genie::TriggerEffect::DeactivateTrigger:
            case genie::TriggerEffect::ActivateTrigger:
            case genie::TriggerEffect::DisplayInstructions:
            case genie::TriggerEffect::SendChat:
            case genie::TriggerEffect::Sound:
            case genie::TriggerEffect::TaskObject:
            case genie::TriggerEffect::ChangeView:
            case genie::TriggerEffect::ResearchTechnology:
            case genie::TriggerEffect::CreateObject:
            case genie::TriggerEffect::RemoveObject:
            case genie::TriggerEffect::DamageObject:
            case genie::TriggerEffect::ChangeObjectHP:
            case genie::TriggerEffect::SetUnitStance:
            case genie::TriggerEffect::ChangeObjectName:
            case genie::TriggerEffect::ChangeDiplomacy:
            case genie::TriggerEffect::SendTribute:
            case genie::TriggerEffect::DeclareVictory:
            case genie::TriggerEffect::HD_HealObject:
                break;
            default:
                missingEffectTypes.insert(effect.type);
                break;
            }

        }
    }
    for (const int32_t type : missingEffectTypes) {
        WARN << "not implemented trigger effect" << genie::TriggerEffect::Type(type);
    }

//    genie::Trigger mainVictoryTrigger;
//    DBG << "global victory type" << scenario->victoryType;
    const genie::ScnVictory &victoryConditions = scenario->playerData.victoryConditions;

    std::vector<genie::TriggerCondition> mainVictoryConditions;
    std::vector<genie::TriggerCondition> conquestConditions;
    if (victoryConditions.conquestRequired || victoryConditions.victoryMode == genie::ScnVictory::Conquest) {
        for (size_t playerId = 0; playerId < scenario->players.size(); playerId++) {
            if (!scenario->playerData.resourcesPlusPlayerInfo[playerId].enabled) {
                continue;
            }
            if (scenario->playerData.resourcesPlusPlayerInfo[playerId].isHuman) {
                continue;
            }
            genie::TriggerCondition condition;
            condition.type = genie::TriggerCondition::PlayerDefeated;
            condition.sourcePlayer = playerId;
            conquestConditions.push_back(std::move(condition));
        }
    }
    if (victoryConditions.conquestRequired) {
        DBG << "conquest required to win";
    }

    switch(victoryConditions.victoryMode) {
    case genie::ScnVictory::Standard:
        DBG << "standard game — treating as conquest";
        mainVictoryConditions = conquestConditions;
        break;
    case genie::ScnVictory::Conquest:
        DBG << "conquest game (should be handled)";
        mainVictoryConditions = conquestConditions;
        break;
    case genie::ScnVictory::Score:
        DBG << "score game — treating as conquest (score tracking not yet implemented)";
        mainVictoryConditions = conquestConditions;
        break;
    case genie::ScnVictory::Timed:
        DBG << "timed game:" << victoryConditions.timeForTimedGame << "seconds";
        if (victoryConditions.timeForTimedGame > 0) {
            // Create timer trigger — when time expires, check score (highest wins)
            genie::TriggerCondition timerCond;
            timerCond.type = genie::TriggerCondition::Timer;
            timerCond.timer = victoryConditions.timeForTimedGame;
            mainVictoryConditions.push_back(std::move(timerCond));
        }
        break;
    case genie::ScnVictory::Custom:
        DBG << "custom game — treating as conquest";
        mainVictoryConditions = conquestConditions;
        break;
    default:
        WARN << "Invalid victory mode" << victoryConditions.victoryMode;
    }

    // I think relics and explored might be for custom games?
    if (victoryConditions.numRelicsRequired > 0) {
        DBG << "requires" << victoryConditions.numRelicsRequired << "relics to win";
        genie::TriggerCondition condition;
        condition.type = genie::TriggerCondition::AccumulateAttribute;
        condition.amount = victoryConditions.numRelicsRequired;
        condition.resource = int32_t(genie::ResourceType::RelicsCaptured);

        mainVictoryConditions.push_back(std::move(condition));
    }

    if (victoryConditions.exploredPerCentRequired > 0) {
        DBG << "requires" << victoryConditions.exploredPerCentRequired << "of map explored to win";
        genie::TriggerCondition condition;
        condition.type = genie::TriggerCondition::AccumulateAttribute;
        condition.amount = victoryConditions.exploredPerCentRequired;
        condition.resource = int32_t(genie::ResourceType::PercentMapExplored);
        mainVictoryConditions.push_back(std::move(condition));
    }


    genie::TriggerEffect humanWinsEffect;
    humanWinsEffect.type = genie::TriggerEffect::DeclareVictory;
    humanWinsEffect.sourcePlayer = m_gameState->humanPlayer()->playerId;
    if (scenario->playerData.victoryConditions.allConditionsRequired) {
        DBG << "all conditions required to win";
        genie::Trigger mainVictoryTrigger;
        mainVictoryTrigger.startingState = 1;
        mainVictoryTrigger.conditions = mainVictoryConditions;

        if (victoryConditions.conquestRequired && victoryConditions.victoryMode != genie::ScnVictory::Conquest) {
            for (const genie::TriggerCondition &conquestCondition : conquestConditions) {
                mainVictoryTrigger.conditions.push_back(conquestCondition);
            }
        }

        mainVictoryTrigger.effects = { humanWinsEffect };
        m_triggers.emplace_back(mainVictoryTrigger);
    } else {
        DBG << "any condition required to win";
        for (const genie::TriggerCondition &victoryCondition : mainVictoryConditions) {
            genie::Trigger mainVictoryTrigger;
            mainVictoryTrigger.startingState = 1;
            mainVictoryTrigger.conditions = { victoryCondition };

            if (victoryConditions.conquestRequired && victoryConditions.victoryMode != genie::ScnVictory::Conquest) {
                for (const genie::TriggerCondition &conquestCondition : conquestConditions) {
                    mainVictoryTrigger.conditions.push_back(conquestCondition);
                }
            }

            mainVictoryTrigger.effects = { humanWinsEffect };
            m_triggers.emplace_back(mainVictoryTrigger);
        }
        if (victoryConditions.conquestRequired || victoryConditions.victoryMode == genie::ScnVictory::Conquest) {
            genie::Trigger conquestTrigger;
            conquestTrigger.startingState = 1;
            conquestTrigger.conditions = conquestConditions;

            for (const genie::TriggerCondition &conquestCondition : conquestConditions) {
                conquestTrigger.conditions.push_back(conquestCondition);
            }

            conquestTrigger.effects = { humanWinsEffect };
            m_triggers.emplace_back(conquestTrigger);
        }
    }

    for (size_t playerId = 0; playerId < scenario->players.size(); playerId++) {
        const genie::ScnMorePlayerData &player  = scenario->players[playerId];
        genie::Trigger winTrigger;
        winTrigger.startingState = 1;
        winTrigger.name = player.playerName + " wins";

        for (const genie::ScnPlayerVictoryCondition &victoryCondition : player.victoryConditions) {
            switch(victoryCondition.type) {
            case genie::ScnPlayerVictoryCondition::Attribute: {
                DBG << "player" << player.playerName << "needs" << victoryCondition.count << "of" << genie::ResourceType(victoryCondition.number) << "to win";

                genie::TriggerCondition condition;
                condition.type = genie::TriggerCondition::AccumulateAttribute;
                condition.amount = victoryCondition.count;
                winTrigger.conditions = {condition};
                winTrigger.conditions.push_back(std::move(condition));

                break;
            }
            default:
                WARN << "unhandled victory condition type:" << victoryCondition.type;
                continue;
            }
        }
        if (winTrigger.conditions.empty()) {
            DBG << "no winning conditions for" << player.playerName;
            continue;
        }

        genie::TriggerEffect effect;
        effect.type = genie::TriggerEffect::DeclareVictory;
        if  (player.playerID >= 0) {
            effect.sourcePlayer = player.playerID;
        } else {
            effect.sourcePlayer = playerId;
        }

        winTrigger.effects = {effect};
        m_triggers.emplace_back(winTrigger);
    }

    EventManager::registerListener(this, EventManager::UnitCreated);
    EventManager::registerListener(this, EventManager::UnitMoved);
    EventManager::registerListener(this, EventManager::UnitSelected);
    EventManager::registerListener(this, EventManager::UnitDeselected);
    EventManager::registerListener(this, EventManager::UnitDestroyed);
    EventManager::registerListener(this, EventManager::PlayerDefeated);
    EventManager::registerListener(this, EventManager::AttributeChanged);
}

bool ScenarioController::update(Time time)
{
    bool updated = false;
    const Time elapsed = time - m_lastUpdateTime;
    m_lastUpdateTime = time;
    for (Trigger &trigger : m_triggers) {
        if (!trigger.enabled) {
            continue;
        }

        bool conditionsSatisfied = true;
        for (Condition &condition : trigger.conditions) {
            if (condition.data.type == genie::TriggerCondition::Timer) {
                condition.amountRequired -= elapsed;
            } else if (condition.data.type == genie::TriggerCondition::DifficultyLevel) {
                // Always satisfied (we don't track difficulty, treat as "standard")
                condition.amountRequired = 0;
            } else if (condition.data.type == genie::TriggerCondition::PlayerDefeated) {
                // Check if the specified player has been defeated (result != Running)
                // For now, mark as not satisfied (player alive)
                // Will be set to 0 by onPlayerDefeated when it fires
            }

            if (condition.amountRequired > 0) {
                conditionsSatisfied = false;
            }
        }

        if (!conditionsSatisfied) {
            continue;
        }

        updated = true;

        if (!trigger.looping) {
            trigger.enabled = false;
        } else {
            // Reset timer conditions so looping triggers don't fire every frame
            for (Condition &condition : trigger.conditions) {
                condition.amountRequired = condition.originalAmount;
            }
        }

        for (const genie::TriggerEffect &effect : trigger.effects) {
            handleTriggerEffect(effect);
        }
    }

    // Check wonder and relic victory
    checkWonderVictory(time);
    checkRelicVictory(time);
    checkRegicide(time);

    return updated;
}

void ScenarioController::checkWonderVictory(Time time)
{
    if (!m_gameState) return;

    // Check if any player has a wonder (ID 276)
    for (const auto &player : m_gameState->players()) {
        if (!player || player->playerId == 0 || !player->alive) continue;

        bool hasWonder = false;
        for (const Unit::Ptr &unit : m_gameState->unitManager()->units()) {
            if (unit && unit->playerId() == player->playerId && unit->data()->ID == 276) {
                hasWonder = true;
                break;
            }
        }

        if (hasWonder) {
            // Check if timer already exists
            bool timerExists = false;
            for (auto &wt : m_wonderTimers) {
                if (wt.playerId == player->playerId) {
                    timerExists = true;
                    // Check countdown
                    if (time - wt.buildTime >= WonderTimer::WONDER_COUNTDOWN) {
                        m_gameState->onPlayerWin(player->playerId);
                    }
                    break;
                }
            }
            if (!timerExists) {
                WonderTimer wt;
                wt.playerId = player->playerId;
                wt.buildTime = time;
                m_wonderTimers.push_back(wt);
                if (m_engine) {
                    m_engine->addMessage("Player " + std::to_string(player->playerId) + " has built a Wonder!");
                }
            }
        } else {
            // Wonder destroyed — remove timer
            m_wonderTimers.erase(
                std::remove_if(m_wonderTimers.begin(), m_wonderTimers.end(),
                    [&](const WonderTimer &wt) { return wt.playerId == player->playerId; }),
                m_wonderTimers.end());
        }
    }
}

void ScenarioController::checkRelicVictory(Time time)
{
    if (!m_gameState) return;

    // Count total relics on map (once)
    if (m_totalRelicsOnMap < 0) {
        m_totalRelicsOnMap = 0;
        for (const Unit::Ptr &unit : m_gameState->unitManager()->units()) {
            if (unit && unit->data()->ID == 285) m_totalRelicsOnMap++; // Relic ID = 285
        }
        if (m_totalRelicsOnMap == 0) m_totalRelicsOnMap = 5; // default assumption
    }

    // Check which player holds the most relics
    int maxRelics = 0;
    int maxPlayer = -1;
    for (const auto &player : m_gameState->players()) {
        if (!player || player->playerId == 0) continue;
        int relics = static_cast<int>(player->resourcesAvailable(genie::ResourceType::RelicsCaptured));
        if (relics > maxRelics) {
            maxRelics = relics;
            maxPlayer = player->playerId;
        }
    }

    if (maxRelics >= m_totalRelicsOnMap && maxPlayer >= 0) {
        if (m_allRelicsHolder != maxPlayer) {
            m_allRelicsHolder = maxPlayer;
            m_allRelicsHeldSince = time;
            if (m_engine) {
                m_engine->addMessage("Player " + std::to_string(maxPlayer) + " holds all relics!");
            }
        }
        if (time - m_allRelicsHeldSince >= RELIC_VICTORY_TIME) {
            m_gameState->onPlayerWin(maxPlayer);
        }
    } else {
        m_allRelicsHolder = -1;
    }
}

void ScenarioController::checkRegicide(Time /*time*/)
{
    if (!m_gameState || m_gameState->gameType() != GameType::Regicide) return;

    // Check if any player's King (ID 434) has died
    for (const auto &player : m_gameState->players()) {
        if (!player || player->playerId == 0 || !player->alive) continue;

        bool hasKing = false;
        for (const Unit::Ptr &unit : m_gameState->unitManager()->units()) {
            if (unit->playerId() == player->playerId && unit->data()->ID == 434 && !unit->isDead()) {
                hasKing = true;
                break;
            }
        }

        if (!hasKing) {
            player->alive = false;
            if (m_engine) {
                m_engine->addMessage("Player " + std::to_string(player->playerId) + " has been regicided!");
            }
            // Check if only one player left alive
            int aliveCount = 0;
            int lastAlive = -1;
            for (const auto &p : m_gameState->players()) {
                if (p && p->playerId != 0 && p->alive) {
                    aliveCount++;
                    lastAlive = p->playerId;
                }
            }
            if (aliveCount == 1) {
                m_gameState->onPlayerWin(lastAlive);
            }
        }
    }
}

void ScenarioController::handleTriggerEffect(const genie::TriggerEffect &effect)
{
    const genie::TriggerEffect::Type effectType = genie::TriggerEffect::Type(effect.type);

    switch(effectType) {
    case genie::TriggerEffect::None:
        DBG << "Trigger missing type" << effect;
        break;

         ////////////////////////
         // Trigger modifications
    case genie::TriggerEffect::ActivateTrigger: {
        uint32_t triggerIndex = effect.trigger;
        if (triggerIndex >= m_triggers.size()) {
            DBG << "can't activate invalid trigger";
            return;
        }
        DBG << "enabling trigger" << m_triggers[effect.trigger].name;
        m_triggers[triggerIndex].enabled = true;
        break;
    }
    case genie::TriggerEffect::DeactivateTrigger: {
        uint32_t triggerIndex = effect.trigger;
        if (triggerIndex >= m_triggers.size()) {
            DBG << "can't deactivate invalid trigger";
            return;
        }
        DBG << "disabling trigger" << m_triggers[effect.trigger].name;
        m_triggers[triggerIndex].enabled = false;
        break;
    }

        ///////////////
        // Chat stuff
    case genie::TriggerEffect::DisplayInstructions: {
        AudioPlayer::instance().playStream("scenario/" + effect.soundFile + ".mp3");
        std::string msg = effect.message;
        // Use localized string from language files when available
        if (effect.stringTableID >= 0) {
            std::string localized = LanguageManager::getString(effect.stringTableID);
            if (!localized.empty()) {
                msg = localized;
            }
        }
        if (m_engine) {
            m_engine->addMessage(msg);
        } else {
            WARN << "missing engine" << msg << effect.soundFile;
        }
        break;
    }
    case genie::TriggerEffect::SendChat:
        // Source player? But that is what aokts seems to use
        EventManager::sendChatMessage(-1, effect.sourcePlayer, effect.message);
        break;
    case genie::TriggerEffect::Sound:
        DBG << "Playing sound" << effect;
        AudioPlayer::instance().playStream("scenario/" + effect.soundFile);
        break;

        ///////////////
        // Camera stuff
    case genie::TriggerEffect::ChangeView:
        DBG << "Moving camera" << effect;

        // WARNING: flipped x and y
        m_gameState->moveCameraTo(MapPos(effect.location.y * Constants::TILE_SIZE, effect.location.x * Constants::TILE_SIZE));
        break;

        /////////////
        // Tech stuff
    case genie::TriggerEffect::ResearchTechnology: {
        DBG << "Researching" << effect;
        Player::Ptr player = m_gameState->player(effect.sourcePlayer);
        if (!player) {
            WARN << "couldn't get player for effect";
            break;
        }
        player->applyResearch(effect.technology);
        break;
    }
        ///////////////
        // Object stuff
    case genie::TriggerEffect::CreateObject: {
        DBG << "Creating unit" << effect;
        Player::Ptr player = m_gameState->player(effect.sourcePlayer);
        if (!player) {
            WARN << "couldn't get player for effect";
            break;
        }
        // WARNING: flipped x and y
        MapPos location(effect.location.y * Constants::TILE_SIZE, effect.location.x * Constants::TILE_SIZE);
        Unit::Ptr unit = UnitFactory::Inst().createUnit(effect.object, player, *m_gameState->unitManager());
        if (!unit) {
            WARN << "Failed to create unit";
            return;
        }
        DBG << "Created" << unit->debugName;
        m_gameState->unitManager()->add(unit, location);
        break;
    }
    case genie::TriggerEffect::RemoveObject: {
        DBG << "Removing unit" << effect;
        forEachMatchingUnit(effect, [this](const Unit::Ptr &unit) {
            DBG << "Removing unit" << unit->debugName;
            m_gameState->unitManager()->remove(unit);
        });
        break;
    }
    case genie::TriggerEffect::TaskObject: {
        // again with the wtf swap of x and y
        // TODO, not sure if it is right to move to the middle of the tile, but whatevs
        // WARNING: flipped x and y
        MapPos targetPos(effect.location.y + 0.5, effect.location.x + 0.5);
        targetPos *= Constants::TILE_SIZE;

        forEachMatchingUnit(effect, [this, &targetPos](const Unit::Ptr &unit) {
            DBG << "Tasking object" << unit->debugName;
            m_gameState->unitManager()->moveUnitTo(unit, targetPos);
        });
        break;
    }
    case genie::TriggerEffect::DamageObject: {
        DBG << "Damaging object" << effect;
        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            DBG << "Damaging unit" << unit->debugName << "for" << effect.amount;
            unit->takeDamage(effect.amount);
        });
        break;
    }
    case genie::TriggerEffect::ChangeObjectHP: {
        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            const float deltaHP = effect.amount - unit->healthLeft();
            DBG << "Changing unit HP" << unit->debugName << "for" << effect.amount;
            unit->takeDamage(deltaHP);
        });
        break;
    }
    case genie::TriggerEffect::SetUnitStance: {
        Unit::Stance stance = Unit::Stance::Invalid;
        switch(effect.boundedValue) {
        case 1:
            stance = Unit::Stance::Aggressive;
            break;
        case 2:
            stance = Unit::Stance::Defensive;
            break;
        case 3:
            stance = Unit::Stance::StandGround;
            break;
        case 4:
            stance = Unit::Stance::NoAttack;
            break;
        default:
            WARN << "Invalid stance" << effect.boundedValue;
            break;
        }

        if (stance == Unit::Stance::Invalid) {
            break;
        }

        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            unit->stance = stance;
        });
        break;
    }
    case genie::TriggerEffect::HD_HealObject: {
        forEachMatchingUnit(effect, [&](const Unit::Ptr &unit) {
            DBG << "Healing" << unit->debugName << "for" << effect.amount;
            unit->takeDamage(-effect.amount);
        });
        break;
    }
    case genie::TriggerEffect::ChangeObjectName:
        // debugName is const, so we can't change it. Log for now.
        DBG << "ChangeObjectName:" << effect.message;
        break;

        ////////////////////
        // Player stuff
    case genie::TriggerEffect::ChangeDiplomacy: {
        DBG << "changing diplomacy:" << effect;

        Player::Ptr player = m_gameState->player(effect.sourcePlayer);
        if (!player) {
            WARN << "couldn't get player for change diplomacy";
            break;
        }

        switch(effect.diplomacy) {
        case 0:
            player->setDiplomaticStance(effect.targetPlayer, Player::Allied);
            break;
        case 1:
            player->setDiplomaticStance(effect.targetPlayer, Player::Neutral);
            break;
        case 3:
            player->setDiplomaticStance(effect.targetPlayer, Player::Enemy);
            break;
        case 2:
        default:
            WARN << "Invalid stance" << effect.diplomacy;
            break;
        }
        break;
    }
    case genie::TriggerEffect::SendTribute: {
        DBG << "Sending tribute" << effect;
        Player::Ptr sourcePlayer = m_gameState->player(effect.sourcePlayer);
        if (!sourcePlayer) {
            WARN << "couldn't get source player for sending tribute";
            break;
        }
        Player::Ptr targetPlayer = m_gameState->player(effect.targetPlayer);
        if (!targetPlayer) {
            WARN << "couldn't get target player for sending tribute";
            break;
        }
        sourcePlayer->sendTribute(targetPlayer, genie::ResourceType(effect.resource), effect.amount);

        break;

    }

        ////////////////////
        // Game ending stuff
    case genie::TriggerEffect::DeclareVictory: {
        DBG << "DeclareVictory for player" << effect.sourcePlayer;
        m_gameState->onPlayerWin(effect.sourcePlayer);
        break;
    }
    default:
        WARN << "not implemented trigger effect" << effect;
        break;
    }
}

void ScenarioController::forEachMatchingUnit(const genie::TriggerEffect &effect, const std::function<void (const Unit::Ptr &)> &action)
{
    bool foundMatching = false;

    /// !! OBS !! notice that Y and X are flipped in the areas in the scn file
    const int fromX = std::min(effect.areaFrom.y, effect.areaTo.y);
    const int fromY = std::min(effect.areaFrom.x, effect.areaTo.x);
    const int toX = std::max(effect.areaFrom.y, effect.areaTo.y);
    const int toY = std::max(effect.areaFrom.x, effect.areaTo.x);
    bool foundUnits = false;
    for (int col = fromX; col <  toX; col++) {
        for (int row = fromY; row <  toY; row++) {
            const std::vector<std::weak_ptr<Entity>> &entities = m_gameState->map()->entitiesAt(col, row);
            for (const std::weak_ptr<Entity> &entity : entities) {
                Unit::Ptr unit = Unit::fromEntity(entity);
                if (!unit) {
                    WARN << "got invalid unit in area for effect";
                    continue;
                }
                foundUnits = true;
                if (!checkUnitMatchingEffect(unit, effect)) {
                    continue;
                }
                foundMatching = true;
                action(unit);
            }
        }
    }

    // WARNING: flipped x and y
    const std::vector<std::weak_ptr<Entity>> &entities = m_gameState->map()->entitiesAt(effect.location.y, effect.location.x);
    for (const std::weak_ptr<Entity> &entity : entities) {
        Unit::Ptr unit = Unit::fromEntity(entity);
        if (!unit) {
            WARN << "got invalid unit in area for effect";
            continue;
        }
        foundUnits = true;
        if (!checkUnitMatchingEffect(unit, effect)) {
            DBG << "Unit" << unit->debugName << "Not matching";
            continue;
        }
        foundMatching = true;
        action(unit);
    }

    if (!foundUnits) {
        DBG << "Found no units for effect" << effect;
    }

    if (!foundMatching) {
        WARN << "Found no matching units for effect" << effect;
    }
}

void ScenarioController::onUnitCreated(Unit *unit)
{
    for (Trigger &trigger : m_triggers) {
        if (!trigger.enabled) {
            continue;
        }

        for (Condition &condition : trigger.conditions) {
            switch(condition.data.type) {
            case genie::TriggerCondition::OwnObjects:
                if (condition.checkUnitMatching(unit)) {
                    condition.amountRequired--;
                }
                continue;
            case genie::TriggerCondition::OwnFewerObjects:
                if (condition.checkUnitMatching(unit)) {
                    condition.amountRequired++;
                }
                break;
            default:
                continue;
            }
        }
    }
}

void ScenarioController::onUnitMoved(Unit *unit, const MapPos &oldTile, const MapPos &newTile)
{
    for (Trigger &trigger : m_triggers) {
        if (!trigger.enabled) {
            continue;
        }

        for (Condition &condition : trigger.conditions) {
            switch(condition.data.type) {
            case genie::TriggerCondition::BringObjectToArea:
                // Falls through — same area check logic as ObjectsInArea
            case genie::TriggerCondition::ObjectsInArea:
                break;
            default:
                continue;
            }

            if (!condition.checkUnitMatching(unit)) {
                continue;
            }

            // WARNING: flipped x and y
            const MapRect conditionRect(MapPos(condition.data.areaFrom.y, condition.data.areaFrom.x),
                                        MapPos(condition.data.areaTo.y, condition.data.areaTo.x)
                );

            // Moved out of required area
            if (conditionRect.contains(oldTile) && !conditionRect.contains(newTile))  {
                DBG << unit->debugName << "moved to" << newTile << "out of" << conditionRect;
                condition.amountRequired++;
                continue;
            }

            // Moved into area
            if (!conditionRect.contains(oldTile) && conditionRect.contains(newTile))  {
                DBG << unit->debugName << "moved to" << newTile << "into" << conditionRect;
                condition.amountRequired--;
                continue;
            }
        }
    }

}

void ScenarioController::onUnitSelected(Unit *unit)
{
    // Don't check for trigger enabled here, the player might select before trigger is enabled
    for (Trigger &trigger : m_triggers) {
        for (Condition &condition : trigger.conditions) {
            switch(condition.data.type) {
            case genie::TriggerCondition::ObjectSelected:
                if (condition.checkUnitMatching(unit)) {
                    condition.amountRequired--;
                    DBG << "select condition match" << unit->spawnId << unit->debugName << unit->id << condition.data << condition.amountRequired;
                }
                break;
            default:
                continue;
            }
        }
    }
}

void ScenarioController::onUnitDeselected(const Unit *unit)
{
    // Don't check for trigger enabled here, the player might select before trigger is enabled
    for (Trigger &trigger : m_triggers) {
        for (Condition &condition : trigger.conditions) {
            switch(condition.data.type) {
            case genie::TriggerCondition::ObjectSelected:
                if (condition.checkUnitMatching(unit)) {
                    condition.amountRequired++;
                    DBG << "deselect condition match" << unit->spawnId << unit->debugName << unit->id << condition.data << condition.amountRequired;
                }
                break;
            default:
                continue;
            }
        }
    }
}

void ScenarioController::onPlayerDefeated(Player *player)
{
    if (!m_gameState) return;

    Player::Ptr human = m_gameState->humanPlayer();
    if (!human) return;

    if (player->playerId == human->playerId) {
        m_gameState->result = GameState::Result::Lost;
    } else {
        DBG << "Player" << player->playerId << "defeated";
    }
}

void ScenarioController::onAttributeChanged(Player *player, int attributeId, float newValue)
{
    for (Trigger &trigger : m_triggers) {
        if (!trigger.enabled) {
            continue;
        }

        for (Condition &condition : trigger.conditions) {
            if (condition.data.type != genie::TriggerCondition::AccumulateAttribute) {
                continue;
            }
            if (condition.data.resource != attributeId) {
                continue;
            }
            if (condition.data.sourcePlayer != -1 && condition.data.sourcePlayer != player->playerId) {
                continue;
            }

            condition.amountRequired += newValue;
        }
    }
}

void ScenarioController::onUnitDying(Unit *unit)
{
    DBG << "unit died" << unit->debugName << unit->spawnId;
    for (Trigger &trigger : m_triggers) {
        if (!trigger.enabled) {
            continue;
        }

        for (Condition &condition : trigger.conditions) {
            switch(condition.data.type) {
            case genie::TriggerCondition::DestroyObject:
                if (condition.checkUnitMatching(unit)) {
                    condition.amountRequired--;
                    DBG << "destroy condition match" << unit->spawnId << unit->debugName << unit->id << condition.data << condition.amountRequired;
                }
                break;
            case genie::TriggerCondition::OwnFewerObjects:
                if (condition.checkUnitMatching(unit)) {
                    condition.amountRequired--;
                    DBG << "fewer condition match" << unit->spawnId << unit->debugName << unit->id << condition.data << condition.amountRequired;
                }
                break;
            default:
                continue;
            }
        }
    }
}

bool ScenarioController::Condition::checkUnitMatching(const Unit *unit) const
{
    if (!unit) {
        WARN << "null unit";
        return false;
    }

    if (data.sourcePlayer > -1) {
        if (unit->playerId() != data.sourcePlayer) {
            return false;
        }
    }

    if (data.objectType > -1 && unit->data()->CombatLevel != data.objectType) {
        return false;
    }

    if (data.object > -1 && unit->data()->ID != data.object) {
        return false;
    }

    if (data.setObject > -1 && unit->spawnId != data.setObject) {
        return false;
    }


    return true;
}

bool ScenarioController::checkUnitMatchingEffect(const std::shared_ptr<Unit> &unit, const genie::TriggerEffect &effect)
{
    if (!unit) {
        WARN << "null unit";
        return false;
    }

    if (effect.sourcePlayer > -1) {
        if (unit->playerId() != effect.sourcePlayer) {
            return false;
        }
    }

    if (effect.objectType > -1 && unit->data()->CombatLevel != effect.objectType) {
        return false;
    }

    if (effect.object > -1 && unit->data()->ID != effect.object) {
        return false;
    }

    // could check area, but we already check it above, so meh


    return true;

}
