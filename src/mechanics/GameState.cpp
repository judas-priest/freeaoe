/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2011  <copyright holder> <email>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "GameState.h"
#include "ai/AiPlayer.h"
#include "ai/AiScript.h"
#include "ai/BasicAI.h"
#include "ai/ScriptLoader.h"
#include "RandomMapGenerator.h"
#ifdef ANDROID
#include <android/log.h>
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, "FreeAoE", __VA_ARGS__)
#else
#define ALOG(...)
#endif

#include "UnitFactory.h"
#include "ScenarioController.h"
#include "actions/IAction.h"
#include "actions/ActionAttack.h"
#include "net/LockstepManager.h"
#include "net/GameCommand.h"
#include "net/SyncRandom.h"

#include <Engine.h>
#ifdef USE_SDL2
#include "render/SdlRenderTarget.h"
#else
#include "render/SfmlRenderTarget.h"
#endif
#include "resource/DataManager.h"
#include "resource/AssetManager.h"
#include "core/Constants.h"
#include "core/Logger.h"
#include "core/ResourceMap.h"
#include "core/Types.h"
#include "debug/ISampleGame.h"
#include "debug/SampleGameFactory.h"
#include "debug/ISampleGame.h"
#include "global/EventManager.h"
#include "global/Config.h"
#include "mechanics/Building.h"
#include "mechanics/FormationHelper.h"
#include "mechanics/UnitManager.h"
#include "mechanics/Player.h"
#include "mechanics/Map.h"

#include "resource/LanguageManager.h"
#include "render/Camera.h"

#include <genie/resource/Color.h>
#include "genie/script/ScnFile.h"

#ifndef USE_SDL2
#ifndef USE_SDL2
#include <SFML/Graphics/RenderTarget.hpp>
#endif
#endif

#include <iostream>
#include <sstream>
#include <fstream>
#include <render/GraphicRender.h>

std::unordered_map<GameType, ResourceMap> GameState::defaultStartingResources = {
    {
        GameType::Default, {
            { genie::ResourceType::FoodStorage, 500 },
            { genie::ResourceType::WoodStorage, 500 },
            { genie::ResourceType::StoneStorage, 500 },
            { genie::ResourceType::GoldStorage, 500 },
        }
    },
    {
        GameType::HighResource, {
            { genie::ResourceType::FoodStorage, 1000 },
            { genie::ResourceType::WoodStorage, 1000 },
            { genie::ResourceType::StoneStorage, 800 },
            { genie::ResourceType::GoldStorage, 700 },
        }
    },
    {
        GameType::MediumResource, {
            { genie::ResourceType::FoodStorage, 500 },
            { genie::ResourceType::WoodStorage, 500 },
            { genie::ResourceType::StoneStorage, 400 },
            { genie::ResourceType::GoldStorage, 300 },
        }
    },
    {
        GameType::KingOfTheHill, {
            { genie::ResourceType::FoodStorage, 200 },
            { genie::ResourceType::WoodStorage, 200 },
            { genie::ResourceType::StoneStorage, 200 },
            { genie::ResourceType::GoldStorage, 100 },
        }
    },
    {
        GameType::Deathmatch, {
            { genie::ResourceType::FoodStorage, 20000 },
            { genie::ResourceType::WoodStorage, 20000 },
            { genie::ResourceType::StoneStorage, 5000 },
            { genie::ResourceType::GoldStorage, 10000 },
        }
    },
    {
        GameType::Regicide, {
            { genie::ResourceType::FoodStorage, 500 },
            { genie::ResourceType::WoodStorage, 500 },
            { genie::ResourceType::StoneStorage, 150 },
            { genie::ResourceType::GoldStorage, 0 },
        }
    },
};

#ifdef USE_SDL2
GameState::GameState(const std::shared_ptr<IRenderTarget> &renderTarget)
#else
GameState::GameState(const std::shared_ptr<SfmlRenderTarget> &renderTarget)
#endif
{
    m_unitManager = std::make_shared<UnitManager>();
    renderTarget_ = renderTarget;
    m_scenarioController = std::make_unique<ScenarioController>(this);

    EventManager::registerListener(this, EventManager::ResourceBought);
    EventManager::registerListener(this, EventManager::ResourceSold);
}

GameState::~GameState()
{
}

void GameState::setScenario(const std::shared_ptr<genie::ScnFile> &scenario)
{
    scenario_ = scenario;
}

bool GameState::init()
{
    TIME_THIS;
    if (!m_unitManager->init()) {
        return false;
    }

    // graphic 2962
    m_waypointFlag = AssetManager::Inst()->getSlp(3404);
    if (!m_waypointFlag) {
        WARN << "Failed to load waypoint animation";
    }

    map_ = std::make_shared<Map>();
    m_unitManager->setMap(map_);

    if (scenario_) {
        setupScenario();
    } else if (!m_skipDemoGame) {
        setupGame();
    }

    if (!m_skipDemoGame) {
        map_->updateMapData();
    }

    return true;
}

