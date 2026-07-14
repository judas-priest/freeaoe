#pragma once

#include "core/SignalEmitter.h"

#include <stdint.h>
#include <array>
#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

struct ma_device;
struct sts_mixer_t;
struct sts_mixer_sample_t;

class AudioPlayer : public SignalReceiver
{
public:
    enum StandardSound {
        TributeSound
    };

    AudioPlayer();
    ~AudioPlayer();

    void playSound(const StandardSound id, const float pan = 0.f, const float volume = 1.f);

    void playSound(const int id, const int civilization, const float pan = 0.f, const float volume = 1.f);
    bool playStream(const std::string &filename);
    void playMidi(const std::string &filename);
    void stopStream(const std::string &filename);
    void tick(); // Call from game loop — drains queued streams

    /// Play a looping ambient sound. Only one ambient loop active at a time per slot.
    /// @param slot 0=water, 1=forest (max 2 ambient loops)
    /// @param soundId The DRS sound ID to loop
    /// @param civilization Civilization index for sound selection
    void startAmbientLoop(int slot, int soundId, int civilization);
    void stopAmbientLoop(int slot);
    void setAmbientVolume(int slot, float volume);

    static AudioPlayer &instance();

    void onSoundVolumeChanged();
    void onMusicVolumeChanged();

private:
    void playSample(const std::shared_ptr<uint8_t[]> &data, const float pan = 0.f, const float volume = 1.f);

    static void malCallback(ma_device *device, void *buffer, const void *, uint32_t frameCount);
    static void mp3Callback(sts_mixer_sample_t *sample, void *userdata);
    static void mp3StopCallback(const int id, sts_mixer_sample_t *sample, void *userdata);
    static void midiCallback(sts_mixer_sample_t *sample, void *userdata);
    static void midiStopCallback(const int id, sts_mixer_sample_t *sample, void *userdata);

    std::unique_ptr<sts_mixer_t> m_mixer;
    std::unique_ptr<ma_device> m_device;
    std::unordered_map<std::string, int> m_activeStreams;
    std::deque<std::string> m_streamQueue;
    std::atomic<bool> m_dialoguePlaying{false};
    std::mutex m_mutex;

    // Ambient sound state (max 2 slots: water + forest)
    struct AmbientSlot {
        int soundId = -1;
        int voiceId = -1;
        float volume = 0.f;
        bool active = false;
        std::shared_ptr<uint8_t[]> wavData; // prevent deallocation while playing
    };
    std::array<AmbientSlot, 2> m_ambientSlots;
};

