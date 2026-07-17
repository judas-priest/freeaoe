#include "LockstepManager.h"
#include "core/Logger.h"

LockstepManager::LockstepManager()
{
    // Initialize turn buffer
    for (size_t i = 0; i < TurnBufferSize; i++) {
        m_turnBuffer[i] = {};
    }
}

LockstepManager::~LockstepManager() = default;

void LockstepManager::setHost(std::shared_ptr<NetHost> host)
{
    m_mode = Mode::Host;
    m_host = std::move(host);
    m_client.reset();
}

void LockstepManager::setClient(std::shared_ptr<NetClient> client)
{
    m_mode = Mode::Client;
    m_client = std::move(client);
    m_host.reset();
}

void LockstepManager::setSinglePlayer()
{
    m_mode = Mode::SinglePlayer;
    m_host.reset();
    m_client.reset();
}

void LockstepManager::update(uint32_t timeMs)
{
    if (!m_started) {
        m_lastUpdateTime = timeMs;
        m_started = true;

        // Pre-fill the first CommandDelay turns so the game can start
        for (uint32_t t = 0; t < static_cast<uint32_t>(CommandDelay); t++) {
            size_t idx = t % TurnBufferSize;
            m_turnBuffer[idx].turnNumber = t;
            m_turnBuffer[idx].commands.clear();
            m_turnBuffer[idx].received = true;
        }
        return;
    }

    // Update network
    if (m_mode == Mode::Host && m_host) {
        m_host->update();
    } else if (m_mode == Mode::Client && m_client) {
        m_client->update();
    }

    // Accumulate time for turn advancement
    uint32_t elapsed = timeMs - m_lastUpdateTime;
    m_lastUpdateTime = timeMs;
    m_turnAccumulator += elapsed;

    // Submit commands for future turns as time progresses
    while (m_turnAccumulator >= static_cast<uint32_t>(TurnDurationMs)) {
        m_turnAccumulator -= TurnDurationMs;
        submitLocalCommands();
    }

    // Receive commands from remote players
    receiveRemoteCommands();
}

void LockstepManager::addCommand(const GameCommand &cmd)
{
    m_pendingCommands.push_back(cmd);
}

bool LockstepManager::isReadyToAdvance() const
{
    size_t idx = m_currentTurn % TurnBufferSize;
    return m_turnBuffer[idx].received && m_turnBuffer[idx].turnNumber == m_currentTurn;
}

std::vector<GameCommand> LockstepManager::commandsForCurrentTurn() const
{
    size_t idx = m_currentTurn % TurnBufferSize;
    if (m_turnBuffer[idx].received && m_turnBuffer[idx].turnNumber == m_currentTurn) {
        return m_turnBuffer[idx].commands;
    }
    return {};
}

void LockstepManager::advanceTurn()
{
    size_t idx = m_currentTurn % TurnBufferSize;
    m_turnBuffer[idx].received = false;
    m_currentTurn++;
}

void LockstepManager::submitLocalCommands()
{
    uint32_t submitTurn = m_lastSubmittedTurn;
    // The commands we submit now will be executed at submitTurn + CommandDelay
    uint32_t executionTurn = submitTurn + CommandDelay;

    switch (m_mode) {
    case Mode::SinglePlayer: {
        // In single-player, directly place commands into the turn buffer
        size_t idx = executionTurn % TurnBufferSize;
        m_turnBuffer[idx].turnNumber = executionTurn;
        m_turnBuffer[idx].commands = m_pendingCommands;
        m_turnBuffer[idx].received = true;
        break;
    }

    case Mode::Host: {
        if (m_host) {
            m_host->submitLocalCommands(submitTurn, m_pendingCommands);
        }
        break;
    }

    case Mode::Client: {
        if (m_client) {
            m_client->sendCommands(submitTurn, m_pendingCommands);
        }
        break;
    }
    }

    m_pendingCommands.clear();
    m_lastSubmittedTurn++;
}

void LockstepManager::receiveRemoteCommands()
{
    if (m_mode == Mode::Host && m_host) {
        // Try to collect completed turns
        // Check a window of turns ahead
        for (uint32_t t = m_currentTurn; t < m_lastSubmittedTurn + CommandDelay; t++) {
            size_t idx = t % TurnBufferSize;
            if (m_turnBuffer[idx].received && m_turnBuffer[idx].turnNumber == t) {
                continue; // Already have this turn
            }

            // The host collects turn (t - CommandDelay) which maps to execution turn t
            uint32_t collectTurn = (t >= static_cast<uint32_t>(CommandDelay)) ? t - CommandDelay : 0;
            std::vector<GameCommand> collected;
            if (m_host->collectTurn(collectTurn, collected)) {
                m_turnBuffer[idx].turnNumber = t;
                m_turnBuffer[idx].commands = std::move(collected);
                m_turnBuffer[idx].received = true;
            }
        }
    } else if (m_mode == Mode::Client && m_client) {
        // Pop received turn bundles from the client
        uint32_t turnNumber;
        std::vector<GameCommand> commands;
        while (m_client->popTurn(turnNumber, commands)) {
            // turnNumber from the host is the submission turn; execution turn = turnNumber + CommandDelay
            uint32_t executionTurn = turnNumber + CommandDelay;
            size_t idx = executionTurn % TurnBufferSize;
            m_turnBuffer[idx].turnNumber = executionTurn;
            m_turnBuffer[idx].commands = std::move(commands);
            m_turnBuffer[idx].received = true;
        }
    }
}
