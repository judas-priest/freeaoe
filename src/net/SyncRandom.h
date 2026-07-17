#pragma once

#include <cstdint>

/// Deterministic pseudo-random number generator for multiplayer synchronization.
/// All game-state code must use this instead of rand() to ensure identical
/// simulation across all players.
class SyncRandom
{
public:
    explicit SyncRandom(uint32_t seed = 12345) : m_state(seed) {}

    /// Generate the next pseudo-random 32-bit value (xorshift32).
    uint32_t next()
    {
        m_state ^= m_state << 13;
        m_state ^= m_state >> 17;
        m_state ^= m_state << 5;
        return m_state;
    }

    /// Return a random integer in [0, max).
    int nextInt(int max)
    {
        if (max <= 0) return 0;
        return static_cast<int>(next() % static_cast<uint32_t>(max));
    }

    /// Return a random float in [0, 1).
    float nextFloat()
    {
        return static_cast<float>(next()) / static_cast<float>(0xFFFFFFFF);
    }

    /// Current internal state (useful for sync checksums).
    uint32_t state() const { return m_state; }

    /// Re-seed the generator.
    void seed(uint32_t s) { m_state = s; }

    /// Global singleton instance used by all game-state code.
    static SyncRandom &inst()
    {
        static SyncRandom s_instance;
        return s_instance;
    }

private:
    uint32_t m_state;
};
