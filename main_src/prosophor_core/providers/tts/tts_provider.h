// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "common/noncopyable.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace prosophor {

/// Abstract base class for TTS backends
class TtsProvider : public Noncopyable {
 public:
    ~TtsProvider() = default;

    virtual std::string GetProviderName() const = 0;

    /// Synchronous synthesis: text → WAV file at output_path
    /// Returns output_path on success, empty string on failure
    virtual std::string Synthesize(const std::string& text,
                                   const std::string& output_path) = 0;

    /// Whether this provider supports streaming audio output
    virtual bool SupportsStreaming() const { return false; }

    /// Called once with audio format info before first chunk
    using OnStreamStarted = std::function<void(int sample_rate, int channels)>;
    /// Called for each raw PCM audio chunk
    using OnAudioChunk = std::function<void(const uint8_t* data, size_t len)>;

    /// Streaming synthesis (optional — default falls back to non-streaming)
    virtual void SynthesizeStream(const std::string& text,
                                  OnStreamStarted on_started,
                                  OnAudioChunk on_chunk);
};

}  // namespace prosophor