bool GameState::update(Time time)
{
    bool updated = false;

    // Lockstep synchronization: if enabled, update the lockstep manager
    // and only process the game tick when the current turn is ready.
    if (m_lockstep) {
        m_lockstep->update(static_cast<uint32_t>(time));

        if (m_lockstep->isReadyToAdvance()) {
            // Execute all commands for this turn
            auto commands = m_lockstep->commandsForCurrentTurn();
            if (!commands.empty()) {
                executeCommands(commands);
            }
            m_lockstep->advanceTurn();
        } else {
            // Not ready yet -- block game update, only return false
            return false;
        }
    }

    updated = m_unitManager->update(time) || updated;
    if (m_scenarioController) {
        updated = m_scenarioController->update(time) || updated;
    }

    // Update AI players
    static Time lastAiUpdate = 0;
    if (time - lastAiUpdate > 2000) {
        lastAiUpdate = time;
        for (const auto &aiPlayer : m_aiPlayers) {
            if (!aiPlayer || !aiPlayer->alive) continue;
            // Run hardcoded BasicAI (trains villagers, builds houses, trains military)
            if (aiPlayer->m_basicAI) {
                aiPlayer->m_basicAI->update(time);
            }
            // Run script-based AI rules (if any loaded from .per files)
            if (aiPlayer->m_aiScript) {
                aiPlayer->m_aiScript->update(time);
            }
        }
    }

    std::vector<Player::Ptr> playersAlive;
    for (const Player::Ptr &player : m_players) {
        if (player->alive) {
            playersAlive.push_back(player);
        }
    }

    if (playersAlive.size() == 1) {
        onPlayerWin(playersAlive[0]->playerId);
    }

    // Alliance win: all surviving players are mutual allies
    if (playersAlive.size() > 1) {
        bool anyEnemiesAlive = false;
        for (size_t i = 0; i < playersAlive.size() - 1 && !anyEnemiesAlive; i++) {
            for (size_t j = i + 1; j < playersAlive.size(); j++) {
                if (playersAlive[i]->isAllied(playersAlive[j]->playerId) &&
                    playersAlive[j]->isAllied(playersAlive[i]->playerId)) {
                    continue;
                }
                anyEnemiesAlive = true;
                break;
            }
        }
        if (!anyEnemiesAlive && m_humanPlayer) {
            // All remaining players are allies — human wins
            bool humanAlive = false;
            for (const auto &p : playersAlive) {
                if (p == m_humanPlayer) { humanAlive = true; break; }
            }
            if (humanAlive) {
                onPlayerWin(m_humanPlayer->playerId);
            }
        }
    }

    return updated;
}

Player::Ptr GameState::player(size_t id)
{
    if (id >= m_players.size()) {
        WARN << "asked for invalid player id" << id << m_players.size();
        return nullptr;
    }
    return m_players[id];
}

void GameState::moveCameraTo(const MapPos &newTarget)
{
    renderTarget_->camera()->setTargetPosition(newTarget);
}

void GameState::onPlayerWin(int playerId)
{
    DBG << "TODO: winner winner chicken dinner" << playerId;
    if (playerId == m_humanPlayer->playerId) {
        result = Result::Won;
    } else {
        result = Result::Lost;
    }
}

void GameState::onResourceBought(const genie::ResourceType type, const int amount)
{
    m_tradingPrices[type] = std::clamp(20, m_tradingPrices[type] + 2 * amount / 100, 9999);

    EventManager::tradingPriceChanged(type, m_tradingPrices[type]);
}

void GameState::onResourceSold(const genie::ResourceType type, const int amount)
{
    m_tradingPrices[type] = std::clamp(20, m_tradingPrices[type] - 2 * amount / 100, 9999);

    EventManager::tradingPriceChanged(type, m_tradingPrices[type]);
}

void GameState::setTradingPrice(const genie::ResourceType type, const int newPrice)
{
    m_tradingPrices[type] = newPrice;
    EventManager::tradingPriceChanged(type, m_tradingPrices[type]);
}

void GameState::setLockstepManager(const std::shared_ptr<LockstepManager> &lockstep)
{
    m_lockstep = lockstep;
    if (m_lockstep) {
        // Wire sync checksum callback
        m_lockstep->setSyncChecksumCallback([this]() -> uint32_t {
            return computeSyncChecksum();
        });
    }
}

bool GameState::isMultiplayer() const
{
    return m_lockstep && m_lockstep->isMultiplayer();
}

