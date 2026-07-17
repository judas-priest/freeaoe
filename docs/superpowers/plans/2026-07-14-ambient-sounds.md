# Ambient Sounds Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add looping environmental ambient sounds (water, forest) that vary in volume based on visible terrain tiles.
**Architecture:** AudioPlayer gains `playAmbientLoop()` and `stopAmbientLoop()` methods using a dedicated mixer voice. The Engine's update loop counts visible water/forest tiles from MapRenderer's visible range and adjusts ambient volume each frame. AoE2's dat file contains water/forest sound IDs -- we use the wav assets from the DRS.
**Tech Stack:** C++, SDL2
---

## Task 1: Add Ambient Sound State to AudioPlayer

**File:** `src/audio/AudioPlayer.h`

Add new public methods and private state (after line 33, before `static AudioPlayer &instance()`):

```cpp
    /// Play a looping ambient sound. Only one ambient loop active at a time per slot.
    /// @param slot 0=water, 1=forest (max 2 ambient loops)
    /// @param soundId The DRS sound ID to loop
    /// @param volume 0.0 to 1.0
    void setAmbientVolume(int slot, float volume);
    void startAmbientLoop(int slot, int soundId, int civilization);
    void stopAmbientLoop(int slot);
```

Add private members (after line 54, before the closing `};`):

```cpp
    // Ambient sound state (max 2 slots: water + forest)
    struct AmbientSlot {
        int soundId = -1;
        int voiceId = -1;
        float volume = 0.f;
        bool active = false;
        sts_mixer_sample_t *sample = nullptr;
    };
    std::array<AmbientSlot, 2> m_ambientSlots;
```

Add `#include <array>` to the includes at the top of the file (after line 7).

## Task 2: Implement Ambient Loop Methods

**File:** `src/audio/AudioPlayer.cpp`

Add after `onMusicVolumeChanged()` (after line 651):

```cpp
void AudioPlayer::setAmbientVolume(int slot, float volume)
{
    if (slot < 0 || slot >= 2) return;
    m_ambientSlots[slot].volume = std::clamp(volume, 0.f, 1.f);

    if (m_ambientSlots[slot].active && m_ambientSlots[slot].voiceId >= 0) {
        std::lock_guard<std::mutex> guard(m_mutex);
        sts_mixer_set_voice_volume(m_mixer.get(), m_ambientSlots[slot].voiceId,
                                   m_ambientSlots[slot].volume);
    }
}

void AudioPlayer::startAmbientLoop(int slot, int soundId, int civilization)
{
    if (slot < 0 || slot >= 2) return;

    // Already playing this sound
    if (m_ambientSlots[slot].active && m_ambientSlots[slot].soundId == soundId) {
        return;
    }

    // Stop previous
    stopAmbientLoop(slot);

    const genie::Sound &sound = DataManager::Inst().getSound(soundId);
    if (sound.Items.empty()) {
        WARN << "No ambient sound items for id" << soundId;
        return;
    }

    // Pick first matching civilization item, or first available
    int wavId = -1;
    for (const genie::SoundItem &item : sound.Items) {
        if (item.Civilization == civilization || item.Civilization == -1) {
            wavId = item.ResourceID;
            break;
        }
    }
    if (wavId < 0 && !sound.Items.empty()) {
        wavId = sound.Items[0].ResourceID;
    }
    if (wavId < 0) {
        WARN << "No wav resource for ambient sound" << soundId;
        return;
    }

    std::shared_ptr<uint8_t[]> wavPtr = AssetManager::Inst()->getWavPtr(wavId);
    if (!wavPtr) {
        WARN << "Failed to load wav for ambient" << wavId;
        return;
    }

    WavHeader *header = reinterpret_cast<WavHeader*>(wavPtr.get());
    if (memcmp(wavPtr.get(), "RIFF", 4) != 0 || header->AudioFormat != WavHeader::PCM) {
        WARN << "Invalid ambient wav format";
        return;
    }
    if (header->NumChannels != 1) {
        WARN << "Ambient sound must be mono";
        return;
    }

    int audioFormat = STS_MIXER_SAMPLE_FORMAT_16;
    if (header->BitsPerSample == 8) audioFormat = STS_MIXER_SAMPLE_FORMAT_8;
    else if (header->BitsPerSample == 32) audioFormat = STS_MIXER_SAMPLE_FORMAT_32;

    sts_mixer_sample_t *sample = new sts_mixer_sample_t;
    sample->audio_format = audioFormat;
    sample->frequency = header->SampleRate;
    sample->length = (header->Subchunk2Size / (header->BitsPerSample/8)) - sizeof(WavHeader);
    sample->data = wavPtr;
    sample->audiodata = wavPtr.get() + sizeof(WavHeader);

    std::lock_guard<std::mutex> guard(m_mutex);
    int voiceId = sts_mixer_play_sample(m_mixer.get(), sample, m_ambientSlots[slot].volume, 1.0f, 0.f);
    if (voiceId < 0) {
        WARN << "Failed to play ambient sample";
        delete sample;
        return;
    }

    m_ambientSlots[slot].soundId = soundId;
    m_ambientSlots[slot].voiceId = voiceId;
    m_ambientSlots[slot].active = true;
    m_ambientSlots[slot].sample = sample;

    DBG << "Started ambient loop slot" << slot << "soundId" << soundId;
}

void AudioPlayer::stopAmbientLoop(int slot)
{
    if (slot < 0 || slot >= 2) return;
    if (!m_ambientSlots[slot].active) return;

    if (m_ambientSlots[slot].voiceId >= 0) {
        std::lock_guard<std::mutex> guard(m_mutex);
        sts_mixer_stop_voice(m_mixer.get(), m_ambientSlots[slot].voiceId);
    }

    m_ambientSlots[slot].active = false;
    m_ambientSlots[slot].soundId = -1;
    m_ambientSlots[slot].voiceId = -1;
    // sample is managed by the mixer after play
    m_ambientSlots[slot].sample = nullptr;
}
```

