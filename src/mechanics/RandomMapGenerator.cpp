#include "RandomMapGenerator.h"

#include "Map.h"
#include "Player.h"
#include "Unit.h"
#include "UnitManager.h"
#include "UnitFactory.h"
#include "core/Constants.h"
#include "core/Logger.h"
#include "net/SyncRandom.h"

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

    // Place starting units (TC, villagers, scout) — returns start positions
    std::vector<MapPos> startPositions = placeStartingUnits(settings, unitManager, players, map);

    // Place resources (gold, stone, berries, deer, boar, sheep, wolves, relics, fish)
    placeResources(settings, map, unitManager, players, startPositions);

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

            // Elevation — flat (slopes cause black tile artifacts in filtermap rendering)
            tile.elevation = 2;
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

std::vector<MapPos> RandomMapGenerator::placeStartingUnits(const Settings &settings,
                                             UnitManager &unitManager,
                                             const std::vector<std::shared_ptr<Player>> &players,
                                             const std::shared_ptr<Map> &map)
{
    std::vector<MapPos> startPositions;
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
        startPositions.push_back(basePos);

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

    return startPositions;
}

void RandomMapGenerator::placeResources(const Settings &settings,
                                         const std::shared_ptr<Map> &map,
                                         UnitManager &unitManager,
                                         const std::vector<std::shared_ptr<Player>> &players,
                                         const std::vector<MapPos> &startPositions)
{
    const int size = settings.size;
    if (players.empty()) return;
    const auto &gaia = players[0]; // Resources belong to Gaia

    // Helper: check if tile at (col,row) is passable land (not water/beach/forest)
    auto isLandTile = [&](int col, int row) -> bool {
        if (col < 0 || col >= size || row < 0 || row >= size) return false;
        const MapTile &tile = map->getTileAt(col, row);
        return tile.terrainId != 1 && tile.terrainId != 2 && tile.terrainId != 10;
    };

    // Helper: minimum distance from a position to any player start (in pixels)
    auto minDistToPlayers = [&](float px, float py) -> float {
        float minDist = 1e9f;
        for (const auto &sp : startPositions) {
            float dx = px - sp.x;
            float dy = py - sp.y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < minDist) minDist = d;
        }
        return minDist;
    };

    // Helper: place a cluster of units at an offset from a center position
    auto placeCluster = [&](int unitId, int count, float centerX, float centerY, float spreadTiles) {
        for (int j = 0; j < count; j++) {
            float ox = (SyncRandom::inst().nextInt(static_cast<int>(spreadTiles * 2 + 1)) - spreadTiles) * Constants::TILE_SIZE;
            float oy = (SyncRandom::inst().nextInt(static_cast<int>(spreadTiles * 2 + 1)) - spreadTiles) * Constants::TILE_SIZE;
            float ux = centerX + ox;
            float uy = centerY + oy;
            int col = static_cast<int>(ux / Constants::TILE_SIZE);
            int row = static_cast<int>(uy / Constants::TILE_SIZE);
            if (!isLandTile(col, row)) continue;
            Unit::Ptr unit = UnitFactory::createUnit(unitId, gaia, unitManager);
            if (unit) {
                unitManager.add(unit, MapPos(ux, uy));
            }
        }
    };

    // ── Step 2: Per-player guaranteed resources ──
    for (const auto &sp : startPositions) {
        float baseTileX = sp.x / Constants::TILE_SIZE;
        float baseTileY = sp.y / Constants::TILE_SIZE;

        // Helper: pick a random position at [minDist, maxDist] tiles from base
        auto pickOffset = [&](float minDist, float maxDist) -> std::pair<float, float> {
            float angle = SyncRandom::inst().nextFloat() * 2.f * M_PI;
            float dist = minDist + SyncRandom::inst().nextInt(static_cast<int>(maxDist - minDist + 1));
            float tx = baseTileX + dist * std::cos(angle);
            float ty = baseTileY + dist * std::sin(angle);
            tx = std::clamp(tx, 5.f, static_cast<float>(size - 5));
            ty = std::clamp(ty, 5.f, static_cast<float>(size - 5));
            return {tx * Constants::TILE_SIZE, ty * Constants::TILE_SIZE};
        };

        // 2 Boar (ID 48) at 10-15 tiles
        for (int b = 0; b < 2; b++) {
            auto [bx, by] = pickOffset(10, 15);
            int col = static_cast<int>(bx / Constants::TILE_SIZE);
            int row = static_cast<int>(by / Constants::TILE_SIZE);
            if (!isLandTile(col, row)) continue;
            Unit::Ptr boar = UnitFactory::createUnit(48, gaia, unitManager);
            if (boar) unitManager.add(boar, MapPos(bx, by));
        }

        // 1 Berry patch (6 bushes, ID 59) at 6-8 tiles
        {
            auto [bx, by] = pickOffset(6, 8);
            placeCluster(59, 6, bx, by, 1.5f);
        }

        // 1 Main gold patch (7 mines, ID 66) at 8-12 tiles
        {
            auto [gx, gy] = pickOffset(8, 12);
            placeCluster(66, 7, gx, gy, 1.5f);
        }

        // 1 Secondary gold patch (4 mines, ID 66) at 18-22 tiles
        {
            auto [gx, gy] = pickOffset(18, 22);
            placeCluster(66, 4, gx, gy, 1.0f);
        }

        // 1 Main stone patch (5 mines, ID 102) at 10-14 tiles
        {
            auto [sx, sy] = pickOffset(10, 14);
            placeCluster(102, 5, sx, sy, 1.5f);
        }

        // 1 Secondary stone patch (4 mines, ID 102) at 20-24 tiles
        {
            auto [sx, sy] = pickOffset(20, 24);
            placeCluster(102, 4, sx, sy, 1.0f);
        }

        // 4 Extra sheep (ID 594) at 8-12 tiles
        for (int s = 0; s < 4; s++) {
            auto [sx, sy] = pickOffset(8, 12);
            int col = static_cast<int>(sx / Constants::TILE_SIZE);
            int row = static_cast<int>(sy / Constants::TILE_SIZE);
            if (!isLandTile(col, row)) continue;
            Unit::Ptr sheep = UnitFactory::createUnit(594, gaia, unitManager);
            if (sheep) unitManager.add(sheep, MapPos(sx, sy));
        }

        // 3 Deer (ID 65) at 12-16 tiles
        {
            auto [dx, dy] = pickOffset(12, 16);
            placeCluster(65, 3, dx, dy, 2.0f);
        }
    }

    // ── Step 3: Wolves (global, away from players) ──
    int wolfCount = size / 15;
    for (int i = 0; i < wolfCount; i++) {
        for (int attempt = 0; attempt < 10; attempt++) {
            int wx = 10 + SyncRandom::inst().nextInt(size - 20);
            int wy = 10 + SyncRandom::inst().nextInt(size - 20);
            if (!isLandTile(wx, wy)) continue;
            float px = wx * Constants::TILE_SIZE;
            float py = wy * Constants::TILE_SIZE;
            if (minDistToPlayers(px, py) < 20 * Constants::TILE_SIZE) continue;
            Unit::Ptr wolf = UnitFactory::createUnit(126, gaia, unitManager);
            if (wolf) unitManager.add(wolf, MapPos(px, py));
            break;
        }
    }

    // ── Step 4: Fish on Islands maps ──
    if (settings.type == Islands) {
        int fishCount = size * size / 200;
        for (int i = 0; i < fishCount; i++) {
            for (int attempt = 0; attempt < 10; attempt++) {
                int fx = 5 + SyncRandom::inst().nextInt(size - 10);
                int fy = 5 + SyncRandom::inst().nextInt(size - 10);
                if (fx < 0 || fx >= size || fy < 0 || fy >= size) continue;
                const MapTile &tile = map->getTileAt(fx, fy);
                if (tile.terrainId != 1) continue; // must be water
                Unit::Ptr fish = UnitFactory::createUnit(69, gaia, unitManager);
                if (fish) unitManager.add(fish, MapPos(fx * Constants::TILE_SIZE, fy * Constants::TILE_SIZE));
                break;
            }
        }
    }

    // ── Step 5: Relics (5, scattered, away from players) ──
    for (int i = 0; i < 5; i++) {
        for (int attempt = 0; attempt < 20; attempt++) {
            int rx = 10 + SyncRandom::inst().nextInt(size - 20);
            int ry = 10 + SyncRandom::inst().nextInt(size - 20);
            if (!isLandTile(rx, ry)) continue;
            float px = rx * Constants::TILE_SIZE;
            float py = ry * Constants::TILE_SIZE;
            if (minDistToPlayers(px, py) < 8 * Constants::TILE_SIZE) continue;
            Unit::Ptr relic = UnitFactory::createUnit(285, gaia, unitManager);
            if (relic) unitManager.add(relic, MapPos(px, py));
            break;
        }
    }

    // ── Step 6: Reduced global scatter (supplementary) ──

    // Extra gold patches (reduced from size/20)
    int goldPatchCount = size / 40;
    for (int i = 0; i < goldPatchCount; i++) {
        int gx = 10 + SyncRandom::inst().nextInt(size - 20);
        int gy = 10 + SyncRandom::inst().nextInt(size - 20);
        if (!isLandTile(gx, gy)) continue;
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

    // Extra stone patches (reduced from size/25)
    int stonePatchCount = size / 50;
    for (int i = 0; i < stonePatchCount; i++) {
        int sx = 10 + SyncRandom::inst().nextInt(size - 20);
        int sy = 10 + SyncRandom::inst().nextInt(size - 20);
        if (!isLandTile(sx, sy)) continue;
        MapPos stonePos(sx * Constants::TILE_SIZE, sy * Constants::TILE_SIZE);
        for (int j = 0; j < 3; j++) {
            Unit::Ptr stone = UnitFactory::createUnit(102, gaia, unitManager);
            if (stone) {
                float ox = j * Constants::TILE_SIZE;
                unitManager.add(stone, MapPos(stonePos.x + ox, stonePos.y));
            }
        }
    }

    // Place trees (ID 349 = oak tree) outside forests for extra wood
    int treeCount = size * 2;
    for (int i = 0; i < treeCount; i++) {
        int tx = 5 + SyncRandom::inst().nextInt(size - 10);
        int ty = 5 + SyncRandom::inst().nextInt(size - 10);
        MapTile &tile = map->getTileAt(tx, ty);
        if (tile.terrainId == 1 || tile.terrainId == 2) continue; // skip water

        Unit::Ptr tree = UnitFactory::createUnit(349, gaia, unitManager);
        if (tree) {
            unitManager.add(tree, MapPos(tx * Constants::TILE_SIZE, ty * Constants::TILE_SIZE));
        }
    }
}
