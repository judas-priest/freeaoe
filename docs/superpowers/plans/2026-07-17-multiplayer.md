# Multiplayer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Complete multiplayer -- TCP networking, lobby, lockstep sync, 2-8 player games

**Architecture:** Lockstep deterministic simulation with command relay. Each client runs the full simulation locally. Player commands (move, attack, build, research, etc.) are serialized and broadcast via a host server. The game advances in "turns" of ~200ms; commands issued in turn N are executed at turn N+2 (giving network time). Periodic checksums detect desync.

**Tech Stack:** C++20, SDL2, BSD sockets (POSIX `<sys/socket.h>` / `<winsock2.h>`), no external networking library.

**Key insight from code audit:** The existing `GameServer`/`GameClient`/`TunnelToServer`/`TunnelToClient` classes in `src/server/`, `src/client/`, `src/communication/` are OLD unused stubs from 2011. The actual game runs entirely through `GameState::update()` -> `UnitManager::update()`, with user input flowing through `UnitManager::onRightClick()` / `onLeftClick()` which call `IAction::assignTask()` directly. The multiplayer system must intercept these user commands, serialize them, and apply them deterministically on all clients.

**Critical path:** Commands flow like this today:
```
Engine::handleMousePress -> UnitManager::onRightClick -> IAction::assignTask (immediate)
Engine::handleMousePress -> UnitManager::startPlaceBuilding -> placeBuilding (immediate)
ActionPanel button -> UnitManager::enqueueProduceUnit / enqueueResearch (immediate)
```
For multiplayer, these must become:
```
Engine::handleMousePress -> serialize command -> send to host -> host broadcasts -> all clients execute at same turn
```

---

## Phase 1: Networking Layer (TCP sockets)

### Task 1.1: Create NetSocket wrapper class
Create `src/net/NetSocket.h` and `src/net/NetSocket.cpp` -- a thin RAII wrapper around BSD/Winsock TCP sockets.

**File:** `src/net/NetSocket.h`
```cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using SocketHandle = SOCKET;
constexpr SocketHandle INVALID_SOCK = INVALID_SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
using SocketHandle = int;
constexpr SocketHandle INVALID_SOCK = -1;
#endif

class NetSocket {
public:
    NetSocket();
    explicit NetSocket(SocketHandle existing);
    ~NetSocket();

    NetSocket(NetSocket &&other) noexcept;
    NetSocket &operator=(NetSocket &&other) noexcept;
    NetSocket(const NetSocket &) = delete;
    NetSocket &operator=(const NetSocket &) = delete;

    bool listen(uint16_t port);
    std::optional<NetSocket> accept();
    bool connect(const std::string &host, uint16_t port);
    void close();

    bool setNonBlocking(bool nonBlocking);
    bool setNoDelay(bool noDelay);

    // Send raw bytes. Returns bytes sent or -1 on error.
    int send(const uint8_t *data, int len);

    // Receive into buffer. Returns bytes read, 0 = closed, -1 = would block / error.
    int recv(uint8_t *buf, int maxLen);

    // Send a length-prefixed message (4-byte LE length + payload). Returns true on success.
    bool sendMessage(const std::vector<uint8_t> &msg);

    // Try to read a length-prefixed message. Returns empty if incomplete/would block.
    std::optional<std::vector<uint8_t>> recvMessage();

    bool isValid() const { return m_socket != INVALID_SOCK; }
    SocketHandle handle() const { return m_socket; }

    static void initNetworking(); // WSAStartup on Windows, no-op on POSIX
    static void shutdownNetworking();

private:
    SocketHandle m_socket = INVALID_SOCK;
    std::vector<uint8_t> m_recvBuffer; // accumulation buffer for partial reads
};
```

