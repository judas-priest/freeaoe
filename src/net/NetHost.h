#pragma once

#include "NetSocket.h"
#include "NetMessage.h"
#include "GameCommand.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <deque>
#include <functional>

/// Relay server for multiplayer games.
/// Listens on a port, accepts up to 7 peer connections.
/// Collects commands from peers into turn bundles and broadcasts them.
class NetHost
{
public:
    static constexpr int MaxPlayers = 8; // including host

    NetHost();
    ~NetHost();

    NetHost(const NetHost &) = delete;
    NetHost &operator=(const NetHost &) = delete;

    /// Start listening on the given port.
    bool start(uint16_t port);

    /// Stop the host, close all connections.
    void stop();

    /// Non-blocking update: accept new connections, receive messages, process turns.
    void update();

    /// Broadcast a turn bundle with the given commands to all peers.
    void broadcastTurn(uint32_t turnNumber, const std::vector<GameCommand> &commands);

    /// True if the host is listening.
    bool isRunning() const { return m_running; }

    /// Number of connected players (including the host itself).
    int playerCount() const { return static_cast<int>(m_peers.size()) + 1; }

    /// Submit local (host player) commands for the current turn.
    void submitLocalCommands(uint32_t turnNumber, const std::vector<GameCommand> &commands);

    /// Retrieve the collected turn bundle when all peers have submitted.
    /// Returns true if the turn is complete, fills outCommands.
    bool collectTurn(uint32_t turnNumber, std::vector<GameCommand> &outCommands);

private:
    struct Peer {
        std::unique_ptr<NetSocket> socket;
        int playerId = -1;
        bool hasSubmittedTurn = false;
        uint32_t submittedTurnNumber = 0;
        std::vector<GameCommand> turnCommands;
    };

    void acceptNewConnections();
    void receiveFromPeers();
    void handlePeerMessage(Peer &peer, const std::vector<uint8_t> &payload);
    void removePeer(size_t index);

    NetSocket m_listenSocket;
    std::vector<Peer> m_peers;
    bool m_running = false;

    // Host's own commands for the current turn
    bool m_localSubmitted = false;
    uint32_t m_localTurnNumber = 0;
    std::vector<GameCommand> m_localCommands;
};
