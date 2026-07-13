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
#include "RandomMapGenerator.h"
#ifdef ANDROID
#include <android/log.h>
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, "FreeAoE", __VA_ARGS__)
#else
#define ALOG(...)
#endif

#include "UnitFactory.h"
#include "ScenarioController.h"

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

void GameState::setupScenario()
{
    TIME_THIS;
    DBG << "Setting up scenario:" << scenario_->scenarioInstructions;
    map_->create(scenario_->map);

    const genie::ScnMainPlayerData &playersData = scenario_->playerData;

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
                aiPlayer->m_aiScript = std::make_shared<ai::AiScript>(aiPlayer.get());
                aiPlayer->m_basicAI = std::make_shared<BasicAI>(aiPlayer.get(), m_unitManager.get());
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
        int civId = 1 + (rand() % 13); // Random civilization
        auto aiPlayer = std::make_shared<AiPlayer>(i, civId, map_, defaultStartingResources[m_gameType]);
        aiPlayer->name = "AI " + std::to_string(i);
        aiPlayer->playerColor = i - 1;
        aiPlayer->m_aiScript = std::make_shared<ai::AiScript>(aiPlayer.get());
        aiPlayer->m_basicAI = std::make_shared<BasicAI>(aiPlayer.get(), m_unitManager.get());
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

    // Generate the map
    RandomMapGenerator::Settings settings;
    settings.type = static_cast<RandomMapGenerator::MapType>(mapType);
    settings.size = mapSize;
    settings.playerCount = playerCount;

    RandomMapGenerator::generate(settings, map_, *m_unitManager, m_players);

    // Compute tile frames, blends, slopes — required for rendering
    map_->updateMapData();

    // Center camera on human player's TC
    for (const Unit::Ptr &unit : m_unitManager->units()) {
        if (unit->playerId() == m_humanPlayer->playerId && unit->data()->ID == 109) {
            renderTarget_->camera()->setTargetPosition(unit->position());
            break;
        }
    }

    ALOG("Random map generated: type=%d size=%d players=%d", mapType, mapSize, playerCount);
}
