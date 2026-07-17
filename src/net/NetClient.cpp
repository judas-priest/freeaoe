#include "NetClient.h"
#include "core/Logger.h"

NetClient::NetClient() = default;

NetClient::~NetClient()
{
    disconnect();
}

bool NetClient::connect(const std::string &host, uint16_t port)
{
    if (!m_socket.connect(host, port)) {
        WARN << "NetClient: failed to connect to" << host << ":" << port;
        return false;
    }

    m_socket.setNonBlocking();
    m_socket.setNoDelay();

    DBG << "NetClient: connected to" << host << ":" << port;
    return true;
}

void NetClient::disconnect()
{
    m_socket.close();
    m_playerId = -1;
    m_receivedTurns.clear();
}

void NetClient::sendCommands(uint32_t turnNumber, const std::vector<GameCommand> &commands)
{
    if (!m_socket.isValid()) return;

    // Build payload: [msgType:1][turnNumber:4][cmdCount:2][cmd0...cmdN]
    std::vector<uint8_t> payload;
    NetSer::writeU8(payload, static_cast<uint8_t>(NetMsgType::TurnCommands));
    NetSer::writeU32(payload, turnNumber);
    NetSer::writeU16(payload, static_cast<uint16_t>(commands.size()));
    for (const auto &cmd : commands) {
        std::vector<uint8_t> cmdData = cmd.serialize();
        NetSer::writeU16(payload, static_cast<uint16_t>(cmdData.size()));
        payload.insert(payload.end(), cmdData.begin(), cmdData.end());
    }

    if (!m_socket.sendMessage(payload)) {
        WARN << "NetClient: failed to send commands for turn" << turnNumber;
    }
}

void NetClient::update()
{
    if (!m_socket.isValid()) return;

    std::vector<uint8_t> payload;
    while (m_socket.recvMessage(payload)) {
        handleMessage(payload);
        payload.clear();
    }
}

bool NetClient::popTurn(uint32_t &outTurnNumber, std::vector<GameCommand> &outCommands)
{
    if (m_receivedTurns.empty()) return false;

    auto &front = m_receivedTurns.front();
    outTurnNumber = front.turnNumber;
    outCommands = std::move(front.commands);
    m_receivedTurns.pop_front();
    return true;
}

void NetClient::handleMessage(const std::vector<uint8_t> &payload)
{
    if (payload.empty()) return;

    size_t offset = 0;
    NetMsgType msgType = static_cast<NetMsgType>(NetSer::readU8(payload, offset));

    switch (msgType) {
    case NetMsgType::LobbyJoin: {
        // Welcome message from host with our assigned player ID
        m_playerId = NetSer::readI32(payload, offset);
        DBG << "NetClient: assigned player ID" << m_playerId;
        break;
    }

    case NetMsgType::TurnAck: {
        // Turn bundle from host
        uint32_t turnNumber = NetSer::readU32(payload, offset);
        uint16_t cmdCount = NetSer::readU16(payload, offset);

        TurnBundle bundle;
        bundle.turnNumber = turnNumber;
        for (uint16_t i = 0; i < cmdCount; i++) {
            uint16_t cmdSize = NetSer::readU16(payload, offset);
            (void)cmdSize;
            GameCommand cmd = GameCommand::deserialize(payload, offset);
            bundle.commands.push_back(std::move(cmd));
        }
        m_receivedTurns.push_back(std::move(bundle));
        break;
    }

    case NetMsgType::Pong:
        // Handle ping response if needed
        break;

    default:
        DBG << "NetClient: unhandled message type" << static_cast<int>(msgType);
        break;
    }
}
