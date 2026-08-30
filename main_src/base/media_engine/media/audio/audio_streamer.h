// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace media_engine {

/// Streaming audio player using SDL_AudioStream
/// Pushes raw PCM chunks for gapless playback
class AudioStreamer {
 public:
    /// @param sample_rate  e.g. 24000, 48000
    /// @param channels     1 = mono, 2 = stereo
    AudioStreamer(int sample_rate, int channels);
    ~AudioStreamer();

    AudioStreamer(const AudioStreamer&) = delete;
    AudioStreamer& operator=(const AudioStreamer&) = delete;
    AudioStreamer(AudioStreamer&&) noexcept = default;
    AudioStreamer& operator=(AudioStreamer&&) noexcept = default;

    /// Push raw PCM audio data for playback
    bool PlayAudio(const std::vector<int16_t>& pcm);

    /// Stop playback and clear all queued data
    void Stop();

    /// Clear queued data without stopping the device
    void Clear();

    /// Number of bytes still queued for playback
    int QueuedBytes() const;

 private:
    struct AudioStreamerImpl;
    std::unique_ptr<AudioStreamerImpl> impl_;
};

}  // namespace media_engine