uint32_t GameState::computeSyncChecksum() const
{
    uint32_t checksum = 0;

    // Hash unit positions
    for (const auto &unit : m_unitManager->units()) {
        if (!unit || unit->isDead()) continue;
        // Simple hash: XOR position components and unit ID
        uint32_t ux = static_cast<uint32_t>(unit->position().x * 100);
        uint32_t uy = static_cast<uint32_t>(unit->position().y * 100);
        uint32_t uid = static_cast<uint32_t>(unit->id);
        checksum ^= (ux * 73856093) ^ (uy * 19349663) ^ (uid * 83492791);
    }

    // Hash player resources
    for (const auto &player : m_players) {
        if (!player) continue;
        uint32_t food = static_cast<uint32_t>(player->resourcesAvailable(genie::ResourceType::FoodStorage));
        uint32_t wood = static_cast<uint32_t>(player->resourcesAvailable(genie::ResourceType::WoodStorage));
        uint32_t gold = static_cast<uint32_t>(player->resourcesAvailable(genie::ResourceType::GoldStorage));
        uint32_t stone = static_cast<uint32_t>(player->resourcesAvailable(genie::ResourceType::StoneStorage));
        uint32_t pid = static_cast<uint32_t>(player->playerId);
        checksum ^= (food * 2654435761u) ^ (wood * 40503u) ^ (gold * 12764787u) ^ (stone * 73856093u) ^ pid;
    }

    // Include SyncRandom state
    checksum ^= SyncRandom::inst().state() * 2246822519u;

    return checksum;
}