**File:** `src/net/NetSocket.cpp`
```cpp
#include "NetSocket.h"
#include "core/Logger.h"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

static Logger &netLog = Logger::getLogger("freeaoe.net.NetSocket");

void NetSocket::initNetworking() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
}

void NetSocket::shutdownNetworking() {
#ifdef _WIN32
    WSACleanup();
#endif
}

NetSocket::NetSocket() = default;

NetSocket::NetSocket(SocketHandle existing) : m_socket(existing) {}

NetSocket::~NetSocket() { close(); }

NetSocket::NetSocket(NetSocket &&other) noexcept : m_socket(other.m_socket), m_recvBuffer(std::move(other.m_recvBuffer)) {
    other.m_socket = INVALID_SOCK;
}

NetSocket &NetSocket::operator=(NetSocket &&other) noexcept {
    if (this != &other) {
        close();
        m_socket = other.m_socket;
        m_recvBuffer = std::move(other.m_recvBuffer);
        other.m_socket = INVALID_SOCK;
    }
    return *this;
}

bool NetSocket::listen(uint16_t port) {
    m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == INVALID_SOCK) return false;

    int opt = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }
    if (::listen(m_socket, 8) < 0) {
        close();
        return false;
    }
    return true;
}

std::optional<NetSocket> NetSocket::accept() {
    sockaddr_in clientAddr{};
    socklen_t len = sizeof(clientAddr);
    SocketHandle client = ::accept(m_socket, reinterpret_cast<sockaddr*>(&clientAddr), &len);
    if (client == INVALID_SOCK) return std::nullopt;
    return NetSocket(client);
}

bool NetSocket::connect(const std::string &host, uint16_t port) {
    m_socket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket == INVALID_SOCK) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (::connect(m_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        close();
        return false;
    }
    return true;
}

void NetSocket::close() {
    if (m_socket != INVALID_SOCK) {
#ifdef _WIN32
        closesocket(m_socket);
#else
        ::close(m_socket);
#endif
        m_socket = INVALID_SOCK;
    }
}

bool NetSocket::setNonBlocking(bool nonBlocking) {
#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    return ioctlsocket(m_socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(m_socket, F_GETFL, 0);
    if (flags < 0) return false;
    if (nonBlocking) flags |= O_NONBLOCK;
    else flags &= ~O_NONBLOCK;
    return fcntl(m_socket, F_SETFL, flags) == 0;
#endif
}

bool NetSocket::setNoDelay(bool noDelay) {
    int flag = noDelay ? 1 : 0;
    return setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag)) == 0;
}

int NetSocket::send(const uint8_t *data, int len) {
    return ::send(m_socket, reinterpret_cast<const char*>(data), len, 0);
}

int NetSocket::recv(uint8_t *buf, int maxLen) {
    return ::recv(m_socket, reinterpret_cast<char*>(buf), maxLen, 0);
}

bool NetSocket::sendMessage(const std::vector<uint8_t> &msg) {
    uint32_t len = static_cast<uint32_t>(msg.size());
    uint8_t header[4];
    header[0] = len & 0xFF;
    header[1] = (len >> 8) & 0xFF;
    header[2] = (len >> 16) & 0xFF;
    header[3] = (len >> 24) & 0xFF;

    if (send(header, 4) != 4) return false;
    if (len > 0 && send(msg.data(), len) != static_cast<int>(len)) return false;
    return true;
}

std::optional<std::vector<uint8_t>> NetSocket::recvMessage() {
    uint8_t tmp[4096];
    int n = recv(tmp, sizeof(tmp));
    if (n > 0) {
        m_recvBuffer.insert(m_recvBuffer.end(), tmp, tmp + n);
    }

    if (m_recvBuffer.size() < 4) return std::nullopt;

    uint32_t msgLen = m_recvBuffer[0] | (m_recvBuffer[1] << 8) | (m_recvBuffer[2] << 16) | (m_recvBuffer[3] << 24);
    if (m_recvBuffer.size() < 4 + msgLen) return std::nullopt;

    std::vector<uint8_t> result(m_recvBuffer.begin() + 4, m_recvBuffer.begin() + 4 + msgLen);
    m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + 4 + msgLen);
    return result;
}
```

**Test:** `cd build && make -j$(nproc)` (add files to CMakeLists.txt first).

**Commit:** `feat(net): add NetSocket TCP wrapper with length-prefixed messaging`

---

### Task 1.2: Add NetSocket to CMakeLists.txt
Edit `/home/dima/Projects/freeaoe/CMakeLists.txt`. Add a new `NET_SRC` variable and include it in `ENGINE_SRC`.

After line `set(SETTINGS_SRC` block, add:
```cmake
set(NET_SRC
    src/net/NetSocket.cpp
    src/net/NetSocket.h
    src/net/NetMessage.h
    src/net/NetHost.cpp
    src/net/NetHost.h
    src/net/NetClient.cpp
    src/net/NetClient.h
    )
```

In the `ENGINE_SRC` set, add `${NET_SRC}` after `${SETTINGS_SRC}`.

For non-Android non-Windows, no extra libs needed (sockets are in libc). For Windows, ws2_32 is linked via pragma in the .cpp.

**Commit:** `build: add net/ sources to CMakeLists.txt`

---

## Phase 2: Command Serialization

### Task 2.1: Define NetMessage types and serialization
Create `src/net/NetMessage.h` -- defines all network message types and a binary serialization format.

