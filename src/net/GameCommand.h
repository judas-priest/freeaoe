#pragma once

#include "NetMessage.h"

#include <cstdint>
#include <string>
#include <vector>

/// Types of in-game commands that can be issued by a player.
enum class CommandType : uint8_t {
    Move         = 1,
    Attack       = 2,
    Build        = 3,
    Train        = 4,
    Research     = 5,
    Garrison     = 6,
    Ungarrison   = 7,
    SetRallyPoint = 8,
    Delete       = 9,
    SetStance    = 10,
    SetFormation = 11,
    Stop         = 12,
    Patrol       = 13,
    AttackMove   = 14,
    Trade        = 15,
    Repair       = 16,
    Heal         = 17,
    Convert      = 18,
    PickupRelic  = 19,
    BuyResource  = 20,
    SellResource = 21,
    Tribute      = 22,
    Chat         = 23,
    SetSpeed     = 24
};

/// A serializable player command for lockstep multiplayer.
struct GameCommand {
    CommandType type = CommandType::Stop;
    int playerId = 0;
    std::vector<int> unitIds;   ///< Selected units
    int targetId = -1;          ///< Target unit/building
    float x = 0;               ///< Target position X
    float y = 0;               ///< Target position Y
    int buildingType = -1;     ///< For Build
    int unitType = -1;         ///< For Train
    int techId = -1;           ///< For Research
    int resourceType = -1;    ///< For Buy/Sell
    int amount = 0;
    std::string message;       ///< For Chat

    /// Serialize this command to a byte buffer.
    std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> buf;

        NetSer::writeU8(buf, static_cast<uint8_t>(type));
        NetSer::writeI32(buf, playerId);

        // Unit IDs list
        NetSer::writeU16(buf, static_cast<uint16_t>(unitIds.size()));
        for (int id : unitIds) {
            NetSer::writeI32(buf, id);
        }

        NetSer::writeI32(buf, targetId);
        NetSer::writeFloat(buf, x);
        NetSer::writeFloat(buf, y);
        NetSer::writeI32(buf, buildingType);
        NetSer::writeI32(buf, unitType);
        NetSer::writeI32(buf, techId);
        NetSer::writeI32(buf, resourceType);
        NetSer::writeI32(buf, amount);
        NetSer::writeString(buf, message);

        return buf;
    }

    /// Deserialize a command from a byte buffer at the given offset.
    static GameCommand deserialize(const std::vector<uint8_t> &data, size_t &offset)
    {
        GameCommand cmd;

        cmd.type = static_cast<CommandType>(NetSer::readU8(data, offset));
        cmd.playerId = NetSer::readI32(data, offset);

        uint16_t unitCount = NetSer::readU16(data, offset);
        cmd.unitIds.resize(unitCount);
        for (uint16_t i = 0; i < unitCount; i++) {
            cmd.unitIds[i] = NetSer::readI32(data, offset);
        }

        cmd.targetId = NetSer::readI32(data, offset);
        cmd.x = NetSer::readFloat(data, offset);
        cmd.y = NetSer::readFloat(data, offset);
        cmd.buildingType = NetSer::readI32(data, offset);
        cmd.unitType = NetSer::readI32(data, offset);
        cmd.techId = NetSer::readI32(data, offset);
        cmd.resourceType = NetSer::readI32(data, offset);
        cmd.amount = NetSer::readI32(data, offset);
        cmd.message = NetSer::readString(data, offset);

        return cmd;
    }
};
