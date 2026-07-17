#include "SaveGame.h"

#include "GameState.h"
#include "Player.h"
#include "Map.h"
#include "Unit.h"
#include "UnitManager.h"
#include "core/Logger.h"
#include "core/Constants.h"
#include "UnitFactory.h"

#include <genie/dat/Unit.h>
#include <genie/dat/ResourceType.h>

#include <fstream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

// Write helpers
static void writeU32(std::ofstream &f, uint32_t v) { f.write(reinterpret_cast<char*>(&v), 4); }
static void writeFloat(std::ofstream &f, float v) { f.write(reinterpret_cast<char*>(&v), 4); }
static void writeI32(std::ofstream &f, int32_t v) { f.write(reinterpret_cast<char*>(&v), 4); }

// Read helpers
static uint32_t readU32(std::ifstream &f) { uint32_t v; f.read(reinterpret_cast<char*>(&v), 4); return v; }
static float readFloat(std::ifstream &f) { float v; f.read(reinterpret_cast<char*>(&v), 4); return v; }
static int32_t readI32(std::ifstream &f) { int32_t v; f.read(reinterpret_cast<char*>(&v), 4); return v; }

bool SaveGame::save(const std::string &path, GameState &state, float cameraX, float cameraY)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        WARN << "Failed to open save file for writing:" << path;
        return false;
    }

    // Header
    writeU32(file, MAGIC);
    writeU32(file, VERSION);
    writeFloat(file, cameraX);
    writeFloat(file, cameraY);

    // Map
    const MapPtr &map = state.map();
    writeI32(file, map->columnCount());
    writeI32(file, map->rowCount());

    // Terrain IDs and elevations for each tile
    for (int col = 0; col < map->columnCount(); col++) {
        for (int row = 0; row < map->rowCount(); row++) {
            const MapTile &tile = map->getTileAt(col, row);
            writeU32(file, tile.terrainId);
            writeI32(file, tile.elevation);
        }
    }

    // Players
    const auto &players = state.players();
    writeU32(file, players.size());
    for (const auto &player : players) {
        writeI32(file, player->playerId);
        // Resources
        writeFloat(file, player->resourcesAvailable(genie::ResourceType::WoodStorage));
        writeFloat(file, player->resourcesAvailable(genie::ResourceType::FoodStorage));
        writeFloat(file, player->resourcesAvailable(genie::ResourceType::GoldStorage));
        writeFloat(file, player->resourcesAvailable(genie::ResourceType::StoneStorage));
        writeFloat(file, player->resourcesAvailable(genie::ResourceType::PopulationHeadroom));
        writeFloat(file, player->resourcesAvailable(genie::ResourceType::CurrentAge));
    }

    // Units
    const auto &units = state.unitManager()->units();
    uint32_t unitCount = 0;
    for (const auto &unit : units) {
        if (unit && unit->data()) unitCount++;
    }
    writeU32(file, unitCount);

    for (const auto &unit : units) {
        if (!unit || !unit->data()) continue;
        writeI32(file, unit->data()->ID);
        writeI32(file, unit->playerId());
        writeFloat(file, unit->position().x);
        writeFloat(file, unit->position().y);
        writeFloat(file, unit->position().z);
        writeFloat(file, unit->hitpointsLeft());
        writeFloat(file, unit->angle());
        writeFloat(file, unit->creationProgress());
    }

    // === V3 sections ===

    // Save player count for new sections
    const auto &allPlayers = state.players();
    writeU32(file, allPlayers.size());

    // Researched techs per player
    for (size_t p = 0; p < allPlayers.size(); p++) {
        const auto &player = allPlayers[p];
        if (!player) { writeU32(file, 0); continue; }
        const auto &techs = player->researchedTechs();
        writeU32(file, techs.size());
        for (int techId : techs) { writeI32(file, techId); }
    }

    // Diplomacy
    for (size_t p = 0; p < allPlayers.size(); p++) {
        const auto &player = allPlayers[p];
        for (size_t other = 0; other < allPlayers.size(); other++) {
            if (!player || other == p) { writeI32(file, 0); continue; }
            writeI32(file, static_cast<int>(player->diplomaticStanceTo(other)));
        }
    }

    // Market prices
    for (size_t p = 0; p < allPlayers.size(); p++) {
        const auto &player = allPlayers[p];
        if (!player) { writeI32(file, 100); writeI32(file, 100); writeI32(file, 100); continue; }
        writeI32(file, player->marketPrices.basePrice[0]);
        writeI32(file, player->marketPrices.basePrice[1]);
        writeI32(file, player->marketPrices.basePrice[2]);
    }

    DBG << "Saved game to" << path << "with" << unitCount << "units";
    return true;
}