**File:** `src/net/NetMessage.h`
```cpp
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "core/Types.h"

// All messages are: [1 byte type] [payload...]
enum class NetMsgType : uint8_t {
    // Lobby
    JoinRequest     = 0x01,  // client -> host: player name
    JoinAccept      = 0x02,  // host -> client: assigned player slot, map seed
    JoinReject      = 0x03,  // host -> client: reason string
    PlayerList      = 0x04,  // host -> all: list of connected players
    ChatMessage     = 0x05,  // bidirectional: chat text
    GameStart       = 0x06,  // host -> all: game config (map seed, civs, etc.)

    // In-game lockstep
    PlayerCommand   = 0x10,  // client -> host: a player action
    TurnCommands    = 0x11,  // host -> all: all commands for a turn
    TurnAck         = 0x12,  // client -> host: "I finished turn N"
    SyncChecksum    = 0x13,  // client -> host: checksum for turn N
    SyncError       = 0x14,  // host -> all: desync detected

    // Connection management
    Ping            = 0xF0,
    Pong            = 0xF1,
    Disconnect      = 0xFF,
};

// Serializable player command -- every action the player can take
enum class CmdType : uint8_t {
    MoveUnits       = 1,   // unit IDs + target MapPos
    AttackUnit      = 2,   // attacker IDs + target unit ID
    BuildPlace      = 3,   // builder IDs + building type ID + MapPos
    ProduceUnit     = 4,   // building ID + unit type ID
    Research        = 5,   // building ID + tech ID
    GatherResource  = 6,   // unit IDs + target unit ID
    Garrison        = 7,   // unit IDs + target building ID
    Ungarrison      = 8,   // building ID
    SetDiplomacy    = 9,   // target player + stance
    SendTribute     = 10,  // target player + resource type + amount
    DeleteUnit      = 11,  // unit ID
    SetRallyPoint   = 12,  // building ID + MapPos
    Patrol          = 13,  // unit IDs + target MapPos
    Guard           = 14,  // unit IDs + target unit ID
    Follow          = 15,  // unit IDs + target unit ID
    Repair          = 16,  // unit IDs + target unit ID
    Convert         = 17,  // monk IDs + target unit ID
    Heal            = 18,  // monk IDs + target unit ID
    Trade           = 19,  // trade cart ID + target market ID
    PickupRelic     = 20,  // monk ID + relic ID
    FlareSignal     = 21,  // MapPos
    Resign          = 22,  // no payload
    AttackMove      = 23,  // unit IDs + target MapPos
};

// Binary writer/reader helpers
struct NetBuffer {
    std::vector<uint8_t> data;

    void writeU8(uint8_t v) { data.push_back(v); }
    void writeU16(uint16_t v) { data.push_back(v & 0xFF); data.push_back((v >> 8) & 0xFF); }
    void writeU32(uint32_t v) {
        data.push_back(v & 0xFF); data.push_back((v >> 8) & 0xFF);
        data.push_back((v >> 16) & 0xFF); data.push_back((v >> 24) & 0xFF);
    }
    void writeI32(int32_t v) { writeU32(static_cast<uint32_t>(v)); }
    void writeFloat(float v) { uint32_t u; memcpy(&u, &v, 4); writeU32(u); }
    void writeMapPos(const MapPos &p) { writeFloat(p.x); writeFloat(p.y); writeFloat(p.z); }
    void writeString(const std::string &s) {
        writeU16(static_cast<uint16_t>(s.size()));
        data.insert(data.end(), s.begin(), s.end());
    }
    void writeSizeT(size_t v) { writeU32(static_cast<uint32_t>(v)); }

    // Write a list of unit IDs (entity IDs are size_t)
    void writeUnitIds(const std::vector<size_t> &ids) {
        writeU16(static_cast<uint16_t>(ids.size()));
        for (size_t id : ids) writeSizeT(id);
    }
};

struct NetReader {
    const uint8_t *data;
    size_t len;
    size_t pos = 0;

    NetReader(const uint8_t *d, size_t l) : data(d), len(l) {}
    NetReader(const std::vector<uint8_t> &v) : data(v.data()), len(v.size()) {}

    bool hasData(size_t n = 1) const { return pos + n <= len; }

    uint8_t readU8() { return data[pos++]; }
    uint16_t readU16() { uint16_t v = data[pos] | (data[pos+1] << 8); pos += 2; return v; }
    uint32_t readU32() {
        uint32_t v = data[pos] | (data[pos+1] << 8) | (data[pos+2] << 16) | (data[pos+3] << 24);
        pos += 4; return v;
    }
    int32_t readI32() { return static_cast<int32_t>(readU32()); }
    float readFloat() { uint32_t u = readU32(); float f; memcpy(&f, &u, 4); return f; }
    MapPos readMapPos() { float x = readFloat(), y = readFloat(), z = readFloat(); return MapPos(x, y, z); }
    std::string readString() {
        uint16_t len = readU16();
        std::string s(reinterpret_cast<const char*>(data + pos), len);
        pos += len;
        return s;
    }
    size_t readSizeT() { return static_cast<size_t>(readU32()); }

    std::vector<size_t> readUnitIds() {
        uint16_t count = readU16();
        std::vector<size_t> ids(count);
        for (uint16_t i = 0; i < count; i++) ids[i] = readSizeT();
        return ids;
    }
};
```

**Commit:** `feat(net): define NetMessage types and binary serialization`

---

### Task 2.2: Create GameCommand struct for serializable player actions
Create `src/net/GameCommand.h` and `src/net/GameCommand.cpp`.

**File:** `src/net/GameCommand.h`
```cpp
#pragma once

#include "NetMessage.h"
#include "core/Types.h"
#include <vector>
#include <cstdint>

// A single player command that can be serialized over the network
struct GameCommand {
    uint8_t playerSlot = 0; // which player issued this (0-7)
    CmdType type = CmdType::MoveUnits;
    std::vector<size_t> unitIds;
    size_t targetUnitId = 0;
    int32_t typeId = 0;      // unit type ID, tech ID, building type, etc.
    MapPos targetPos;
    uint8_t targetPlayer = 0;
    int32_t amount = 0;
    uint8_t resourceType = 0;
    uint8_t stance = 0;

    std::vector<uint8_t> serialize() const;
    static GameCommand deserialize(NetReader &r);
};
```

**File:** `src/net/GameCommand.cpp`
```cpp
#include "GameCommand.h"

std::vector<uint8_t> GameCommand::serialize() const {
    NetBuffer buf;
    buf.writeU8(playerSlot);
    buf.writeU8(static_cast<uint8_t>(type));
    buf.writeUnitIds(unitIds);
    buf.writeSizeT(targetUnitId);
    buf.writeI32(typeId);
    buf.writeMapPos(targetPos);
    buf.writeU8(targetPlayer);
    buf.writeI32(amount);
    buf.writeU8(resourceType);
    buf.writeU8(stance);
    return buf.data;
}

GameCommand GameCommand::deserialize(NetReader &r) {
    GameCommand cmd;
    cmd.playerSlot = r.readU8();
    cmd.type = static_cast<CmdType>(r.readU8());
    cmd.unitIds = r.readUnitIds();
    cmd.targetUnitId = r.readSizeT();
    cmd.typeId = r.readI32();
    cmd.targetPos = r.readMapPos();
    cmd.targetPlayer = r.readU8();
    cmd.amount = r.readI32();
    cmd.resourceType = r.readU8();
    cmd.stance = r.readU8();
    return cmd;
}
```

**Commit:** `feat(net): add GameCommand struct with serialize/deserialize`

