#pragma once

#include <memory>
#include <vector>
#include <string>

class Map;
class Player;
class UnitManager;

// Generates random playable maps with terrain, resources, and starting positions
struct RandomMapGenerator
{
    enum MapType {
        Arabia,      // Open, some forest patches
        BlackForest, // Dense forest, narrow paths
        Islands,     // Water with land masses
        Arena,       // Walled start
        Nomad        // No TC, scattered villagers
    };

    enum MapSize {
        Tiny = 72,
        Small = 100,
        Medium = 144,
        Normal = 200,
        Large = 220,
        Giant = 255
    };

    struct Settings {
        MapType type = Arabia;
        int size = Medium;
        int playerCount = 2;
    };

    // Generate a random map and place starting units for each player
    static bool generate(const Settings &settings,
                         const std::shared_ptr<Map> &map,
                         UnitManager &unitManager,
                         const std::vector<std::shared_ptr<Player>> &players);

private:
    static void generateTerrain(const Settings &settings, const std::shared_ptr<Map> &map);
    static void placeForests(const Settings &settings, const std::shared_ptr<Map> &map);
    static void placeResources(const Settings &settings, const std::shared_ptr<Map> &map,
                               UnitManager &unitManager, const std::vector<std::shared_ptr<Player>> &players);
    static void placeStartingUnits(const Settings &settings,
                                   UnitManager &unitManager,
                                   const std::vector<std::shared_ptr<Player>> &players,
                                   const std::shared_ptr<Map> &map);

    // Perlin-like noise for terrain variation
    static float noise(float x, float y);
};
