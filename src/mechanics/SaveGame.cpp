#include "SaveGame.h"

#include "GameState.h"
#include "Player.h"
#include "Map.h"
#include "Unit.h"
#include "UnitManager.h"
#include "core/Logger.h"
#include "core/Constants.h"

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
    if (version != VERSION) {
        WARN << "Unsupported save version:" << version;
        return false;
    }

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

    // Units — skip for now (would need to remove existing and create new)
    uint32_t unitCount = readU32(file);
    DBG << "Save has" << unitCount << "units (resource restore only)";

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
