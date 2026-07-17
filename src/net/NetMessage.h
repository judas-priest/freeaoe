#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <stdexcept>

/// Network message types for the multiplayer protocol.
enum class NetMsgType : uint8_t {
    Ping             = 1,
    Pong             = 2,

    LobbyJoin        = 10,
    LobbyReady       = 11,
    LobbyStart       = 12,
    LobbyChat        = 13,
    LobbySetup       = 14,

    TurnCommands     = 20,
    TurnAck          = 21,

    SyncCheck        = 30,
    SyncMismatch     = 31,

    PlayerDisconnect = 40
};

/// Binary serialization helpers.
/// All multi-byte integers are little-endian.
namespace NetSer {

inline void writeU8(std::vector<uint8_t> &buf, uint8_t v)
{
    buf.push_back(v);
}

inline void writeU16(std::vector<uint8_t> &buf, uint16_t v)
{
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

inline void writeU32(std::vector<uint8_t> &buf, uint32_t v)
{
    buf.push_back(static_cast<uint8_t>(v & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

inline void writeI32(std::vector<uint8_t> &buf, int32_t v)
{
    writeU32(buf, static_cast<uint32_t>(v));
}

inline void writeFloat(std::vector<uint8_t> &buf, float v)
{
    uint32_t bits;
    static_assert(sizeof(float) == sizeof(uint32_t));
    __builtin_memcpy(&bits, &v, sizeof(bits));
    writeU32(buf, bits);
}

inline void writeString(std::vector<uint8_t> &buf, const std::string &s)
{
    writeU16(buf, static_cast<uint16_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

inline uint8_t readU8(const std::vector<uint8_t> &buf, size_t &offset)
{
    if (offset >= buf.size()) throw std::runtime_error("NetSer: readU8 out of bounds");
    return buf[offset++];
}

inline uint16_t readU16(const std::vector<uint8_t> &buf, size_t &offset)
{
    if (offset + 2 > buf.size()) throw std::runtime_error("NetSer: readU16 out of bounds");
    uint16_t v = static_cast<uint16_t>(buf[offset])
               | (static_cast<uint16_t>(buf[offset + 1]) << 8);
    offset += 2;
    return v;
}

inline uint32_t readU32(const std::vector<uint8_t> &buf, size_t &offset)
{
    if (offset + 4 > buf.size()) throw std::runtime_error("NetSer: readU32 out of bounds");
    uint32_t v = static_cast<uint32_t>(buf[offset])
               | (static_cast<uint32_t>(buf[offset + 1]) << 8)
               | (static_cast<uint32_t>(buf[offset + 2]) << 16)
               | (static_cast<uint32_t>(buf[offset + 3]) << 24);
    offset += 4;
    return v;
}

inline int32_t readI32(const std::vector<uint8_t> &buf, size_t &offset)
{
    return static_cast<int32_t>(readU32(buf, offset));
}

inline float readFloat(const std::vector<uint8_t> &buf, size_t &offset)
{
    uint32_t bits = readU32(buf, offset);
    float v;
    __builtin_memcpy(&v, &bits, sizeof(v));
    return v;
}

inline std::string readString(const std::vector<uint8_t> &buf, size_t &offset)
{
    uint16_t len = readU16(buf, offset);
    if (offset + len > buf.size()) throw std::runtime_error("NetSer: readString out of bounds");
    std::string s(buf.begin() + offset, buf.begin() + offset + len);
    offset += len;
    return s;
}

} // namespace NetSer
