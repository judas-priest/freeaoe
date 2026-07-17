#pragma once

#include "core/Types.h"

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
        Arabia,        // Open, some forest patches
        BlackForest,   // Dense forest, narrow paths
        Islands,       // Water with land masses
        Arena,         // Walled start
        Nomad,         // No TC, scattered villagers
        Coastal,       // Land with water on one side
        Rivers,        // Two landmasses separated by a river
        Baltic,        // Large central lake, land around edges
        Mediterranean, // Horizontal water strip between continents
        Highland,      // High elevation, sparse forests, extra gold/stone
        GoldRush,      // Arabia-like with large gold pile at center
        Fortress,      // Arabia with stone walls around each base
        Oasis,         // Desert with forest/water circle in center
        MegaRandom,    // Randomly picks another map type
        TeamIslands,   // Teams share islands
        MapTypeCount   // Sentinel — must be last
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
                               UnitManager &unitManager, const std::vector<std::shared_ptr<Player>> &players,
                               const std::vector<MapPos> &startPositions);
    static std::vector<MapPos> placeStartingUnits(const Settings &settings,
                                                   UnitManager &unitManager,
                                                   const std::vector<std::shared_ptr<Player>> &players,
                                                   const std::shared_ptr<Map> &map);

    // Per-type terrain generators (called from generateTerrain/placeForests)
    static void generateCoastal(const Settings &settings, const std::shared_ptr<Map> &map);
    static void generateRivers(const Settings &settings, const std::shared_ptr<Map> &map);
    static void generateBaltic(const Settings &settings, const std::shared_ptr<Map> &map);
    static void generateMediterranean(const Settings &settings, const std::shared_ptr<Map> &map);
    static void generateHighland(const Settings &settings, const std::shared_ptr<Map> &map);
    static void generateOasis(const Settings &settings, const std::shared_ptr<Map> &map);
    static void generateTeamIslands(const Settings &settings, const std::shared_ptr<Map> &map);

    // Place fish on water tiles
    static void placeFish(const std::shared_ptr<Map> &map, int size,
                          UnitManager &unitManager, const std::shared_ptr<Player> &gaia);

    // Place stone walls around a position
    static void placeWallsAround(float centerX, float centerY, int radius,
                                  const std::shared_ptr<Player> &player,
                                  UnitManager &unitManager, const std::shared_ptr<Map> &map);

    // Perlin-like noise for terrain variation
    static float noise(float x, float y);
};
