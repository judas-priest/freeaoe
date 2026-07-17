#pragma once

#include "NetSocket.h"
#include "NetMessage.h"
#include "GameCommand.h"

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <mutex>

/// Client connection for multiplayer games.
/// Connects to a NetHost, sends local commands, receives turn bundles.
class NetClient
{
public:
    NetClient();
    ~NetClient();

    NetClient(const NetClient &) = delete;
    NetClient &operator=(const NetClient &) = delete;

    /// Connect to a host.
    bool connect(const std::string &host, uint16_t port);

    /// Disconnect from the host.
    void disconnect();

    /// Send local commands for the given turn to the host.
    void sendCommands(uint32_t turnNumber, const std::vector<GameCommand> &commands);

    /// Non-blocking update: receive messages, queue turns.
    void update();

    /// Pop the next ready turn's commands. Returns true if a turn was available.
    bool popTurn(uint32_t &outTurnNumber, std::vector<GameCommand> &outCommands);

    /// True if connected to a host.
    bool isConnected() const { return m_socket.isValid(); }

    /// The player ID assigned by the host.
    int assignedPlayerId() const { return m_playerId; }

private:
    void handleMessage(const std::vector<uint8_t> &payload);

    NetSocket m_socket;
    int m_playerId = -1;

    struct TurnBundle {
        uint32_t turnNumber;
        std::vector<GameCommand> commands;
    };

    std::deque<TurnBundle> m_receivedTurns;
};
