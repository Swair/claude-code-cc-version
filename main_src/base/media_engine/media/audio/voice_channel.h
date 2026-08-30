// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "noncopyable.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace media_engine {

class AudioCapture;
class AudioStreamer;

/// Global audio I/O pump: reads mic on CaptureHandler, drains PlaybackHandler.
/// Owns the audio thread, AudioCapture, and AudioStreamer.
class VoiceChannel : public prosophor::Noncopyable {
public:
    static constexpr int kSampleRate = 16000;
    static constexpr int kChannels = 1;
    static constexpr int kFrameSize = 480; // 30ms @ 16kHz

    using CaptureHandler = std::function<void(const int16_t* data, size_t samples)>;

    static VoiceChannel& GetInstance();

    /// Directly push PCM data to the audio output streamer.
    void PlayAudio(const std::vector<int16_t>& pcm);
    /// Initialize capture independently. Safe to call multiple times.
    void InitCapture(CaptureHandler on_capture);
    /// Enable/disable microphone capture on a running channel.
    void EnableCapture(bool on);
    void Stop();
    bool IsRunning() const { return running_; }

private:
    VoiceChannel();
    ~VoiceChannel();
    void AudioLoop();

    std::unique_ptr<AudioCapture> capture_;
    std::unique_ptr<AudioStreamer> streamer_;

    CaptureHandler on_capture_;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::mutex start_mtx_;
};

}  // namespace media_engine
