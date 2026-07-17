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

    // MegaRandom: pick a random type (excluding MegaRandom itself)
    Settings effectiveSettings = settings;
    if (settings.type == MegaRandom) {
        int pick = SyncRandom::inst().nextInt(static_cast<int>(MegaRandom));
        effectiveSettings.type = static_cast<MapType>(pick);
        DBG << "MegaRandom picked type=" << effectiveSettings.type;
    }

    DBG << "Generating random map type=" << effectiveSettings.type << "size=" << effectiveSettings.size;

    // Initialize map
    map->setupBasic(effectiveSettings.size);

    // Generate terrain
    generateTerrain(effectiveSettings, map);

    // Place forests
    placeForests(effectiveSettings, map);

    // Place starting units (TC, villagers, scout) — returns start positions
    std::vector<MapPos> startPositions = placeStartingUnits(effectiveSettings, unitManager, players, map);

    // Place resources (gold, stone, berries, deer, boar, sheep, wolves, relics, fish)
    placeResources(effectiveSettings, map, unitManager, players, startPositions);

    // Fortress: place stone walls around each player start
    if (effectiveSettings.type == Fortress) {
        const int playerCount = std::min(static_cast<int>(players.size()), effectiveSettings.playerCount + 1);
        for (int i = 1; i < playerCount; i++) {
            if (i >= static_cast<int>(players.size()) || !players[i]) continue;
            if (i - 1 >= static_cast<int>(startPositions.size())) continue;
            placeWallsAround(startPositions[i - 1].x, startPositions[i - 1].y,
                             7, players[i], unitManager, map);
        }
    }

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
            case GoldRush:
            case Fortress:
                // Mostly grass with some dirt patches
                if (n > 0.6f) tile.terrainId = 6;  // Dirt1
                if (n > 0.8f) tile.terrainId = 11; // Dirt3
                // Occasional desert patches
                {
                    float desertNoise = noise(col * 0.03f + 100.f, row * 0.03f + 100.f);
                    if (desertNoise > 0.75f && tile.terrainId == 0) {
                        tile.terrainId = 14; // Desert
                    }
                }
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

            case Highland:
                // Grass base with more dirt
                if (n > 0.4f) tile.terrainId = 6;  // Dirt1
                if (n > 0.7f) tile.terrainId = 0;  // Grass (variety)
                break;

            // These types use dedicated generation after the base loop
            case Coastal:
            case Rivers:
            case Baltic:
            case Mediterranean:
            case Oasis:
            case TeamIslands:
            case MegaRandom:
            case MapTypeCount:
                break;
            }

            // Gentle hills using low-frequency noise — values 1-3
            float elevNoise = noise(col * 0.05f, row * 0.05f);
            int elevation = 2; // Default flat
            if (settings.type == Highland) {
                // Higher baseline for Highland
                elevation = 4;
                if (elevNoise > 0.5f) elevation = 5;
                else if (elevNoise < 0.2f) elevation = 3;
            } else {
                if (elevNoise > 0.6f) elevation = 3;
                else if (elevNoise < 0.3f) elevation = 1;
            }

            // Water and beach stay at water level
            if (tile.terrainId == 1 || tile.terrainId == 2) elevation = 0;

            tile.elevation = std::clamp(elevation, 0, 7);
        }
    }

    // Post-loop: dedicated terrain generators for complex map types
    switch (settings.type) {
    case Coastal:       generateCoastal(settings, map); break;
    case Rivers:        generateRivers(settings, map); break;
    case Baltic:        generateBaltic(settings, map); break;
    case Mediterranean: generateMediterranean(settings, map); break;
    case Oasis:         generateOasis(settings, map); break;
    case TeamIslands:   generateTeamIslands(settings, map); break;
    default: break;
    }
}

