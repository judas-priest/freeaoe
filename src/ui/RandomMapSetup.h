#pragma once

#include "render/IRenderTarget.h"
#include "render/EventTypes.h"
#include "core/Types.h"
#include "mechanics/RandomMapGenerator.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>

#ifdef USE_SDL2
struct SdlWindow;
#endif

// Screen for configuring and starting a random map game
class RandomMapSetup
{
public:
    struct Result {
        bool start = false;
        int mapType = 0;
        int mapSize = 144;
        int playerCount = 2;
        int startingAge = 0;   // 0=Dark, 1=Feudal, 2=Castle, 3=Imperial
        int civIds[8] = {};    // 0 = random, 1+ = specific civ index
        int teams[8] = {};     // 0 = no team, 1-4 = team number
    };

    // Show setup screen, returns configuration when user taps Start
    static Result show(SdlWindow *window, const std::shared_ptr<IRenderTarget> &renderTarget);

private:
    RandomMapSetup() = default;

    void render();
    void handleEvent(const input::Event &event);

    SdlWindow *m_window = nullptr;
    std::shared_ptr<IRenderTarget> m_renderTarget;
    bool m_done = false;
    bool m_cancelled = false;

    int m_mapType = 0;
    int m_mapSize = 2; // index into sizes array
    int m_playerCount = 2;
    int m_startingAge = 0;
    int m_civIds[8] = {};
    int m_teams[8] = {};
    std::vector<std::string> m_civNames;

    Drawable::Text::Ptr m_titleText;
    Drawable::Text::Ptr m_labelText;

    static constexpr int MAP_TYPE_COUNT = 15;
    static constexpr const char *MAP_TYPE_NAMES[] = {
        "Arabia", "Black Forest", "Islands", "Arena", "Nomad",
        "Coastal", "Rivers", "Baltic", "Mediterranean", "Highland",
        "Gold Rush", "Fortress", "Oasis", "Mega Random", "Team Islands"
    };
    static constexpr int MAP_SIZE_COUNT = 5;
    static constexpr int MAP_SIZES[] = {72, 100, 144, 200, 220};
    static constexpr const char *MAP_SIZE_NAMES[] = {"Tiny", "Small", "Medium", "Large", "Giant"};
    static constexpr int AGE_COUNT = 4;
    static constexpr const char *AGE_NAMES[] = {"Dark Age", "Feudal Age", "Castle Age", "Imperial Age"};
    static constexpr int MAX_TEAMS = 4;
};
