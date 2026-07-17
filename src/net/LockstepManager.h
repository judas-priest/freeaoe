#pragma once

#include "GameCommand.h"
#include "NetHost.h"
#include "NetClient.h"

#include <cstdint>
#include <memory>
#include <vector>

/// Manages lockstep turn advancement for multiplayer synchronization.
///
/// Turn duration = 200ms.
/// Command delay = 2 turns (commands from turn N execute at turn N+2).
/// In single-player mode, networking is bypassed and commands execute immediately
/// with the same 2-turn delay for consistent behavior.
class LockstepManager
{
public:
    static constexpr int TurnDurationMs = 200;
    static constexpr int CommandDelay = 2;

    LockstepManager();
    ~LockstepManager();

    /// Set the host for hosting a game. Mutually exclusive with setClient.
    void setHost(std::shared_ptr<NetHost> host);

    /// Set the client for joining a game. Mutually exclusive with setHost.
    void setClient(std::shared_ptr<NetClient> client);

    /// Set single-player mode (bypass networking).
    void setSinglePlayer();

    /// Called every frame. Advances turns based on elapsed time.
    /// Submits/receives commands via the network.
    void update(uint32_t timeMs);

    /// Queue a local command for the next submission.
    void addCommand(const GameCommand &cmd);

    /// True if all commands for the current turn have been received.
    bool isReadyToAdvance() const;

    /// Get the commands to execute for the current turn.
    std::vector<GameCommand> commandsForCurrentTurn() const;

    /// Mark the current turn as consumed and advance.
    void advanceTurn();

    /// Get the current turn number.
    uint32_t currentTurn() const { return m_currentTurn; }

    /// The player slot this instance controls (0 for host, assigned by host for clients).
    void setLocalPlayerId(int id) { m_localPlayerId = id; }
    int localPlayerId() const { return m_localPlayerId; }

    /// Whether this is running in multiplayer mode (host or client).
    bool isMultiplayer() const { return m_mode != Mode::SinglePlayer; }

    /// Whether this instance is the host.
    bool isHost() const { return m_mode == Mode::Host; }

private:
    void submitLocalCommands();
    void receiveRemoteCommands();

    enum class Mode {
        SinglePlayer,
        Host,
        Client
    };

    Mode m_mode = Mode::SinglePlayer;

    std::shared_ptr<NetHost> m_host;
    std::shared_ptr<NetClient> m_client;

    uint32_t m_currentTurn = 0;
    uint32_t m_lastUpdateTime = 0;
    uint32_t m_turnAccumulator = 0;

    // Commands queued locally for the upcoming submission turn (currentTurn + CommandDelay)
    std::vector<GameCommand> m_pendingCommands;

    // Commands received and ready for execution, indexed by turn number
    struct TurnData {
        uint32_t turnNumber = 0;
        std::vector<GameCommand> commands;
        bool received = false;
    };

    // Circular buffer of received turns (we only need a small window)
    static constexpr size_t TurnBufferSize = 16;
    TurnData m_turnBuffer[TurnBufferSize];

    // Track which turn we last submitted commands for
    uint32_t m_lastSubmittedTurn = 0;
    bool m_started = false;

    // Player slot this instance controls
    int m_localPlayerId = 0;
};
