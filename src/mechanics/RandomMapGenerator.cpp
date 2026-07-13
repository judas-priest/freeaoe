#include "RandomMapGenerator.h"

#include "Map.h"
#include "Player.h"
#include "Unit.h"
#include "UnitManager.h"
#include "UnitFactory.h"
#include "core/Constants.h"
#include "core/Logger.h"

#include <cmath>
#include <cstdlib>
#include <algorithm>

// Simple hash-based noise
float RandomMapGenerator::noise(float x, float y)
{
    int xi = static_cast<int>(x * 1000);
    int yi = static_cast<int>(y * 1000);
    int n = xi + yi * 57;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

bool RandomMapGenerator::generate(const Settings &settings,
                                   const std::shared_ptr<Map> &map,
                                   UnitManager &unitManager,
                                   const std::vector<std::shared_ptr<Player>> &players)
{
    if (!map) return false;

    DBG << "Generating random map type=" << settings.type << "size=" << settings.size;

    // Initialize map
    map->setupBasic(settings.size);

    // Generate terrain
    generateTerrain(settings, map);

    // Place forests
    placeForests(settings, map);

    // Place starting units (TC, villagers, scout)
    placeStartingUnits(settings, unitManager, players, map);

    // Place resources (gold, stone, berries, deer, boar, sheep)
    placeResources(settings, map, unitManager, players);

    return true;
}

void RandomMapGenerator::generateTerrain(const Settings &settings, const std::shared_ptr<Map> &map)
{
    const int size = settings.size;

    for (int col = 0; col < size; col++) {
        for (int row = 0; row < size; row++) {
            MapTile &tile = map->getTileAt(col, row);

            // Base terrain: grass
            tile.terrainId = 0; // Grass1

            // Noise-based terrain variation
            float n = noise(col * 0.05f, row * 0.05f);

            switch (settings.type) {
            case Arabia:
                // Mostly grass with some dirt patches
                if (n > 0.6f) tile.terrainId = 6;  // Dirt1
                if (n > 0.8f) tile.terrainId = 11; // Dirt3
                break;

            case BlackForest:
                // Grass base, forests added later
                if (n > 0.7f) tile.terrainId = 5; // Leaves
                break;

            case Islands: {
                // Water with islands
                float cx = (col - size / 2.f) / (size / 2.f);
                float cy = (row - size / 2.f) / (size / 2.f);
                float distFromCenter = std::sqrt(cx * cx + cy * cy);
                float waterNoise = noise(col * 0.08f, row * 0.08f) * 0.3f;

                if (distFromCenter + waterNoise > 0.6f) {
                    tile.terrainId = 1; // Water (shallow)
                } else if (distFromCenter + waterNoise > 0.5f) {
                    tile.terrainId = 2; // Beach
                }
                break;
            }

            case Arena:
                // Grass with road ring
                if (n > 0.7f) tile.terrainId = 6; // Dirt
                break;

            case Nomad:
                // Desert-ish
                if (n > 0.3f) tile.terrainId = 14; // Desert
                if (n > 0.7f) tile.terrainId = 6;  // Dirt
                break;
            }

            // Elevation from noise
            float elevNoise = noise(col * 0.03f + 100, row * 0.03f + 100);
            tile.elevation = std::clamp(static_cast<int>(elevNoise * 3 + 2), 0, 7);
        }
    }
}

void RandomMapGenerator::placeForests(const Settings &settings, const std::shared_ptr<Map> &map)
{
    const int size = settings.size;
    float forestDensity = 0.0f;

    switch (settings.type) {
    case Arabia: forestDensity = 0.08f; break;
    case BlackForest: forestDensity = 0.40f; break;
    case Islands: forestDensity = 0.06f; break;
    case Arena: forestDensity = 0.10f; break;
    case Nomad: forestDensity = 0.03f; break;
    }

    // Place forest patches using noise
    for (int col = 0; col < size; col++) {
        for (int row = 0; row < size; row++) {
            MapTile &tile = map->getTileAt(col, row);
            if (tile.terrainId == 1 || tile.terrainId == 2) continue; // skip water/beach

            float n = noise(col * 0.1f + 50, row * 0.1f + 50);
            if (n > 1.0f - forestDensity * 2) {
                tile.terrainId = 10; // Forest (oak)
            }
        }
    }
}

void RandomMapGenerator::placeStartingUnits(const Settings &settings,
                                             UnitManager &unitManager,
                                             const std::vector<std::shared_ptr<Player>> &players,
                                             const std::shared_ptr<Map> &map)
{
    const int size = settings.size;
    const int playerCount = std::min(static_cast<int>(players.size()), settings.playerCount + 1); // +1 for gaia

    // Calculate starting positions in a circle
    float centerX = size / 2.f;
    float centerY = size / 2.f;
    float radius = size * 0.35f;

    for (int i = 1; i < playerCount; i++) { // skip gaia (0)
        if (i >= static_cast<int>(players.size())) break;
        const auto &player = players[i];
        if (!player) continue;

        float angle = (i - 1) * 2.f * M_PI / (playerCount - 1);
        float startX = centerX + radius * std::cos(angle);
        float startY = centerY + radius * std::sin(angle);

        // Clamp to map
        startX = std::clamp(startX, 10.f, static_cast<float>(size - 10));
        startY = std::clamp(startY, 10.f, static_cast<float>(size - 10));

        MapPos basePos(startX * Constants::TILE_SIZE, startY * Constants::TILE_SIZE);

        // Clear forest around starting position
        for (int dx = -5; dx <= 5; dx++) {
            for (int dy = -5; dy <= 5; dy++) {
                int cx = static_cast<int>(startX) + dx;
                int cy = static_cast<int>(startY) + dy;
                if (cx >= 0 && cx < size && cy >= 0 && cy < size) {
                    MapTile &tile = map->getTileAt(cx, cy);
                    if (tile.terrainId == 10) tile.terrainId = 0; // Remove forest
                    if (tile.terrainId == 1 || tile.terrainId == 2) tile.terrainId = 0; // Remove water
                }
            }
        }

        if (settings.type != Nomad) {
            // Town Center (ID 109)
            Unit::Ptr tc = UnitFactory::createUnit(109, player, unitManager);
            if (tc) {
                unitManager.add(tc, basePos);
                DBG << "Placed TC for player" << player->playerId << "at" << basePos.x << basePos.y;
            } else {
                WARN << "Failed to create TC (ID 109) for player" << player->playerId;
            }
        }

        // 3 Villagers (ID 83 = male villager)
        for (int v = 0; v < 3; v++) {
            float vx = basePos.x + (v - 1) * Constants::TILE_SIZE;
            float vy = basePos.y + Constants::TILE_SIZE * 2;
            Unit::Ptr villager = UnitFactory::createUnit(83, player, unitManager);
            if (villager) {
                unitManager.add(villager, MapPos(vx, vy));
            }
        }

        // Scout (ID 448 = scout cavalry)
        Unit::Ptr scout = UnitFactory::createUnit(448, player, unitManager);
        if (scout) {
            unitManager.add(scout, MapPos(basePos.x + Constants::TILE_SIZE * 3, basePos.y));
        }
    }
}

void RandomMapGenerator::placeResources(const Settings &settings,
                                         const std::shared_ptr<Map> &map,
                                         UnitManager &unitManager,
                                         const std::vector<std::shared_ptr<Player>> &players)
{
    const int size = settings.size;
    if (players.empty()) return;
    const auto &gaia = players[0]; // Resources belong to Gaia

    // Place gold mines (ID 66)
    int goldPatchCount = size / 20;
    for (int i = 0; i < goldPatchCount; i++) {
        int gx = 10 + rand() % (size - 20);
        int gy = 10 + rand() % (size - 20);
        MapPos goldPos(gx * Constants::TILE_SIZE, gy * Constants::TILE_SIZE);

        for (int j = 0; j < 4; j++) {
            Unit::Ptr gold = UnitFactory::createUnit(66, gaia, unitManager);
            if (gold) {
                float ox = (j % 2) * Constants::TILE_SIZE;
                float oy = (j / 2) * Constants::TILE_SIZE;
                unitManager.add(gold, MapPos(goldPos.x + ox, goldPos.y + oy));
            }
        }
    }

    // Place stone mines (ID 102)
    int stonePatchCount = size / 25;
    for (int i = 0; i < stonePatchCount; i++) {
        int sx = 10 + rand() % (size - 20);
        int sy = 10 + rand() % (size - 20);
        MapPos stonePos(sx * Constants::TILE_SIZE, sy * Constants::TILE_SIZE);

        for (int j = 0; j < 3; j++) {
            Unit::Ptr stone = UnitFactory::createUnit(102, gaia, unitManager);
            if (stone) {
                float ox = j * Constants::TILE_SIZE;
                unitManager.add(stone, MapPos(stonePos.x + ox, stonePos.y));
            }
        }
    }

    // Place berry bushes (ID 59) — near starting positions
    int berryPatchCount = size / 30;
    for (int i = 0; i < berryPatchCount; i++) {
        int bx = 10 + rand() % (size - 20);
        int by = 10 + rand() % (size - 20);
        MapPos berryPos(bx * Constants::TILE_SIZE, by * Constants::TILE_SIZE);

        for (int j = 0; j < 6; j++) {
            Unit::Ptr berry = UnitFactory::createUnit(59, gaia, unitManager);
            if (berry) {
                float ox = (j % 3) * Constants::TILE_SIZE * 0.7f;
                float oy = (j / 3) * Constants::TILE_SIZE * 0.7f;
                unitManager.add(berry, MapPos(berryPos.x + ox, berryPos.y + oy));
            }
        }
    }

    // Place deer (ID 65)
    int deerCount = size / 15;
    for (int i = 0; i < deerCount; i++) {
        int dx = 10 + rand() % (size - 20);
        int dy = 10 + rand() % (size - 20);
        Unit::Ptr deer = UnitFactory::createUnit(65, gaia, unitManager);
        if (deer) {
            unitManager.add(deer, MapPos(dx * Constants::TILE_SIZE, dy * Constants::TILE_SIZE));
        }
    }

    // Place sheep (ID 594) near starting positions
    for (size_t p = 1; p < players.size() && p <= static_cast<size_t>(settings.playerCount); p++) {
        float angle = (p - 1) * 2.f * M_PI / settings.playerCount;
        float px = size / 2.f + size * 0.35f * std::cos(angle);
        float py = size / 2.f + size * 0.35f * std::sin(angle);

        for (int s = 0; s < 4; s++) {
            float sx = (px + (rand() % 10 - 5)) * Constants::TILE_SIZE;
            float sy = (py + (rand() % 10 - 5)) * Constants::TILE_SIZE;
            Unit::Ptr sheep = UnitFactory::createUnit(594, gaia, unitManager);
            if (sheep) {
                unitManager.add(sheep, MapPos(sx, sy));
            }
        }
    }

    // Place trees (ID 349 = oak tree) outside forests for extra wood
    int treeCount = size * 2;
    for (int i = 0; i < treeCount; i++) {
        int tx = 5 + rand() % (size - 10);
        int ty = 5 + rand() % (size - 10);
        MapTile &tile = map->getTileAt(tx, ty);
        if (tile.terrainId == 1 || tile.terrainId == 2) continue; // skip water

        Unit::Ptr tree = UnitFactory::createUnit(349, gaia, unitManager);
        if (tree) {
            unitManager.add(tree, MapPos(tx * Constants::TILE_SIZE, ty * Constants::TILE_SIZE));
        }
    }
}
