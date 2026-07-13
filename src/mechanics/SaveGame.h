#pragma once

#include <string>
#include <memory>
#include <vector>

class GameState;

// Simple binary save format for freeaoe
// Saves: map terrain, player resources, unit positions/HP, camera
struct SaveGame
{
    static constexpr uint32_t MAGIC = 0x454F4146; // "FAOE"
    static constexpr uint32_t VERSION = 1;

    static bool save(const std::string &path, GameState &state, float cameraX, float cameraY);
    static bool load(const std::string &path, GameState &state, float &cameraX, float &cameraY);

    // List save files in directory
    static std::vector<std::string> listSaves(const std::string &dir);
};