bool SaveGame::load(const std::string &path, GameState &state, float &cameraX, float &cameraY)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        WARN << "Failed to open save file:" << path;
        return false;
    }

    // Header
    uint32_t magic = readU32(file);
    if (magic != MAGIC) {
        WARN << "Invalid save file magic:" << magic;
        return false;
    }
    uint32_t version = readU32(file);
    if (version < 1 || version > VERSION) {
        WARN << "Unsupported save version:" << version;
        return false;
    }
    const bool hasUnitRestore = (version >= 2);

    cameraX = readFloat(file);
    cameraY = readFloat(file);

    // Map dimensions (skip terrain — we'd need to reconstruct the full map)
    int cols = readI32(file);
    int rows = readI32(file);
    DBG << "Save has map" << cols << "x" << rows;

    // Skip terrain data (we'll use scenario's terrain)
    for (int i = 0; i < cols * rows; i++) {
        readU32(file); // terrainId
        readI32(file); // elevation
    }

    // Players — update resources
    uint32_t playerCount = readU32(file);
    for (uint32_t i = 0; i < playerCount; i++) {
        int playerId = readI32(file);
        float wood = readFloat(file);
        float food = readFloat(file);
        float gold = readFloat(file);
        float stone = readFloat(file);
        float pop = readFloat(file);
        float age = readFloat(file);

        // Find matching player and update resources
        for (const auto &player : state.players()) {
            if (player->playerId == playerId) {
                player->setAvailableResource(genie::ResourceType::WoodStorage, wood);
                player->setAvailableResource(genie::ResourceType::FoodStorage, food);
                player->setAvailableResource(genie::ResourceType::GoldStorage, gold);
                player->setAvailableResource(genie::ResourceType::StoneStorage, stone);
                break;
            }
        }
    }

    // Units
    uint32_t unitCount = readU32(file);

    if (hasUnitRestore) {
        DBG << "Save has" << unitCount << "units, restoring...";

        // Remove all existing units
        auto &unitManager = state.unitManager();
        {
            // Copy the vector since remove() modifies it
            UnitVector existingUnits = unitManager->units();
            for (const auto &unit : existingUnits) {
                if (unit) {
                    unitManager->remove(unit);
                }
            }
        }

        // Recreate units from save data
        uint32_t restored = 0;
        for (uint32_t i = 0; i < unitCount; i++) {
            int unitId = readI32(file);
            int playerId = readI32(file);
            float posX = readFloat(file);
            float posY = readFloat(file);
            float posZ = readFloat(file);
            float hp = readFloat(file);
            float angle = readFloat(file);

            // Find the owning player
            std::shared_ptr<Player> owner;
            for (const auto &player : state.players()) {
                if (player->playerId == playerId) {
                    owner = player;
                    break;
                }
            }
            if (!owner) {
                WARN << "No player found for id" << playerId << ", skipping unit" << unitId;
                if (version >= 3) readFloat(file); // skip construction progress
                continue;
            }

            Unit::Ptr unit = UnitFactory::createUnit(unitId, owner, *unitManager);
            if (!unit) {
                WARN << "Failed to create unit" << unitId << "for player" << playerId;
                if (version >= 3) readFloat(file); // skip construction progress
                continue;
            }

            MapPos pos(posX, posY, posZ);
            unitManager->add(unit, pos);
            unit->setAngle(angle);

            // Restore HP by applying damage difference
            float maxHp = unit->data()->HitPoints;
            if (hp < maxHp) {
                unit->takeDamage(maxHp - hp);
            }

            // Construction progress
            if (version >= 3) {
                float progress = readFloat(file);
                unit->setCreationProgress(progress);
            } else {
                unit->setCreationProgress(1.f);
            }
            restored++;
        }

        DBG << "Restored" << restored << "of" << unitCount << "units from save";
    } else {
        DBG << "Save v1 has" << unitCount << "units (resource restore only, skipping)";
        // Skip unit data: 7 fields * 4 bytes each
        for (uint32_t i = 0; i < unitCount; i++) {
            readI32(file); readI32(file);
            readFloat(file); readFloat(file); readFloat(file);
            readFloat(file); readFloat(file);
        }
    }

    // === V3 sections ===
    if (version >= 3) {
        uint32_t totalPlayers = readU32(file);

        // Researched techs
        for (uint32_t p = 0; p < totalPlayers; p++) {
            uint32_t techCount = readU32(file);
            auto player = state.player(p);
            for (uint32_t t = 0; t < techCount; t++) {
                int32_t techId = readI32(file);
                if (player) player->applyResearch(techId);
            }
        }

        // Diplomacy
        for (uint32_t p = 0; p < totalPlayers; p++) {
            auto player = state.player(p);
            for (uint32_t other = 0; other < totalPlayers; other++) {
                int32_t stance = readI32(file);
                if (player && other != p) {
                    player->setDiplomaticStance(other, static_cast<Player::DiplomaticStance>(stance));
                }
            }
        }

        // Market prices
        for (uint32_t p = 0; p < totalPlayers; p++) {
            auto player = state.player(p);
            int32_t f = readI32(file), w = readI32(file), s = readI32(file);
            if (player) {
                player->marketPrices.basePrice[0] = f;
                player->marketPrices.basePrice[1] = w;
                player->marketPrices.basePrice[2] = s;
            }
        }
    }

    return true;
}

std::vector<std::string> SaveGame::listSaves(const std::string &dir)
{
    std::vector<std::string> saves;
    if (!fs::exists(dir)) return saves;

    for (const auto &entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".faoe") {
                saves.push_back(entry.path().string());
            }
        }
    }
    std::sort(saves.begin(), saves.end());
    return saves;
}