void RandomMapGenerator::placeForests(const Settings &settings, const std::shared_ptr<Map> &map)
{
    const int size = settings.size;
    float forestDensity = 0.0f;

    switch (settings.type) {
    case Arabia:        forestDensity = 0.08f; break;
    case BlackForest:   forestDensity = 0.40f; break;
    case Islands:       forestDensity = 0.06f; break;
    case Arena:         forestDensity = 0.10f; break;
    case Nomad:         forestDensity = 0.03f; break;
    case Coastal:       forestDensity = 0.08f; break;
    case Rivers:        forestDensity = 0.08f; break;
    case Baltic:        forestDensity = 0.07f; break;
    case Mediterranean: forestDensity = 0.07f; break;
    case Highland:      forestDensity = 0.04f; break;  // Sparse
    case GoldRush:      forestDensity = 0.08f; break;
    case Fortress:      forestDensity = 0.08f; break;
    case Oasis:         forestDensity = 0.0f;  break;  // Handled by generateOasis
    case TeamIslands:   forestDensity = 0.06f; break;
    case MegaRandom:    forestDensity = 0.08f; break;
    case MapTypeCount:  forestDensity = 0.08f; break;
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

    // Calculate starting positions
    float centerX = size / 2.f;
    float centerY = size / 2.f;
    float radius = size * 0.35f;

    // For Coastal, shrink radius and shift center left so players stay on land
    if (settings.type == Coastal) {
        centerX = size * 0.33f;
        radius = size * 0.25f;
    }

    for (int i = 1; i < playerCount; i++) { // skip gaia (0)
        if (i >= static_cast<int>(players.size())) break;
        const auto &player = players[i];
        if (!player) continue;

        float startX, startY;
        int humanPlayers = playerCount - 1;

        if (settings.type == Mediterranean) {
            // Split north/south: first half north, second half south
            int halfIdx = (i - 1);
            bool isNorth = halfIdx < (humanPlayers + 1) / 2;
            int sideIdx = isNorth ? halfIdx : halfIdx - (humanPlayers + 1) / 2;
            int sideCount = isNorth ? (humanPlayers + 1) / 2 : humanPlayers - (humanPlayers + 1) / 2;
            if (sideCount < 1) sideCount = 1;
            startX = size * 0.2f + (sideIdx + 0.5f) * (size * 0.6f) / sideCount;
            startY = isNorth ? size * 0.2f : size * 0.8f;
        } else if (settings.type == Rivers) {
            // Split left/right of river
            bool isLeft = (i - 1) < (humanPlayers + 1) / 2;
            int sideIdx = isLeft ? (i - 1) : (i - 1) - (humanPlayers + 1) / 2;
            int sideCount = isLeft ? (humanPlayers + 1) / 2 : humanPlayers - (humanPlayers + 1) / 2;
            if (sideCount < 1) sideCount = 1;
            startX = isLeft ? size * 0.2f : size * 0.8f;
            startY = size * 0.15f + (sideIdx + 0.5f) * (size * 0.7f) / sideCount;
        } else if (settings.type == TeamIslands) {
            // Team 1 on left island, team 2 on right island
            // Even-indexed players go left, odd go right (simple split)
            bool isLeft = ((i - 1) % 2 == 0);
            float islandCX = isLeft ? size * 0.25f : size * 0.75f;
            float islandCY = size / 2.f;
            float angle = (i - 1) * 1.5f;
            float r = size * 0.08f;
            startX = islandCX + r * std::cos(angle);
            startY = islandCY + r * std::sin(angle);
        } else {
            // Default: circle placement
            float angle = (i - 1) * 2.f * M_PI / humanPlayers;
            startX = centerX + radius * std::cos(angle);
            startY = centerY + radius * std::sin(angle);
        }

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

    // ── Step 4: Fish on water maps ──
    if (settings.type == Islands || settings.type == Coastal || settings.type == Rivers ||
        settings.type == Baltic || settings.type == Mediterranean || settings.type == TeamIslands) {
        placeFish(map, size, unitManager, gaia);
    }

    // ── Step 4b: GoldRush — large gold pile at map center ──
    if (settings.type == GoldRush) {
        float centerPx = (size / 2.f) * Constants::TILE_SIZE;
        placeCluster(66, 12, centerPx, centerPx, 2.0f);
    }

    // ── Step 4c: Highland — 50% more gold and stone ──
    if (settings.type == Highland) {
        for (const auto &sp : startPositions) {
            float baseTileX = sp.x / Constants::TILE_SIZE;
            float baseTileY = sp.y / Constants::TILE_SIZE;
            auto pickOffset = [&](float minDist, float maxDist) -> std::pair<float, float> {
                float angle = SyncRandom::inst().nextFloat() * 2.f * M_PI;
                float dist = minDist + SyncRandom::inst().nextInt(static_cast<int>(maxDist - minDist + 1));
                float tx = baseTileX + dist * std::cos(angle);
                float ty = baseTileY + dist * std::sin(angle);
                tx = std::clamp(tx, 5.f, static_cast<float>(size - 5));
                ty = std::clamp(ty, 5.f, static_cast<float>(size - 5));
                return {tx * Constants::TILE_SIZE, ty * Constants::TILE_SIZE};
            };
            // Extra gold patch
            {
                auto [gx, gy] = pickOffset(14, 18);
                placeCluster(66, 5, gx, gy, 1.5f);
            }
            // Extra stone patch
            {
                auto [sx, sy] = pickOffset(16, 20);
                placeCluster(102, 4, sx, sy, 1.5f);
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

// ═══════════════════════════════════════════════════════════════════════
// Per-type terrain generators
// ═══════════════════════════════════════════════════════════════════════

void RandomMapGenerator::generateCoastal(const Settings &settings, const std::shared_ptr<Map> &map)
{
    const int size = settings.size;
    // Water on the right ~30% with noisy edge
    for (int col = 0; col < size; col++) {
        for (int row = 0; row < size; row++) {
            MapTile &tile = map->getTileAt(col, row);
            float normalizedCol = static_cast<float>(col) / size;
            float edgeNoise = noise(col * 0.06f + 200.f, row * 0.06f + 200.f) * 0.12f;
            float waterThreshold = 0.68f + edgeNoise;

            if (normalizedCol > waterThreshold + 0.03f) {
                tile.terrainId = 1; // Water
                tile.elevation = 0;
            } else if (normalizedCol > waterThreshold) {
                tile.terrainId = 2; // Beach
                tile.elevation = 0;
            } else {
                // Land — grass with some dirt
                float n = noise(col * 0.05f, row * 0.05f);
                tile.terrainId = 0; // Grass
                if (n > 0.6f) tile.terrainId = 6; // Dirt
            }
        }
    }
}

void RandomMapGenerator::generateRivers(const Settings &settings, const std::shared_ptr<Map> &map)
{
    const int size = settings.size;

    // First fill with grass + dirt variation
    for (int col = 0; col < size; col++) {
        for (int row = 0; row < size; row++) {
            MapTile &tile = map->getTileAt(col, row);
            float n = noise(col * 0.05f, row * 0.05f);
            tile.terrainId = 0; // Grass
            if (n > 0.65f) tile.terrainId = 6; // Dirt
        }
    }

    // Draw a meandering river from top to bottom through the center
    float riverCenter = size / 2.f;
    float riverWidth = 3.0f;
    // Pre-compute ford positions (2-3 fords)
    int fordCount = 2 + (size > 150 ? 1 : 0);
    std::vector<int> fordRows;
    for (int f = 0; f < fordCount; f++) {
        fordRows.push_back(static_cast<int>(size * (f + 1.f) / (fordCount + 1.f)));
    }

    for (int row = 0; row < size; row++) {
        // Sinusoidal meander
        float meander = std::sin(row * 0.04f) * (size * 0.12f) + noise(0.f, row * 0.03f + 300.f) * (size * 0.05f);
        float center = riverCenter + meander;

        // Check if this row is near a ford
        bool isFord = false;
        for (int fr : fordRows) {
            if (std::abs(row - fr) <= 2) { isFord = true; break; }
        }

        for (int col = 0; col < size; col++) {
            float dist = std::abs(col - center);
            MapTile &tile = map->getTileAt(col, row);

            if (isFord) {
                // Shallows at fords (terrain 4 = shallows, fallback to beach)
                if (dist < riverWidth) {
                    tile.terrainId = 4; // Shallows (passable water)
                    tile.elevation = 0;
                } else if (dist < riverWidth + 1.5f) {
                    tile.terrainId = 2; // Beach
                    tile.elevation = 0;
                }
            } else {
                if (dist < riverWidth) {
                    tile.terrainId = 1; // Deep water
                    tile.elevation = 0;
                } else if (dist < riverWidth + 1.5f) {
                    tile.terrainId = 2; // Beach
                    tile.elevation = 0;
                }
            }
        }
    }
}

void RandomMapGenerator::generateBaltic(const Settings &settings, const std::shared_ptr<Map> &map)
{
    const int size = settings.size;
    float centerX = size / 2.f;
    float centerY = size / 2.f;
    float lakeRadius = size * 0.35f;

    for (int col = 0; col < size; col++) {
        for (int row = 0; row < size; row++) {
            MapTile &tile = map->getTileAt(col, row);
            float dx = col - centerX;
            float dy = row - centerY;
            float dist = std::sqrt(dx * dx + dy * dy);
            float edgeNoise = noise(col * 0.07f + 400.f, row * 0.07f + 400.f) * (size * 0.04f);

            if (dist + edgeNoise < lakeRadius - 2.f) {
                tile.terrainId = 1; // Water
                tile.elevation = 0;
            } else if (dist + edgeNoise < lakeRadius) {
                tile.terrainId = 2; // Beach
                tile.elevation = 0;
            } else {
                float n = noise(col * 0.05f, row * 0.05f);
                tile.terrainId = 0; // Grass
                if (n > 0.65f) tile.terrainId = 6; // Dirt
            }
        }
    }
}

void RandomMapGenerator::generateMediterranean(const Settings &settings, const std::shared_ptr<Map> &map)
{
    const int size = settings.size;

    for (int col = 0; col < size; col++) {
        for (int row = 0; row < size; row++) {
            MapTile &tile = map->getTileAt(col, row);
            float normalizedRow = static_cast<float>(row) / size;
            float edgeNoise = noise(col * 0.06f + 500.f, row * 0.06f + 500.f) * 0.08f;

            // Water band at y = 40%-60%
            float waterLow = 0.40f + edgeNoise;
            float waterHigh = 0.60f + edgeNoise;

            if (normalizedRow > waterLow + 0.02f && normalizedRow < waterHigh - 0.02f) {
                tile.terrainId = 1; // Water
                tile.elevation = 0;
            } else if (normalizedRow > waterLow && normalizedRow < waterHigh) {
                tile.terrainId = 2; // Beach
                tile.elevation = 0;
            } else {
                float n = noise(col * 0.05f, row * 0.05f);
                tile.terrainId = 0; // Grass
                if (n > 0.6f) tile.terrainId = 6;  // Dirt
                if (n > 0.8f) tile.terrainId = 11; // Dirt3
            }
        }
    }
}

void RandomMapGenerator::generateOasis(const Settings &settings, const std::shared_ptr<Map> &map)
{
    const int size = settings.size;
    float centerX = size / 2.f;
    float centerY = size / 2.f;
    float oasisRadius = size * 0.18f;
    float pondRadius = size * 0.08f;

    for (int col = 0; col < size; col++) {
        for (int row = 0; row < size; row++) {
            MapTile &tile = map->getTileAt(col, row);
            float dx = col - centerX;
            float dy = row - centerY;
            float dist = std::sqrt(dx * dx + dy * dy);
            float edgeNoise = noise(col * 0.08f + 600.f, row * 0.08f + 600.f) * (size * 0.02f);

            if (dist + edgeNoise < pondRadius) {
                tile.terrainId = 1; // Water (central pond)
                tile.elevation = 0;
            } else if (dist + edgeNoise < pondRadius + 1.5f) {
                tile.terrainId = 2; // Beach around pond
                tile.elevation = 0;
            } else if (dist + edgeNoise < oasisRadius) {
                tile.terrainId = 10; // Forest ring
            } else {
                // Desert outside
                tile.terrainId = 14; // Desert
                float n = noise(col * 0.04f, row * 0.04f);
                if (n > 0.7f) tile.terrainId = 6; // Dirt patches in desert
            }
        }
    }
}

void RandomMapGenerator::generateTeamIslands(const Settings &settings, const std::shared_ptr<Map> &map)
{
    const int size = settings.size;

    // Fill everything with water first
    for (int col = 0; col < size; col++) {
        for (int row = 0; row < size; row++) {
            MapTile &tile = map->getTileAt(col, row);
            tile.terrainId = 1; // Water
            tile.elevation = 0;
        }
    }

    // Determine team groups — count unique nonzero teams, group players
    // For placement we just need 2 island centers (left and right halves)
    // Players will be placed on islands by placeStartingUnits which clears around them
    int islandCount = 2; // default: 2 islands
    float islandRadius = size * 0.22f;

    struct IslandCenter { float x, y; };
    std::vector<IslandCenter> islands;
    for (int i = 0; i < islandCount; i++) {
        float angle = i * M_PI; // 0 and pi — left and right
        float ix = size / 2.f + (size * 0.25f) * std::cos(angle);
        float iy = size / 2.f + (size * 0.25f) * std::sin(angle);
        islands.push_back({ix, iy});
    }

    // Carve islands
    for (int col = 0; col < size; col++) {
        for (int row = 0; row < size; row++) {
            MapTile &tile = map->getTileAt(col, row);
            for (const auto &island : islands) {
                float dx = col - island.x;
                float dy = row - island.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                float edgeNoise = noise(col * 0.08f + 700.f, row * 0.08f + 700.f) * (size * 0.04f);
                if (dist + edgeNoise < islandRadius - 2.f) {
                    float n = noise(col * 0.05f, row * 0.05f);
                    tile.terrainId = 0; // Grass
                    if (n > 0.65f) tile.terrainId = 6; // Dirt
                    tile.elevation = 2;
                } else if (dist + edgeNoise < islandRadius) {
                    tile.terrainId = 2; // Beach
                    tile.elevation = 0;
                }
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Shared helpers
// ═══════════════════════════════════════════════════════════════════════

void RandomMapGenerator::placeFish(const std::shared_ptr<Map> &map, int size,
                                    UnitManager &unitManager, const std::shared_ptr<Player> &gaia)
{
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

void RandomMapGenerator::placeWallsAround(float centerX, float centerY, int radius,
                                            const std::shared_ptr<Player> &player,
                                            UnitManager &unitManager, const std::shared_ptr<Map> &map)
{
    // Place stone wall segments (ID 117) in a square around the center position
    float tileX = centerX / Constants::TILE_SIZE;
    float tileY = centerY / Constants::TILE_SIZE;

    for (int d = -radius; d <= radius; d++) {
        // Four sides of the square
        struct { float x, y; } positions[] = {
            { tileX + d, tileY - radius }, // top
            { tileX + d, tileY + radius }, // bottom
            { tileX - radius, tileY + d }, // left
            { tileX + radius, tileY + d }, // right
        };

        for (const auto &pos : positions) {
            float px = pos.x * Constants::TILE_SIZE;
            float py = pos.y * Constants::TILE_SIZE;

            int col = static_cast<int>(pos.x);
            int row = static_cast<int>(pos.y);
            int mapSize = map->columnCount();
            if (col < 2 || col >= mapSize - 2 || row < 2 || row >= mapSize - 2) continue;

            const MapTile &tile = map->getTileAt(col, row);
            if (tile.terrainId == 1 || tile.terrainId == 2 || tile.terrainId == 10) continue;

            Unit::Ptr wall = UnitFactory::createUnit(117, player, unitManager);
            if (wall) {
                unitManager.add(wall, MapPos(px, py));
            }
        }
    }
}