---

## Phase 3: Host and Client Networking

### Task 3.1: Create NetHost -- the relay server
Create `src/net/NetHost.h` and `src/net/NetHost.cpp`. The host listens on a TCP port, accepts up to 7 remote clients (host itself is player 0), and relays commands. Runs in a background thread.

**File:** `src/net/NetHost.h`
```cpp
#pragma once

#include "NetSocket.h"
#include "GameCommand.h"
#include <vector>
#include <string>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>

struct NetPeer {
    NetSocket socket;
    std::string name;
    int playerSlot = -1;
    bool ready = false;
};

class NetHost {
public:
    static constexpr uint16_t DEFAULT_PORT = 21547; // aoe2 port-ish
    static constexpr int MAX_PLAYERS = 8;

    NetHost();
    ~NetHost();

    bool start(uint16_t port = DEFAULT_PORT);
    void stop();

    // Lobby
    int playerCount() const;
    std::vector<std::string> playerNames() const;
    void startGame(uint32_t mapSeed, int mapType, int mapSize, const std::vector<int> &civIds);

    // In-game: collect commands for current turn, then advance
    void submitLocalCommand(const GameCommand &cmd);
    void broadcastTurn(uint32_t turnNumber); // sends TurnCommands to all clients

    // Poll for incoming commands from remote clients
    void poll();

    // Get all commands collected for the current pending turn
    std::vector<GameCommand> pendingCommands() const;

    // Check if all clients have acknowledged turn N
    bool allClientsAcked(uint32_t turnNumber) const;

    bool isRunning() const { return m_running.load(); }

private:
    void acceptLoop();

    NetSocket m_listenSocket;
    std::vector<std::unique_ptr<NetPeer>> m_peers;
    mutable std::mutex m_mutex;
    std::thread m_acceptThread;
    std::atomic<bool> m_running{false};

    std::vector<GameCommand> m_pendingCommands;
    std::vector<uint32_t> m_clientAckedTurn; // per-peer last acked turn
};
```

**File:** `src/net/NetHost.cpp` -- implement `start()`, `stop()`, `acceptLoop()`, `poll()`, `broadcastTurn()`, `submitLocalCommand()`. Each method is short:

- `start()`: call `initNetworking()`, `listen(port)`, `setNonBlocking(true)`, start accept thread.
- `acceptLoop()`: loop calling `accept()`, create `NetPeer`, assign slot.
- `poll()`: iterate peers, call `recvMessage()`, parse `PlayerCommand` messages, add to `m_pendingCommands`.
- `broadcastTurn()`: serialize all pending commands into a `TurnCommands` message, send to every peer.
- `submitLocalCommand()`: push to `m_pendingCommands` (host's own commands).

Implementation: ~120 lines. Full code in the task.

**Commit:** `feat(net): add NetHost relay server with lobby and turn broadcasting`

---

### Task 3.2: Create NetClient -- remote client connection
Create `src/net/NetClient.h` and `src/net/NetClient.cpp`. Connects to host, sends commands, receives turn bundles.

**File:** `src/net/NetClient.h`
```cpp
#pragma once

#include "NetSocket.h"
#include "GameCommand.h"
#include <vector>
#include <string>
#include <mutex>
#include <queue>
#include <functional>

struct TurnData {
    uint32_t turnNumber = 0;
    std::vector<GameCommand> commands;
};

class NetClient {
public:
    NetClient();
    ~NetClient();

    bool connect(const std::string &host, uint16_t port);
    void disconnect();

    // Lobby
    bool sendJoinRequest(const std::string &playerName);
    int assignedSlot() const { return m_slot; }

    // In-game
    void sendCommand(const GameCommand &cmd);
    void sendTurnAck(uint32_t turnNumber);
    void sendChecksum(uint32_t turnNumber, uint32_t checksum);

    // Poll network; returns true if a new TurnData is available
    bool poll();
    bool hasPendingTurn() const;
    TurnData popTurnData();

    bool isConnected() const { return m_socket.isValid(); }

    // Lobby callbacks
    std::function<void(const std::vector<std::string> &)> onPlayerListUpdated;
    std::function<void(uint32_t mapSeed, int mapType, int mapSize, const std::vector<int> &civIds)> onGameStart;

private:
    void handleMessage(const std::vector<uint8_t> &msg);

    NetSocket m_socket;
    int m_slot = -1;
    std::queue<TurnData> m_turnQueue;
    mutable std::mutex m_mutex;
};
```

Implementation: ~100 lines. `poll()` calls `recvMessage()` in a loop, dispatches by `NetMsgType`. `TurnCommands` messages are parsed into `TurnData` and queued.

**Commit:** `feat(net): add NetClient for connecting to host and receiving turns`

---

## Phase 4: Lockstep Simulation Engine

### Task 4.1: Create LockstepManager
Create `src/net/LockstepManager.h` and `src/net/LockstepManager.cpp`. This is the core synchronization engine that sits between the game simulation and the network layer.

**File:** `src/net/LockstepManager.h`
```cpp
#pragma once

#include "GameCommand.h"
#include "NetHost.h"
#include "NetClient.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

class UnitManager;
struct Player;

class LockstepManager {
public:
    static constexpr int TURN_LENGTH_MS = 200;   // 200ms per turn (5 turns/sec)
    static constexpr int COMMAND_DELAY_TURNS = 2; // commands execute 2 turns after issued

    enum class Mode { Offline, Host, Client };

    LockstepManager();

    // Setup
    void setMode(Mode mode) { m_mode = mode; }
    Mode mode() const { return m_mode; }
    void setLocalPlayerSlot(int slot) { m_localSlot = slot; }
    int localPlayerSlot() const { return m_localSlot; }

    void setHost(std::shared_ptr<NetHost> host) { m_host = host; }
    void setClient(std::shared_ptr<NetClient> client) { m_client = client; }

    // Called by the game to queue a local player command (instead of executing immediately)
    void queueCommand(const GameCommand &cmd);

    // Called every frame. Returns true if the simulation should advance one turn.
    // Fills outCommands with the commands to execute this turn.
    bool update(int64_t currentTimeMs, std::vector<GameCommand> &outCommands);

    // Current turn number
    uint32_t currentTurn() const { return m_currentTurn; }

    // Compute a sync checksum from game state
    static uint32_t computeChecksum(const UnitManager &unitManager, const std::vector<std::shared_ptr<Player>> &players);

private:
    Mode m_mode = Mode::Offline;
    int m_localSlot = 0;

    uint32_t m_currentTurn = 0;
    int64_t m_lastTurnTime = 0;

    // Commands queued locally, to be sent at next opportunity
    std::vector<GameCommand> m_localCommandBuffer;

    // Commands received for future turns: turn -> commands
    std::unordered_map<uint32_t, std::vector<GameCommand>> m_turnCommandBuffers;

    std::shared_ptr<NetHost> m_host;
    std::shared_ptr<NetClient> m_client;
};
```

**File:** `src/net/LockstepManager.cpp`
```cpp
#include "LockstepManager.h"
#include "mechanics/UnitManager.h"
#include "mechanics/Player.h"
#include "mechanics/Unit.h"

LockstepManager::LockstepManager() = default;

void LockstepManager::queueCommand(const GameCommand &cmd) {
    m_localCommandBuffer.push_back(cmd);
}

bool LockstepManager::update(int64_t currentTimeMs, std::vector<GameCommand> &outCommands) {
    outCommands.clear();

    if (m_mode == Mode::Offline) {
        // In offline mode, just execute commands immediately (like current behavior)
        outCommands = std::move(m_localCommandBuffer);
        m_localCommandBuffer.clear();
        return true; // always advance
    }

    // Network modes: check timing
    if (currentTimeMs - m_lastTurnTime < TURN_LENGTH_MS) {
        return false; // not time for next turn yet
    }

    // Send local commands to network
    if (m_mode == Mode::Host) {
        for (const auto &cmd : m_localCommandBuffer) {
            m_host->submitLocalCommand(cmd);
        }
        m_localCommandBuffer.clear();
        m_host->poll();

        // Broadcast turn and collect commands
        m_host->broadcastTurn(m_currentTurn + COMMAND_DELAY_TURNS);
        outCommands = m_host->pendingCommands();
    } else if (m_mode == Mode::Client) {
        for (const auto &cmd : m_localCommandBuffer) {
            m_client->sendCommand(cmd);
        }
        m_localCommandBuffer.clear();
        m_client->poll();

        // Wait for turn data from host
        if (!m_client->hasPendingTurn()) {
            return false; // host hasn't sent this turn's commands yet -- stall
        }
        TurnData td = m_client->popTurnData();
        outCommands = std::move(td.commands);

        m_client->sendTurnAck(m_currentTurn);
    }

    m_lastTurnTime = currentTimeMs;
    m_currentTurn++;
    return true;
}

uint32_t LockstepManager::computeChecksum(const UnitManager &unitManager, const std::vector<std::shared_ptr<Player>> &players) {
    // Simple FNV-1a over unit positions and player resources
    uint32_t hash = 2166136261u;
    auto mix = [&](uint32_t val) {
        hash ^= val;
        hash *= 16777619u;
    };

    for (const auto &unit : unitManager.units()) {
        uint32_t px, py;
        memcpy(&px, &unit->position().x, 4);
        memcpy(&py, &unit->position().y, 4);
        mix(static_cast<uint32_t>(unit->id));
        mix(px);
        mix(py);
    }

    for (const auto &player : players) {
        uint32_t food, wood, gold, stone;
        float f = player->resourcesAvailable(genie::ResourceType::FoodStorage);
        float w = player->resourcesAvailable(genie::ResourceType::WoodStorage);
        float g = player->resourcesAvailable(genie::ResourceType::GoldStorage);
        float s = player->resourcesAvailable(genie::ResourceType::StoneStorage);
        memcpy(&food, &f, 4); memcpy(&wood, &w, 4);
        memcpy(&gold, &g, 4); memcpy(&stone, &s, 4);
        mix(food); mix(wood); mix(gold); mix(stone);
    }

    return hash;
}
```

**Commit:** `feat(net): add LockstepManager for turn-based command synchronization`

---

### Task 4.2: Add LockstepManager to GameState
Edit `/home/dima/Projects/freeaoe/src/mechanics/GameState.h` and `.cpp`:

1. Add `#include "net/LockstepManager.h"` and a member `std::shared_ptr<LockstepManager> m_lockstep;`
2. Add accessor `LockstepManager &lockstep() { return *m_lockstep; }`
3. In constructor, create `m_lockstep = std::make_shared<LockstepManager>();`
4. In `GameState::update()`, before `m_unitManager->update(time)`, call:
```cpp
std::vector<GameCommand> turnCommands;
if (m_lockstep->mode() != LockstepManager::Mode::Offline) {
    if (!m_lockstep->update(time, turnCommands)) {
        return false; // stalling, waiting for network
    }
    executeCommands(turnCommands);
}
```
5. Add `void GameState::executeCommands(const std::vector<GameCommand> &commands)` that applies each command to the simulation (see Task 4.3).

**Commit:** `feat(net): integrate LockstepManager into GameState update loop`

---

### Task 4.3: Implement GameState::executeCommands -- command executor
Add method to `/home/dima/Projects/freeaoe/src/mechanics/GameState.cpp`:

```cpp
void GameState::executeCommands(const std::vector<GameCommand> &commands) {
    for (const auto &cmd : commands) {
        Player::Ptr cmdPlayer = player(cmd.playerSlot);
        if (!cmdPlayer) continue;

        switch (cmd.type) {
        case CmdType::MoveUnits: {
            for (size_t uid : cmd.unitIds) {
                Unit::Ptr unit = m_unitManager->unitById(uid);
                if (unit && unit->playerId() == cmdPlayer->playerId) {
                    m_unitManager->moveUnitTo(unit, cmd.targetPos);
                }
            }
            break;
        }
        case CmdType::AttackUnit: {
            Unit::Ptr target = m_unitManager->unitById(cmd.targetUnitId);
            if (!target) break;
            for (size_t uid : cmd.unitIds) {
                Unit::Ptr unit = m_unitManager->unitById(uid);
                if (!unit || unit->playerId() != cmdPlayer->playerId) continue;
                Task task = unit->actions.findTaskWithTarget(target);
                if (task.isValid()) {
                    IAction::assignTask(task, unit, IAction::AssignType::Replace);
                }
            }
            break;
        }
        case CmdType::ProduceUnit: {
            // Find the building and enqueue production
            Unit::Ptr building = m_unitManager->unitById(cmd.unitIds.empty() ? 0 : cmd.unitIds[0]);
            if (building && building->playerId() == cmdPlayer->playerId) {
                const genie::Unit &unitData = cmdPlayer->civilization.unitData(cmd.typeId);
                UnitVector producers;
                producers.push_back(building);
                m_unitManager->enqueueProduceUnit(&unitData, producers);
            }
            break;
        }
        case CmdType::Research: {
            Unit::Ptr building = m_unitManager->unitById(cmd.unitIds.empty() ? 0 : cmd.unitIds[0]);
            if (building && building->playerId() == cmdPlayer->playerId) {
                // Tech lookup by index
                const auto &techs = DataManager::Inst().allTechs();
                if (cmd.typeId >= 0 && cmd.typeId < static_cast<int>(techs.size())) {
                    UnitVector producers;
                    producers.push_back(building);
                    m_unitManager->enqueueResearch(&techs[cmd.typeId], producers);
                }
            }
            break;
        }
        case CmdType::Resign: {
            cmdPlayer->resign();
            break;
        }
        // Add remaining command types as stubs initially, implement one by one
        default:
            WARN << "Unhandled multiplayer command type:" << static_cast<int>(cmd.type);
            break;
        }
    }
}
```

This requires adding `Unit::Ptr UnitManager::unitById(size_t id)` -- a lookup method.

**Commit:** `feat(net): implement command executor for lockstep multiplayer`

---

### Task 4.4: Add UnitManager::unitById lookup
Edit `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.h`:
```cpp
Unit::Ptr unitById(size_t id) const;
```

Edit `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp`:
```cpp
Unit::Ptr UnitManager::unitById(size_t id) const {
    for (const auto &unit : m_units) {
        if (unit->id == id) return unit;
    }
    return nullptr;
}
```

Also add an `std::unordered_map<size_t, Unit::Ptr> m_unitIndex;` member for O(1) lookup, populated in `add()` and cleaned in `remove()`.

**Commit:** `feat: add UnitManager::unitById with O(1) index lookup`

---

## Phase 5: Intercept Player Commands for Network

### Task 5.1: Add command interception layer to UnitManager
The key change: when in multiplayer mode, `onRightClick`, `startPlaceBuilding`, `enqueueProduceUnit`, and `enqueueResearch` must NOT execute immediately. Instead they create a `GameCommand` and pass it to `LockstepManager::queueCommand()`.

Edit `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.h`:
```cpp
void setLockstep(std::shared_ptr<LockstepManager> lockstep) { m_lockstep = lockstep; }
bool isMultiplayer() const { return m_lockstep && m_lockstep->mode() != LockstepManager::Mode::Offline; }
```
Add member: `std::shared_ptr<LockstepManager> m_lockstep;`

Edit `/home/dima/Projects/freeaoe/src/mechanics/UnitManager.cpp`:

In `onRightClick()`, wrap the action-assignment block: if `isMultiplayer()`, build a `GameCommand` instead of calling `IAction::assignTask()` directly. For moves:
```cpp
if (isMultiplayer()) {
    GameCommand cmd;
    cmd.playerSlot = m_lockstep->localPlayerSlot();
    cmd.type = CmdType::MoveUnits;
    for (const auto &u : m_selectedUnits) {
        if (u->playerId() == humanPlayer->playerId)
            cmd.unitIds.push_back(u->id);
    }
    cmd.targetPos = mapPos;
    m_lockstep->queueCommand(cmd);
    return;
}
```

Similarly for attack (if `m_tasksUnderCursor` has Combat task -> `CmdType::AttackUnit`), gather, etc.

Do the same for `enqueueProduceUnit()` and `enqueueResearch()`.

**Commit:** `feat(net): intercept player commands in UnitManager for multiplayer relay`

---

### Task 5.2: Handle remaining command types
Extend the `onRightClick` interception to handle:
- GatherResource (task has ActionType GatherRebuild/Hunt)
- BuildPlace (from placeBuilding)
- Garrison, Patrol, Guard, Follow, Repair, Convert, Heal

Each one creates the appropriate `GameCommand` and calls `m_lockstep->queueCommand()`.

Also intercept `ActionPanel` button presses that call `enqueueProduceUnit` and `enqueueResearch`.

**Commit:** `feat(net): intercept all player action types for multiplayer serialization`

---

## Phase 6: Lobby System

### Task 6.1: Create LobbyScreen UI
Create `src/ui/LobbyScreen.h` and `src/ui/LobbyScreen.cpp` -- a simple lobby screen with:
- Host button (starts NetHost, shows waiting screen)
- Join button (shows IP input, connects NetClient)
- Player list (updated as players join)
- Civ selection dropdowns (one per slot)
- Map type / size selection
- Start button (host only)

Use the existing `UiScreen` base class and `TextButton` for buttons. Reuse the parchment background from `Dialog`.

**File:** `src/ui/LobbyScreen.h`
```cpp
#pragma once

#include "UiScreen.h"
#include "TextButton.h"
#include "net/NetHost.h"
#include "net/NetClient.h"
#include <vector>
#include <string>
#include <memory>

class LobbyScreen : public UiScreen {
public:
    enum class Result { None, StartGame, Cancel };

    LobbyScreen();

    void setRenderTarget(const std::shared_ptr<IRenderTarget> &target);

    Result update();
    void render();

    bool isHosting() const { return m_host != nullptr; }
    std::shared_ptr<NetHost> host() const { return m_host; }
    std::shared_ptr<NetClient> client() const { return m_client; }

    // Game settings chosen in lobby
    uint32_t mapSeed() const { return m_mapSeed; }
    int mapType() const { return m_mapType; }
    int mapSize() const { return m_mapSize; }
    std::vector<int> civIds() const { return m_civIds; }
    int localSlot() const { return m_localSlot; }
    int playerCount() const { return m_playerCount; }

private:
    void hostGame();
    void joinGame(const std::string &ip);

    std::shared_ptr<NetHost> m_host;
    std::shared_ptr<NetClient> m_client;

    TextButton m_btnHost;
    TextButton m_btnJoin;
    TextButton m_btnStart;
    TextButton m_btnCancel;

    std::vector<std::string> m_playerNames;
    int m_localSlot = 0;
    int m_playerCount = 2;

    uint32_t m_mapSeed = 0;
    int m_mapType = 0;
    int m_mapSize = 120;
    std::vector<int> m_civIds;

    Result m_result = Result::None;
    std::shared_ptr<IRenderTarget> m_renderTarget;
};
```

**Commit:** `feat(ui): add LobbyScreen for multiplayer game setup`

---

### Task 6.2: Integrate LobbyScreen into Engine
Edit `/home/dima/Projects/freeaoe/src/Engine.h`:
- Add `#include "ui/LobbyScreen.h"`
- Add member `std::unique_ptr<LobbyScreen> m_lobbyScreen;`
- Add `void showMultiplayerLobby();`

Edit `/home/dima/Projects/freeaoe/src/Engine.cpp`:
- In `showStartScreen()` or menu, add a "Multiplayer" button that calls `showMultiplayerLobby()`
- `showMultiplayerLobby()` creates the lobby screen, enters a polling loop
- When `LobbyScreen::Result::StartGame`, create `GameState` with the lockstep manager configured

**Commit:** `feat: integrate multiplayer lobby into Engine main menu`

---

## Phase 7: Deterministic Simulation Fixes

### Task 7.1: Fix random number usage for determinism
The simulation must produce identical results on all clients. Audit and fix all uses of `rand()` and floating-point that could diverge:

1. Create `src/net/SyncRandom.h` with a seedable PRNG:
```cpp
#pragma once
#include <cstdint>

class SyncRandom {
public:
    static SyncRandom &instance() { static SyncRandom s; return s; }

    void seed(uint32_t s) { m_state = s; }

    // xorshift32
    uint32_t next() {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return m_state;
    }

    int nextInt(int min, int max) {
        return min + static_cast<int>(next() % (max - min + 1));
    }

    float nextFloat() {
        return static_cast<float>(next()) / static_cast<float>(UINT32_MAX);
    }

private:
    uint32_t m_state = 12345;
};
```

2. Replace `rand()` calls in game mechanics (Unit, Map, actions) with `SyncRandom::instance()`. Leave `rand()` in UI/rendering (those don't affect game state).

Key files to audit:
- `/home/dima/Projects/freeaoe/src/mechanics/RandomMapGenerator.cpp` -- use SyncRandom with shared seed
- `/home/dima/Projects/freeaoe/src/mechanics/GameState.cpp` -- `setupRandomMap()` uses `rand()` for civ selection
- `/home/dima/Projects/freeaoe/src/actions/ActionAttack.cpp` -- damage variance
- `/home/dima/Projects/freeaoe/src/actions/ActionConvert.cpp` -- conversion probability

**Commit:** `feat(net): add SyncRandom deterministic PRNG for multiplayer`

---

### Task 7.2: Replace rand() in game-state-affecting code
Search all action files and mechanics for `rand()`. Replace with `SyncRandom`. Do NOT touch rendering or UI code.

```bash
grep -rn "rand()" src/actions/ src/mechanics/ --include="*.cpp"
```

Fix each occurrence. The map generator should take a seed parameter from the lobby.

**Commit:** `fix(net): replace all game-state rand() with SyncRandom for determinism`

---

### Task 7.3: Add periodic sync checksum validation
In `GameState::update()`, every 50 turns (~10 seconds), compute and compare checksums:

```cpp
if (m_lockstep->mode() != LockstepManager::Mode::Offline && m_lockstep->currentTurn() % 50 == 0) {
    uint32_t checksum = LockstepManager::computeChecksum(*m_unitManager, m_players);
    if (m_lockstep->mode() == LockstepManager::Mode::Client) {
        m_client->sendChecksum(m_lockstep->currentTurn(), checksum);
    } else {
        // Host compares incoming checksums from clients
        // If mismatch, broadcast SyncError
    }
}
```

**Commit:** `feat(net): periodic sync checksum for desync detection`

---

## Phase 8: Game Start Synchronization

### Task 8.1: Synchronize game setup across clients
When the host clicks "Start Game" in the lobby:

1. Host generates a random map seed
2. Host sends `GameStart` message with: map seed, map type, map size, player count, civ IDs per slot
3. All clients receive `GameStart`, call `GameState::setupRandomMap()` with the same seed
4. The deterministic PRNG is seeded with the map seed -- all clients generate identical maps
5. Each client's `LockstepManager` starts at turn 0

Edit `GameState::setupRandomMap()` to accept a seed parameter and use `SyncRandom::instance().seed(seed)`.

**Commit:** `feat(net): synchronize game setup via GameStart message with shared seed`

---

### Task 8.2: Handle player slot -> player ID mapping
In multiplayer, each network peer has a `playerSlot` (0-7). Map this to in-game `playerId` (1-8, since 0 is Gaia):
- `playerSlot 0` (host) = `playerId 1`
- `playerSlot N` = `playerId N+1`

In `GameState::executeCommands()`, use `player(cmd.playerSlot + 1)` to get the correct player.

Each client's `m_humanPlayer` is set to their own slot's player. The other human players run without AI -- their commands come from the network.

**Commit:** `feat(net): map network player slots to in-game player IDs`

---

## Phase 9: Polish and Edge Cases

### Task 9.1: Handle player disconnection
In `NetHost::poll()`, detect when a peer's socket returns 0 (closed). Mark that player as disconnected. Options:
- Pause game and wait for reconnect (timeout 30s)
- After timeout, resign the disconnected player (`Player::resign()`)
- Broadcast a `Disconnect` message to remaining clients

**Commit:** `feat(net): handle player disconnection with timeout and auto-resign`

---

### Task 9.2: Add speed control and pause for multiplayer
In multiplayer, game speed changes must be synchronized. Add `CmdType::SetSpeed` and `CmdType::Pause` command types. Only the host can change speed. Broadcast to all clients.

**Commit:** `feat(net): synchronized game speed and pause for multiplayer`

---

### Task 9.3: Chat messaging in multiplayer
Extend the existing chat system (`Engine::m_chat`, `onChatMessage()`) to send chat messages over the network. Use the existing `NetMsgType::ChatMessage`. Route through `NetHost`/`NetClient`.

**Commit:** `feat(net): multiplayer chat messages over network`

---

### Task 9.4: Multiplayer fog of war -- each client sees only their own fog
Already handled: each `Player` has its own `VisibilityMap`. In multiplayer, each client's `m_humanPlayer` points to their own player, so fog of war works correctly without changes. Verify this by testing with two instances.

**Commit:** (no code change, just verification)

---

## Phase 10: Testing and Integration

### Task 10.1: Add "--host" and "--join" command-line flags
Edit `/home/dima/Projects/freeaoe/src/global/Config.h` and `Config.cpp` to add:
- `--host [port]` -- start as host on given port
- `--join <ip[:port]>` -- connect to host
- `--name <player_name>` -- set player name

These bypass the lobby UI for quick testing.

**Commit:** `feat: add --host and --join CLI flags for multiplayer`

---

### Task 10.2: Two-instance local test
Test procedure (no unit tests -- just build and run):

```bash
# Terminal 1: Host
cd build && make -j$(nproc) && ./freeaoe --host 21547 --name "Player1"

# Terminal 2: Client
cd build && ./freeaoe --join 127.0.0.1:21547 --name "Player2"
```

Verify:
1. Client connects and appears in host lobby
2. Host starts game, both see same map
3. Moving units on host is visible on client and vice versa
4. Building and researching work across network
5. No desync warnings in first 5 minutes of play

**Commit:** `test: verify two-instance multiplayer on localhost`

---

## Summary of new files

| File | Purpose |
|------|---------|
| `src/net/NetSocket.h/.cpp` | TCP socket wrapper |
| `src/net/NetMessage.h` | Message types and binary serialization |
| `src/net/GameCommand.h/.cpp` | Serializable player command |
| `src/net/NetHost.h/.cpp` | Relay server (host) |
| `src/net/NetClient.h/.cpp` | Client connection |
| `src/net/LockstepManager.h/.cpp` | Turn-based lockstep synchronization |
| `src/net/SyncRandom.h` | Deterministic PRNG |
| `src/ui/LobbyScreen.h/.cpp` | Multiplayer lobby UI |

## Summary of modified files

| File | Change |
|------|--------|
| `CMakeLists.txt` | Add `NET_SRC` to build |
| `src/mechanics/GameState.h/.cpp` | Add LockstepManager, executeCommands() |
| `src/mechanics/UnitManager.h/.cpp` | Add unitById(), command interception |
| `src/mechanics/RandomMapGenerator.cpp` | Use SyncRandom |
| `src/actions/ActionAttack.cpp` | Use SyncRandom |
| `src/actions/ActionConvert.cpp` | Use SyncRandom |
| `src/Engine.h/.cpp` | Add lobby screen, --host/--join flags |
| `src/global/Config.h/.cpp` | Add multiplayer CLI options |