void GameState::executeCommands(const std::vector<GameCommand> &commands)
{
    for (const auto &cmd : commands) {
        switch (cmd.type) {
        case CommandType::Move: {
            MapPos targetPos(cmd.x, cmd.y, 0);

            std::vector<Unit::Ptr> units;
            for (int unitId : cmd.unitIds) {
                Unit::Ptr unit = m_unitManager->unitById(static_cast<size_t>(unitId));
                if (unit && unit->isAlive()) {
                    units.push_back(unit);
                }
            }

            if (units.size() > 1) {
                MapPos center;
                for (auto &u : units) {
                    center.x += u->position().x;
                    center.y += u->position().y;
                }
                center.x /= units.size();
                center.y /= units.size();
                float angle = std::atan2(targetPos.y - center.y, targetPos.x - center.x);

                auto positions = FormationHelper::computePositions(
                    units, targetPos, Unit::s_formation, angle);

                for (size_t i = 0; i < units.size(); ++i) {
                    m_unitManager->moveUnitTo(units[i], positions[i]);
                }
            } else {
                for (auto &unit : units) {
                    m_unitManager->moveUnitTo(unit, targetPos);
                }
            }
            break;
        }

        case CommandType::Attack: {
            Unit::Ptr target = m_unitManager->unitById(static_cast<size_t>(cmd.targetId));
            for (int unitId : cmd.unitIds) {
                Unit::Ptr unit = m_unitManager->unitById(static_cast<size_t>(unitId));
                if (!unit || !unit->isAlive()) continue;

                if (target) {
                    Task task = unit->actions.findAnyTask(genie::ActionType::Attack, target->data()->ID);
                    task.target = target;
                    IAction::assignTask(task, unit, IAction::AssignType::Replace);
                } else {
                    // Attack-ground at position
                    MapPos targetPos(cmd.x, cmd.y, 0);
                    auto action = std::make_shared<ActionAttack>(unit, targetPos, unit->actions.findAnyTask(genie::ActionType::Attack, -1));
                    unit->actions.setCurrentAction(action);
                }
            }
            break;
        }

        case CommandType::Build: {
            if (cmd.buildingType < 0) break;
            Player::Ptr owner = player(cmd.playerId);
            if (!owner) break;

            MapPos pos(cmd.x, cmd.y, 0);
            Unit::Ptr building = UnitFactory::createUnit(cmd.buildingType, owner, *m_unitManager);
            if (!building) break;

            building->isVisible = true;
            m_unitManager->add(building, pos);
            building->setCreationProgress(0.f);

            // Assign all selected builders to construct it
            for (int unitId : cmd.unitIds) {
                Unit::Ptr builder = m_unitManager->unitById(static_cast<size_t>(unitId));
                if (!builder || !builder->isAlive()) continue;

                Task task;
                for (const Task &potential : builder->actions.availableActions()) {
                    if (potential.data->ActionType == genie::ActionType::Build) {
                        task = potential;
                        break;
                    }
                }
                if (!task.data) continue;

                task.target = building;
                IAction::assignTask(task, builder, IAction::AssignType::Replace);
            }
            break;
        }

        case CommandType::Train: {
            if (cmd.unitType < 0) break;
            for (int unitId : cmd.unitIds) {
                Unit::Ptr producer = m_unitManager->unitById(static_cast<size_t>(unitId));
                if (!producer) continue;
                Player::Ptr owner = producer->player().lock();
                if (!owner) continue;
                const genie::Unit *unitData = &owner->civilization.unitData(cmd.unitType);
                UnitVector producers = { producer };
                m_unitManager->enqueueProduceUnit(unitData, producers);
            }
            break;
        }

        case CommandType::Research: {
            if (cmd.techId < 0) break;
            for (int unitId : cmd.unitIds) {
                Unit::Ptr producer = m_unitManager->unitById(static_cast<size_t>(unitId));
                if (!producer) continue;
                Player::Ptr owner = producer->player().lock();
                if (!owner) continue;
                const std::vector<genie::Tech> &techs = DataManager::Inst().allTechs();
                if (cmd.techId >= 0 && cmd.techId < static_cast<int>(techs.size())) {
                    const genie::Tech *techData = &techs[cmd.techId];
                    UnitVector producers = { producer };
                    m_unitManager->enqueueResearch(techData, producers);
                }
            }
            break;
        }

        case CommandType::Delete: {
            for (int unitId : cmd.unitIds) {
                Unit::Ptr unit = m_unitManager->unitById(static_cast<size_t>(unitId));
                if (unit && unit->isAlive()) {
                    unit->kill();
                }
            }
            break;
        }

        case CommandType::Stop: {
            for (int unitId : cmd.unitIds) {
                Unit::Ptr unit = m_unitManager->unitById(static_cast<size_t>(unitId));
                if (unit && unit->isAlive()) {
                    unit->actions.clearActionQueue();
                }
            }
            break;
        }

        case CommandType::Chat: {
            EventManager::sendChatMessage(cmd.playerId, cmd.targetId, cmd.message);
            break;
        }

        case CommandType::Garrison: {
            Unit::Ptr target = m_unitManager->unitById(static_cast<size_t>(cmd.targetId));
            if (!target || !target->isAlive()) break;

            for (int unitId : cmd.unitIds) {
                Unit::Ptr unit = m_unitManager->unitById(static_cast<size_t>(unitId));
                if (!unit || !unit->isAlive()) continue;

                Task task = unit->actions.findAnyTask(genie::ActionType::Garrison, target->data()->ID);
                if (!task.data) continue;
                task.target = target;
                IAction::assignTask(task, unit, IAction::AssignType::Replace);
            }
            break;
        }

        case CommandType::Ungarrison: {
            for (int unitId : cmd.unitIds) {
                Unit::Ptr container = m_unitManager->unitById(static_cast<size_t>(unitId));
                if (!container || !container->isAlive()) continue;

                auto building = Building::fromUnit(container);
                if (building) {
                    building->ungarrisonAll();
                } else {
                    // Transport ship / ram: place units on land if on water
                    int cx = static_cast<int>(container->position().x) / Constants::TILE_SIZE;
                    int cy = static_cast<int>(container->position().y) / Constants::TILE_SIZE;
                    bool onWater = map_->isValidTile(cx, cy) && map_->isWaterTile(cx, cy);

                    if (onWater) {
                        MapPos landPos = map_->nearestLandTile(container->position());
                        int count = 0;
                        for (auto &w : container->garrisonedUnits) {
                            if (w.lock()) count++;
                        }
                        int i = 0;
                        for (auto it = container->garrisonedUnits.begin(); it != container->garrisonedUnits.end(); ) {
                            auto u = it->lock();
                            if (u) {
                                u->garrisonedInUnit.reset();
                                float angle = (2.f * M_PI * i) / std::max(1, count);
                                MapPos exitPos = landPos;
                                exitPos.x += std::cos(angle) * Constants::TILE_SIZE;
                                exitPos.y += std::sin(angle) * Constants::TILE_SIZE;
                                u->setPosition(exitPos);
                                i++;
                            }
                            it = container->garrisonedUnits.erase(it);
                        }
                    } else {
                        container->ungarrisonAllUnits();
                    }
                }
            }
            break;
        }

        case CommandType::BuyResource: {
            Player::Ptr owner = player(cmd.playerId);
            if (!owner) break;
            int resType = cmd.resourceType; // 0=Food, 1=Wood, 2=Stone
            if (resType < 0 || resType > 2) break;

            int buyPrice = owner->marketPrices.buyPrice(resType);
            float goldAvailable = owner->resourcesAvailable(genie::ResourceType::GoldStorage);

            if (goldAvailable >= buyPrice) {
                owner->removeResource(genie::ResourceType::GoldStorage, buyPrice);
                genie::ResourceType targetRes;
                switch (resType) {
                    case 0: targetRes = genie::ResourceType::FoodStorage; break;
                    case 1: targetRes = genie::ResourceType::WoodStorage; break;
                    default: targetRes = genie::ResourceType::StoneStorage; break;
                }
                owner->addResource(targetRes, 100);
                owner->marketPrices.onBuy(resType);
            }
            break;
        }

        case CommandType::SellResource: {
            Player::Ptr owner = player(cmd.playerId);
            if (!owner) break;
            int resType = cmd.resourceType;
            if (resType < 0 || resType > 2) break;

            genie::ResourceType sourceRes;
            switch (resType) {
                case 0: sourceRes = genie::ResourceType::FoodStorage; break;
                case 1: sourceRes = genie::ResourceType::WoodStorage; break;
                default: sourceRes = genie::ResourceType::StoneStorage; break;
            }

            float available = owner->resourcesAvailable(sourceRes);
            if (available >= 100) {
                owner->removeResource(sourceRes, 100);
                int sellPrice = owner->marketPrices.sellPrice(resType);
                owner->addResource(genie::ResourceType::GoldStorage, sellPrice);
                owner->marketPrices.onSell(resType);
            }
            break;
        }

        case CommandType::Tribute: {
            Player::Ptr sender = player(cmd.playerId);
            Player::Ptr receiver = player(cmd.targetId);
            if (!sender || !receiver) break;

            genie::ResourceType resType;
            switch (cmd.resourceType) {
                case 0: resType = genie::ResourceType::FoodStorage; break;
                case 1: resType = genie::ResourceType::WoodStorage; break;
                case 2: resType = genie::ResourceType::StoneStorage; break;
                default: resType = genie::ResourceType::GoldStorage; break;
            }

            float amount = static_cast<float>(cmd.amount);
            if (sender->resourcesAvailable(resType) >= amount) {
                sender->removeResource(resType, amount);
                float received = amount * 0.75f; // 25% tribute tax
                receiver->addResource(resType, received);
            }
            break;
        }

        case CommandType::SetFormation: {
            int formIdx = cmd.amount;
            if (formIdx >= 0 && formIdx <= 3) {
                Unit::s_formation = static_cast<Unit::Formation>(formIdx);
            }
            break;
        }

        default:
            DBG << "executeCommands: unhandled command type" << static_cast<int>(cmd.type);
            break;
        }
    }
}

