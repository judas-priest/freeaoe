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

#pragma once

#include "IState.h"

#include "ai/gen/enums.h"
#include "core/ResourceMap.h"

#include "global/EventListener.h"

#include <memory>
#include <vector>
#include <unordered_map>

struct Player;
struct AiPlayer;
struct GameCommand;
class Map;
class UnitManager;
class ScenarioController;
class LockstepManager;

using MapPtr = std::shared_ptr<Map>;

namespace genie {
class ScnFile;
class SlpFile;
typedef std::shared_ptr<SlpFile> SlpFilePtr;
}

struct Player;
using PlayerPtr = std::shared_ptr<Player>;

enum class GameType {
    Default,
    HighResource,
    MediumResource,
    KingOfTheHill,
    Deathmatch,
    SuddenDeath,
    Regicide,
    WonderRace
};

#ifdef USE_SDL2
class IRenderTarget;
#else
namespace sf {
class RenderTarget;
}
class SfmlRenderTarget;
#endif

//------------------------------------------------------------------------------
/// State where the game is processed
//
class GameState : public IState, public EventListener
{
public:
    enum class Result {
        Won,
        Lost,
        Running // meh names
    } result = Result::Running;


    static std::unordered_map<GameType, ResourceMap> defaultStartingResources;

#ifdef USE_SDL2
    GameState(const std::shared_ptr<IRenderTarget> &renderTarget);
#else
    GameState(const std::shared_ptr<SfmlRenderTarget> &renderTarget);
#endif
    virtual ~GameState();

    void setScenario(const std::shared_ptr<genie::ScnFile> &scenario);
    void setGameType(const GameType &type) { m_gameType = type; }
    GameType gameType() const { return m_gameType; }
    void setDifficulty(ai::DifficultyLevel d) { m_difficulty = d; }
    ai::DifficultyLevel difficulty() const { return m_difficulty; }
    void setSkipDemoGame(bool skip) { m_skipDemoGame = skip; }
    void setupRandomMap(int mapType, int mapSize, int playerCount);

    bool init() override;

    bool update(Time time) override;

    const std::shared_ptr<Player> &humanPlayer() { return m_humanPlayer; }

    std::shared_ptr<Player> player(size_t id);

    const std::shared_ptr<UnitManager> &unitManager() { return m_unitManager; }
    const MapPtr &map() const { return map_; }
    const std::vector<std::shared_ptr<Player>> &players() const { return m_players; }

    void moveCameraTo(const MapPos &newTarget);

    void onPlayerWin(int playerId);
    const std::unique_ptr<ScenarioController> &scenarioController() const { return m_scenarioController; }

    int sellPrice(const genie::ResourceType type) { return (0.7 * m_tradingPrices[type]); }
    int buyPrice(const genie::ResourceType type) { return (1.3 * m_tradingPrices[type]); }

    void onResourceBought(const genie::ResourceType type, const int amount) override;
    void onResourceSold(const genie::ResourceType type, const int amount) override;

    void setTradingPrice(const genie::ResourceType type, const int newPrice);

    /// Set the lockstep manager for multiplayer synchronization.
    void setLockstepManager(const std::shared_ptr<LockstepManager> &lockstep) { m_lockstep = lockstep; }
    const std::shared_ptr<LockstepManager> &lockstepManager() const { return m_lockstep; }

    /// Execute a batch of serialized game commands (from lockstep turns).
    void executeCommands(const std::vector<GameCommand> &commands);

private:
    void setupScenario();
    void setupGame();

    GameState(const GameState &other) = delete;

#ifdef USE_SDL2
    std::shared_ptr<IRenderTarget> renderTarget_;
#else
    std::shared_ptr<SfmlRenderTarget> renderTarget_;
#endif

    std::shared_ptr<UnitManager> m_unitManager;

    MapPtr map_;

    std::shared_ptr<genie::ScnFile> scenario_;

    std::shared_ptr<genie::SlpFile> m_waypointFlag;

    std::shared_ptr<Player> m_humanPlayer;
    std::vector<std::shared_ptr<Player>> m_players;
    std::vector<std::shared_ptr<AiPlayer>> m_aiPlayers;

    GameType m_gameType = GameType::Default;
    ai::DifficultyLevel m_difficulty = ai::DifficultyLevel::Moderate;
    bool m_skipDemoGame = false;

    std::unique_ptr<ScenarioController> m_scenarioController;
    std::shared_ptr<LockstepManager> m_lockstep;

    std::unordered_map<genie::ResourceType, int> m_tradingPrices = {
        { genie::ResourceType::FoodStorage, 100 },
        { genie::ResourceType::WoodStorage, 100 },
        { genie::ResourceType::StoneStorage, 100 }
    };
};

