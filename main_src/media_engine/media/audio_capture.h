// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace media_engine {

/// AudioCapture: continuous microphone capture using SDL3 audio.
/// Uses an internal ring buffer (like whisper's audio_async).
/// Read audio via Read() — no push callback needed.
class AudioCapture {
 public:
    AudioCapture();
    ~AudioCapture();

    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;
    AudioCapture(AudioCapture&&) noexcept = default;
    AudioCapture& operator=(AudioCapture&&) noexcept = default;

    bool Start();
    void Stop();
    bool IsCapturing() const { return capturing_; }

    /// Read exactly n_samples from the microphone. Blocks until available.
    /// Returns n_samples on success, 0 on timeout or error.
    int Read(int16_t* buf, int n_samples);

    void SetOnError(std::function<void(const std::string&)> cb) { on_error_ = std::move(cb); }

    static constexpr int kRingBufferMs = 10000;  // 10 second ring buffer
    static constexpr int kSampleRate = 16000;

 private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool capturing_ = false;
    std::function<void(const std::string&)> on_error_;
};

}  // namespace media_engine