void GameState::setupScenario()
{
    TIME_THIS;
    DBG << "Setting up scenario:" << scenario_->scenarioInstructions;
    map_->create(scenario_->map);

    const genie::ScnMainPlayerData &playersData = scenario_->playerData;

    for (const auto &script : scenario_->includedFiles) {
        if (script.content.empty()) continue;
        ALOG("Scenario includes AI script: %s (%zu bytes)", script.filename.c_str(), script.content.size());
    }

    int humanPlayerId = 1;
    for (size_t playerNum = 0; playerNum < scenario_->enabledPlayerCount + 1; playerNum++) { // +1 for gaia
        Player::Ptr player;

        int realPlayerNum = 0;

        // player 0 is gaia, but the layout in the scn files is extremely confusing, so some data is at index 8
        if (playerNum == UnitManager::GaiaID) {
            realPlayerNum = 8;

            player = std::make_shared<Player>(playerNum, UnitManager::GaiaID, map_);
            player->civilization.setGaiaOverrideCiv(playersData.resourcesPlusPlayerInfo[realPlayerNum].civilizationID);
            player->name = "Gaia";
            player->playerColor = -1;
        } else {
            realPlayerNum = playerNum - 1;

            // Create AiPlayer for non-human players
            bool isHuman = playersData.resourcesPlusPlayerInfo[realPlayerNum].isHuman;
            if (isHuman) {
                player = std::make_shared<Player>(playerNum, playersData.resourcesPlusPlayerInfo[realPlayerNum].civilizationID, map_);
            } else {
                auto aiPlayer = std::make_shared<AiPlayer>(playerNum, playersData.resourcesPlusPlayerInfo[realPlayerNum].civilizationID, map_);
                aiPlayer->setDifficulty(m_difficulty);
                aiPlayer->m_aiScript = std::make_shared<ai::AiScript>(aiPlayer.get());
                aiPlayer->m_basicAI = std::make_shared<BasicAI>(aiPlayer.get(), m_unitManager.get());

                if (realPlayerNum < static_cast<int>(playersData.aiFiles.size())) {
                    const genie::AiFile &aiFile = playersData.aiFiles[realPlayerNum];
                    if (!aiFile.perFile.empty()) {
                        ALOG("Loading AI .per script for player %d (%zu bytes)", (int)playerNum, aiFile.perFile.size());
                        ai::ScriptLoader loader(aiPlayer.get());
                        std::istringstream scriptStream(aiFile.perFile);
                        std::ostringstream debugOut;
                        int parseResult = loader.parse(scriptStream, debugOut);
                        if (parseResult == 0) {
                            aiPlayer->m_aiScript = loader.script();
                            ALOG("AI .per script loaded successfully for player %d, %zu rules", (int)playerNum, aiPlayer->m_aiScript->rules.size());
                        } else {
                            ALOG("AI .per script parse failed for player %d (result=%d)", (int)playerNum, parseResult);
                        }
                    }
                }

                player = aiPlayer;
                m_aiPlayers.push_back(aiPlayer);
                ALOG("Created AI player %d with BasicAI", (int)playerNum);
            }
            player->name = playersData.playerNames[realPlayerNum];
            player->playerColor = scenario_->players[realPlayerNum].playerColor;
        }

        const genie::ScnPlayerResources &resources = scenario_->playerResources[playerNum];
        player->setAvailableResource(genie::ResourceType::GoldStorage,  resources.gold);
        player->setAvailableResource(genie::ResourceType::FoodStorage,  resources.food);
        player->setAvailableResource(genie::ResourceType::WoodStorage,  resources.wood);
        player->setAvailableResource(genie::ResourceType::StoneStorage, resources.stone);
        player->setAvailableResource(genie::ResourceType::OreStorage,   resources.ore);
        player->setAvailableResource(genie::ResourceType::TradeGoods,   resources.goods);
        player->setAvailableResource(genie::ResourceType::CurrentPopulation, resources.popLimit);

        if (playersData.resourcesPlusPlayerInfo[realPlayerNum].isHuman) {
            if (m_humanPlayer) {
                WARN << "multiple human players defined" << m_humanPlayer->playerId << realPlayerNum;
            }
            m_humanPlayer = player;
            humanPlayerId = playerNum;
        }

        m_players.push_back(player);
    }

    if (!m_humanPlayer) {
        WARN << "no human player defined, setting to 1. player";
        m_humanPlayer = m_players[1];
    }
    ALOG("Human player ID: %d, total players: %zu", m_humanPlayer->playerId, m_players.size());

    // Apply team bonuses to allied players
    for (const Player::Ptr &player : m_players) {
        if (player->playerId == 0) continue; // skip gaia
        const genie::Civ &civData = DataManager::Inst().civilization(player->civilization.id());
        if (civData.TeamBonusID < 0) continue;
        for (Player::Ptr &ally : m_players) {
            if (ally->playerId == 0) continue;
            if (!ally->isAllied(player->playerId)) continue;
            ally->applyTechEffect(civData.TeamBonusID);
        }
    }

    // Apply scenario-disabled techs, units, and buildings
    const genie::ScnDisables &disables = scenario_->playerData.disables;
    for (size_t playerIdx = 1; playerIdx < m_players.size(); playerIdx++) {
        size_t disableIdx = playerIdx - 1;
        if (disableIdx >= 16) break;
        Player::Ptr &player = m_players[playerIdx];

        for (size_t i = 0; i < disables.numDisabledTechs[disableIdx] && i < disables.disabledTechs[disableIdx].size(); i++) {
            uint32_t techId = disables.disabledTechs[disableIdx][i];
            if (techId != 0xFFFFFFFF) player->civilization.disableTech(static_cast<uint16_t>(techId));
        }
        for (size_t i = 0; i < disables.numDisabledUnits[disableIdx] && i < disables.disabledUnits[disableIdx].size(); i++) {
            uint32_t unitId = disables.disabledUnits[disableIdx][i];
            if (unitId != 0xFFFFFFFF) player->civilization.disableUnit(static_cast<uint16_t>(unitId));
        }
        for (size_t i = 0; i < disables.numDisabledBuildings[disableIdx] && i < disables.disabledBuildings[disableIdx].size(); i++) {
            uint32_t bldId = disables.disabledBuildings[disableIdx][i];
            if (bldId != 0xFFFFFFFF) player->civilization.disableUnit(static_cast<uint16_t>(bldId));
        }
    }

    m_unitManager->setPlayers(m_players);
    m_unitManager->setHumanPlayer(m_humanPlayer);

    int totalHumanUnits = 0;
    for (size_t playerNum = 0; playerNum < scenario_->enabledPlayerCount + 1; playerNum++) { // +1 for gaia
        const Player::Ptr &player = m_players[playerNum];
        ALOG("Player %zu units: %zu", playerNum, scenario_->playerUnits[playerNum].units.size());

        for (const genie::ScnUnit &scnunit : scenario_->playerUnits[playerNum].units) {
            MapPos unitPos((scnunit.positionY) * Constants::TILE_SIZE, (scnunit.positionX) * Constants::TILE_SIZE, scnunit.positionZ * DataManager::Inst().terrainBlock().ElevHeight);
            Unit::Ptr unit = UnitFactory::createUnit(scnunit.objectID, player, *m_unitManager);
            if (!unit) {
                WARN << "Failed to create unit";
                continue;
            }
            unit->spawnId = scnunit.spawnID;
            m_unitManager->add(unit, unitPos);

            unit->setAngle(scnunit.rotation - M_PI_2/2.);

            if (unit->renderer().frameCount()) {
                unit->renderer().setCurrentFrame(scnunit.initAnimationFrame % unit->renderer().frameCount());
            } else {
//                WARN << "invalid graphics";
            }
            if (player == m_humanPlayer) totalHumanUnits++;
        }
    }
    ALOG("Total human player units: %d", totalHumanUnits);

    MapPos cameraPos;
    bool cameraSet = false;

    // Try scenario camera position (WARNING: X and Y are swapped in genie format)
    if (scenario_->playerData.player1CameraX > 0 && scenario_->playerData.player1CameraY > 0) {
        cameraPos = MapPos(scenario_->playerData.player1CameraY * Constants::TILE_SIZE,
                           scenario_->playerData.player1CameraX * Constants::TILE_SIZE);
        cameraSet = true;
        ALOG("Camera from playerData (swapped): %.0f, %.0f", cameraPos.x, cameraPos.y);
    }

    // Try per-player camera position (players[] is 0-indexed, humanPlayerId is 1-based)
    int playerIdx = humanPlayerId - 1;
    if (!cameraSet && playerIdx >= 0 && playerIdx < static_cast<int>(scenario_->players.size())) {
        float cx = scenario_->players[playerIdx].initCameraX;
        float cy = scenario_->players[playerIdx].initCameraY;
        if (cx > 0 && cy > 0) {
            cameraPos = MapPos(cy * Constants::TILE_SIZE, cx * Constants::TILE_SIZE);
            cameraSet = true;
            ALOG("Camera from players[%d] (swapped): %.0f, %.0f", humanPlayerId, cameraPos.x, cameraPos.y);
        }
    }

    // Fall back to first human player unit position
    if (!cameraSet && humanPlayerId < static_cast<int>(scenario_->playerUnits.size())) {
        for (const genie::ScnUnit &u : scenario_->playerUnits[humanPlayerId].units) {
            cameraPos = MapPos(u.positionY * Constants::TILE_SIZE, u.positionX * Constants::TILE_SIZE);
            cameraSet = true;
            ALOG("Camera from first unit: %.0f, %.0f", cameraPos.x, cameraPos.y);
            break;
        }
    }

    if (cameraSet) {
        renderTarget_->camera()->setTargetPosition(cameraPos);
    }

    m_scenarioController->setScenario(scenario_);
}

