#include "NetHost.h"
#include "core/Logger.h"

NetHost::NetHost() = default;

NetHost::~NetHost()
{
    stop();
}

bool NetHost::start(uint16_t port)
{
    if (m_running) {
        WARN << "NetHost already running";
        return false;
    }

    if (!m_listenSocket.listen(port)) {
        WARN << "NetHost failed to listen on port" << port;
        return false;
    }

    m_listenSocket.setNonBlocking();
    m_running = true;
    DBG << "NetHost started on port" << port;
    return true;
}

void NetHost::stop()
{
    m_peers.clear();
    m_listenSocket.close();
    m_running = false;
    m_localSubmitted = false;
    m_localCommands.clear();
}

void NetHost::update()
{
    if (!m_running) return;

    acceptNewConnections();
    receiveFromPeers();
}

void NetHost::broadcastTurn(uint32_t turnNumber, const std::vector<GameCommand> &commands)
{
    // Build the TurnBundle payload:
    // [msgType:1][turnNumber:4][cmdCount:2][cmd0...cmdN]
    std::vector<uint8_t> payload;
    NetSer::writeU8(payload, static_cast<uint8_t>(NetMsgType::TurnAck));
    NetSer::writeU32(payload, turnNumber);
    NetSer::writeU16(payload, static_cast<uint16_t>(commands.size()));
    for (const auto &cmd : commands) {
        std::vector<uint8_t> cmdData = cmd.serialize();
        // Write command size then command data
        NetSer::writeU16(payload, static_cast<uint16_t>(cmdData.size()));
        payload.insert(payload.end(), cmdData.begin(), cmdData.end());
    }

    for (auto &peer : m_peers) {
        if (peer.socket && peer.socket->isValid()) {
            if (!peer.socket->sendMessage(payload)) {
                WARN << "Failed to send turn bundle to player" << peer.playerId;
            }
        }
    }
}

void NetHost::submitLocalCommands(uint32_t turnNumber, const std::vector<GameCommand> &commands)
{
    m_localSubmitted = true;
    m_localTurnNumber = turnNumber;
    m_localCommands = commands;
}

bool NetHost::collectTurn(uint32_t turnNumber, std::vector<GameCommand> &outCommands)
{
    // Check if the host's local commands are ready
    if (!m_localSubmitted || m_localTurnNumber != turnNumber) {
        return false;
    }

    // Check all peers
    for (const auto &peer : m_peers) {
        if (!peer.hasSubmittedTurn || peer.submittedTurnNumber != turnNumber) {
            return false;
        }
    }

    // All ready -- collect commands
    outCommands = m_localCommands;
    for (const auto &peer : m_peers) {
        outCommands.insert(outCommands.end(), peer.turnCommands.begin(), peer.turnCommands.end());
    }

    // Reset for next turn
    m_localSubmitted = false;
    m_localCommands.clear();
    for (auto &peer : m_peers) {
        peer.hasSubmittedTurn = false;
        peer.turnCommands.clear();
    }

    // Broadcast the collected turn to all peers
    broadcastTurn(turnNumber, outCommands);

    return true;
}

void NetHost::acceptNewConnections()
{
    if (!m_listenSocket.isValid()) return;

    for (;;) {
        auto client = m_listenSocket.accept();
        if (!client) break;

        if (static_cast<int>(m_peers.size()) >= MaxPlayers - 1) {
            WARN << "Max players reached, rejecting connection";
            client->close();
            continue;
        }

        client->setNonBlocking();
        client->setNoDelay();

        Peer peer;
        peer.playerId = static_cast<int>(m_peers.size()) + 1; // Host is player 0
        peer.socket = std::move(client);

        // Send welcome message with assigned player ID
        std::vector<uint8_t> welcome;
        NetSer::writeU8(welcome, static_cast<uint8_t>(NetMsgType::LobbyJoin));
        NetSer::writeI32(welcome, peer.playerId);
        peer.socket->sendMessage(welcome);

        DBG << "Player" << peer.playerId << "connected";
        m_peers.push_back(std::move(peer));
    }
}

void NetHost::receiveFromPeers()
{
    for (size_t i = 0; i < m_peers.size(); ) {
        auto &peer = m_peers[i];
        if (!peer.socket || !peer.socket->isValid()) {
            removePeer(i);
            continue;
        }

        std::vector<uint8_t> payload;
        while (peer.socket->recvMessage(payload)) {
            handlePeerMessage(peer, payload);
            payload.clear();
        }

        // Check if socket disconnected during recv
        if (!peer.socket->isValid()) {
            removePeer(i);
            continue;
        }

        i++;
    }
}

void NetHost::handlePeerMessage(Peer &peer, const std::vector<uint8_t> &payload)
{
    if (payload.empty()) return;

    size_t offset = 0;
    NetMsgType msgType = static_cast<NetMsgType>(NetSer::readU8(payload, offset));

    switch (msgType) {
    case NetMsgType::TurnCommands: {
        uint32_t turnNum = NetSer::readU32(payload, offset);
        uint16_t cmdCount = NetSer::readU16(payload, offset);

        peer.turnCommands.clear();
        for (uint16_t i = 0; i < cmdCount; i++) {
            uint16_t cmdSize = NetSer::readU16(payload, offset);
            (void)cmdSize; // size is for framing; deserialize reads what it needs
            GameCommand cmd = GameCommand::deserialize(payload, offset);
            peer.turnCommands.push_back(std::move(cmd));
        }
        peer.hasSubmittedTurn = true;
        peer.submittedTurnNumber = turnNum;
        break;
    }

    case NetMsgType::Ping: {
        std::vector<uint8_t> pong;
        NetSer::writeU8(pong, static_cast<uint8_t>(NetMsgType::Pong));
        peer.socket->sendMessage(pong);
        break;
    }

    default:
        DBG << "NetHost: unhandled message type" << static_cast<int>(msgType) << "from player" << peer.playerId;
        break;
    }
}

void NetHost::removePeer(size_t index)
{
    if (index < m_peers.size()) {
        DBG << "Player" << m_peers[index].playerId << "disconnected";
        m_peers.erase(m_peers.begin() + index);
    }
}