Note: `sts_mixer_set_voice_volume` may not exist in the current mixer. If it doesn't, Task 3 provides a fallback.

## Task 3: Add Volume Control to STS Mixer (if needed)

**File:** `src/audio/sts_mixer.h`

Check if `sts_mixer_set_voice_volume` exists. If not, add it:

```c
static void sts_mixer_set_voice_volume(sts_mixer_t *mixer, int voice, float volume)
{
    if (voice < 0 || voice >= STS_MIXER_VOICES) return;
    mixer->voices[voice].gain = volume;
}
```

If the mixer doesn't support per-voice volume control, the alternative approach is to stop and restart the sample at the new volume. In that case, modify `setAmbientVolume` to call `stopAmbientLoop` then `startAmbientLoop` when the volume changes significantly (delta > 0.1).

## Task 4: Count Visible Terrain Tiles in Engine Update

**File:** `src/Engine.cpp`

Add ambient terrain counting in the main game loop. Find the section where `m_mapRenderer->update(time)` is called (in the `start()` function's game loop). After the map renderer update, add ambient sound logic.

Add a new private method to Engine. In `src/Engine.h`, add after `void loadUiOverlay();` (line 106):

```cpp
    void updateAmbientSounds(const std::shared_ptr<GameState> &state);
```

Add private state in Engine.h (after line 245, `bool m_paused`):

```cpp
    Time m_lastAmbientUpdate = 0;
```

**File:** `src/Engine.cpp`

Add the implementation (after `loadUiOverlay()`):

```cpp
void Engine::updateAmbientSounds(const std::shared_ptr<GameState> &state)
{
    if (!m_mapRenderer || !state || !state->map()) return;

    // Only update every 500ms to avoid thrashing
    Time now = currentTimeMs();
    if (now - m_lastAmbientUpdate < 500) return;
    m_lastAmbientUpdate = now;

    int waterTiles = 0;
    int treeTiles = 0;
    int totalTiles = 0;

    const int colBegin = m_mapRenderer->firstVisibleColumn();
    const int colEnd = m_mapRenderer->lastVisibleColumn();
    const int rowBegin = m_mapRenderer->firstVisibleRow();
    const int rowEnd = m_mapRenderer->lastVisibleRow();

    const MapPtr &map = state->map();

    for (int col = colBegin; col < colEnd; col++) {
        for (int row = rowBegin; row < rowEnd; row++) {
            totalTiles++;
            const MapTile &tile = map->getTileAt(col, row);
            int id = tile.terrainId;
            // Water terrain IDs: 1=shallow, 2=medium, 3=deep, 4=ocean, 22=deep, 26=beach
            if (id == 1 || id == 2 || id == 3 || id == 4 || id == 22 || id == 26) {
                waterTiles++;
            }
            // Forest terrain IDs: 10=forest, 13=palm, 17=jungle, 18=bamboo, 19=pine
            if (id == 10 || id == 13 || id == 17 || id == 18 || id == 19) {
                treeTiles++;
            }
        }
    }

    if (totalTiles == 0) return;

    // Water ambient: sound 326 (water lapping)
    float waterRatio = static_cast<float>(waterTiles) / totalTiles;
    if (waterRatio > 0.05f) {
        float vol = std::clamp(waterRatio * 0.5f, 0.05f, 0.3f);
        AudioPlayer::instance().startAmbientLoop(0, 326, 0);
        AudioPlayer::instance().setAmbientVolume(0, vol);
    } else {
        AudioPlayer::instance().stopAmbientLoop(0);
    }

    // Forest ambient: sound 330 (birds/forest)
    float treeRatio = static_cast<float>(treeTiles) / totalTiles;
    if (treeRatio > 0.05f) {
        float vol = std::clamp(treeRatio * 0.4f, 0.05f, 0.25f);
        AudioPlayer::instance().startAmbientLoop(1, 330, 0);
        AudioPlayer::instance().setAmbientVolume(1, vol);
    } else {
        AudioPlayer::instance().stopAmbientLoop(1);
    }
}
```

Then call `updateAmbientSounds(state)` in the game loop after the map renderer update. In `Engine::start()`, find where `m_mapRenderer->update(time)` is called and add after it:

```cpp
        updateAmbientSounds(state);
```

## Task 5: Add Required Includes

**File:** `src/Engine.cpp`

Ensure these includes are present (most already are):
- `#include "audio/AudioPlayer.h"` (already at line 22)
- `#include "mechanics/MapTile.h"` (add if not present)

**File:** `src/audio/AudioPlayer.cpp`

Add at top if not present:
```cpp
#include "resource/DataManager.h"  // already present
```

## Summary of Changes

| File | Change |
|------|--------|
| `src/audio/AudioPlayer.h` | Add `startAmbientLoop()`, `stopAmbientLoop()`, `setAmbientVolume()`, `AmbientSlot` struct |
| `src/audio/AudioPlayer.cpp` | Implement ambient loop methods |
| `src/audio/sts_mixer.h` | Add `sts_mixer_set_voice_volume()` if missing |
| `src/Engine.h` | Add `updateAmbientSounds()` method, `m_lastAmbientUpdate` field |
| `src/Engine.cpp` | Implement terrain tile counting and ambient volume scaling |