void GameState::setupGame()
{
    if (Config::Inst().isOptionSet(Config::GameSample)) {
        SampleGameFactory::Inst().setSampleFromAlias(Config::Inst().getValue(Config::GameSample));
    }
    SampleGamePtr sampleGameSetup = SampleGameFactory::Inst().createGameSetup(map_, m_unitManager);

    sampleGameSetup->setupMap();
    sampleGameSetup->setupActors(defaultStartingResources[m_gameType]);

    m_humanPlayer = sampleGameSetup->getHumanPlayer();
    m_unitManager->setHumanPlayer(m_humanPlayer);

    m_players.push_back(sampleGameSetup->getGaiaPlayer());
    m_players.push_back(m_humanPlayer);
    m_players.push_back(sampleGameSetup->getEnemyPlayer());

    m_unitManager->setPlayers(m_players);
    sampleGameSetup->setupUnits();

    MapPos cameraPos(map_->pixelWidth() / 2.f, map_->pixelHeight()  / 2.f);
    renderTarget_->camera()->setTargetPosition(cameraPos);
}

void GameState::setupRandomMap(int mapType, int mapSize, int playerCount)
{
    // Note: old demo game players/units remain in UnitManager.
    // We just override m_players and m_humanPlayer.
    // Old units will be on the old map tiles which get overwritten.
    m_players.clear();
    m_aiPlayers.clear();

    // Create players
    auto gaiaPlayer = std::make_shared<Player>(0, 0, map_);
    gaiaPlayer->name = "Gaia";
    gaiaPlayer->playerColor = -1;
    m_players.push_back(gaiaPlayer);

    // Human player (always player 1, civ 1 = Briton)
    m_humanPlayer = std::make_shared<Player>(1, 1, map_, defaultStartingResources[m_gameType]);
    m_humanPlayer->name = "You";
    m_humanPlayer->playerColor = 0;
    m_players.push_back(m_humanPlayer);

    // AI players
    for (int i = 2; i <= playerCount; i++) {
        int civId = 1 + SyncRandom::inst().nextInt(13); // Random civilization
        auto aiPlayer = std::make_shared<AiPlayer>(i, civId, map_, defaultStartingResources[m_gameType]);
        aiPlayer->name = "AI " + std::to_string(i);
        aiPlayer->playerColor = i - 1;
        aiPlayer->setDifficulty(m_difficulty);
        aiPlayer->m_aiScript = std::make_shared<ai::AiScript>(aiPlayer.get());
        aiPlayer->m_basicAI = std::make_shared<BasicAI>(aiPlayer.get(), m_unitManager.get());

        {
            std::string aiPath = Config::Inst().getValue(Config::GamePath) + "/Ai/";
            static const char *defaultScripts[] = {
                "RandomGame.per", "randomgame.per",
                "The Horde.per", "the horde.per",
                nullptr
            };
            for (const char **name = defaultScripts; *name; ++name) {
                std::string fullPath = aiPath + *name;
                std::ifstream scriptFile(fullPath);
                if (scriptFile.is_open()) {
                    ALOG("Loading default AI script: %s", fullPath.c_str());
                    ai::ScriptLoader loader(aiPlayer.get());
                    std::ostringstream debugOut;
                    int result = loader.parse(scriptFile, debugOut);
                    if (result == 0) {
                        aiPlayer->m_aiScript = loader.script();
                        ALOG("Default AI script loaded, %zu rules", aiPlayer->m_aiScript->rules.size());
                    }
                    break;
                }
            }
        }

        m_players.push_back(aiPlayer);
        m_aiPlayers.push_back(aiPlayer);
    }

    m_unitManager->setPlayers(m_players);
    m_unitManager->setHumanPlayer(m_humanPlayer);

    // Set diplomacy — everyone is enemy to everyone else
    for (auto &p1 : m_players) {
        for (auto &p2 : m_players) {
            if (p1 == p2 || p1->playerId == 0 || p2->playerId == 0) continue;
            if (p1 != m_humanPlayer && p2 != m_humanPlayer) {
                // AI players neutral to each other
                p1->setDiplomaticStance(p2->playerId, Player::Neutral);
            } else if (p1 != p2) {
                p1->setDiplomaticStance(p2->playerId, Player::Enemy);
            }
        }
    }

    // Apply team bonuses to allied players
    for (const Player::Ptr &player : m_players) {
        if (player->playerId == 0) continue; // skip gaia
        const genie::Civ &civData = DataManager::Inst().civilization(player->civilization.id());
        if (civData.TeamBonusID < 0) continue;
        for (Player::Ptr &ally : m_players) {
            if (ally->playerId == 0) continue;
            if (!ally->isAllied(player->playerId)) continue;
            ally->applyTechEffect(civData.TeamBonusID);
        }
    }

    // Deathmatch: start in Imperial Age
    if (m_gameType == GameType::Deathmatch) {
        for (auto &p : m_players) {
            if (p->playerId == 0) continue;
            p->setAge(Player::ImperialAge);
        }
    }

    // Generate the map
    RandomMapGenerator::Settings settings;
    settings.type = static_cast<RandomMapGenerator::MapType>(mapType);
    settings.size = mapSize;
    settings.playerCount = playerCount;

    RandomMapGenerator::generate(settings, map_, *m_unitManager, m_players);

    // Compute tile frames, blends, slopes — required for rendering
    map_->updateMapData();

    // Regicide: spawn King (ID 434) near each player's TC
    if (m_gameType == GameType::Regicide) {
        for (const auto &player : m_players) {
            if (!player || player->playerId == 0) continue;
            // Find player's TC position
            for (const Unit::Ptr &unit : m_unitManager->units()) {
                if (unit->playerId() == player->playerId && unit->data()->ID == 109) {
                    Unit::Ptr king = UnitFactory::createUnit(434, player, *m_unitManager);
                    if (king) {
                        MapPos kingPos = unit->position();
                        kingPos.x += Constants::TILE_SIZE * 2;
                        kingPos.y += Constants::TILE_SIZE * 2;
                        m_unitManager->add(king, kingPos);
                    }
                    break;
                }
            }
        }
    }

    // Center camera on human player's TC
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (unit->playerId() == m_humanPlayer->playerId && unit->data()->ID == 109) {
            renderTarget_->camera()->setTargetPosition(unit->position());
            break;
        }
    }

    ALOG("Random map generated: type=%d size=%d players=%d", mapType, mapSize, playerCount);

    // DEBUG: Dump villager tasks to diagnose gathering
    {
        const int villagerId = 83;
        const auto &directTasks = DataManager::Inst().getTasks(villagerId);
        ALOG("=== GATHERING DEBUG: Villager (ID=%d) direct tasks: %zu ===", villagerId, directTasks.size());
        for (size_t i = 0; i < directTasks.size(); i++) {
            const auto &t = directTasks[i];
            ALOG("  Task[%zu]: ActionType=%d ClassID=%d UnitID=%d TargetDiplo=%d ResIn=%d ResOut=%d WorkVal=%.2f",
                 i, (int)t.ActionType, t.ClassID, t.UnitID, t.TargetDiplomacy, t.ResourceIn, t.ResourceOut, t.WorkValue1);
        }

        // Check TaskSwapGroup
        const auto &villagerData = m_humanPlayer->civilization.unitData(villagerId);
        int swapGroup = villagerData.Action.TaskSwapGroup;
        ALOG("  Villager TaskSwapGroup=%d", swapGroup);

        if (swapGroup && m_humanPlayer) {
            const auto &swappables = m_humanPlayer->civilization.swappableUnits(swapGroup);
            ALOG("  Swappable units in group %d: %zu", swapGroup, swappables.size());
            for (const genie::Unit *su : swappables) {
                const auto &suTasks = DataManager::Inst().getTasks(su->ID);
                for (size_t i = 0; i < suTasks.size(); i++) {
                    const auto &t = suTasks[i];
                    if (t.ActionType == genie::ActionType::GatherRebuild ||
                        t.ActionType == genie::ActionType::Hunt) {
                        ALOG("  SwapUnit[%d] Task: ActionType=%d ClassID=%d UnitID=%d TargetDiplo=%d ResIn=%d ResOut=%d",
                             su->ID, (int)t.ActionType, t.ClassID, t.UnitID, t.TargetDiplomacy, t.ResourceIn, t.ResourceOut);
                    }
                }
            }
        }

        // Dump first few Gaia resource units
        int resCount = 0;
        for (const Unit::Ptr &u : m_unitManager->units()) {
            if (u->playerId() == 0 && u->data()->CanBeGathered && resCount < 5) {
                ALOG("  GaiaResource: ID=%d Class=%d Name=%s OutlineSize=%.1fx%.1f resources:",
                     u->data()->ID, u->data()->Class, u->data()->Name.c_str(),
                     u->data()->OutlineSize.x, u->data()->OutlineSize.y);
                for (const auto &res : u->resources) {
                    ALOG("    ResType=%d Amount=%.1f", (int)res.first, res.second);
                }
                resCount++;
            }
        }
        ALOG("=== END GATHERING DEBUG ===");
    }
}
