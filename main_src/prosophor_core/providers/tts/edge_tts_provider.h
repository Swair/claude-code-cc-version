// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "providers/tts/tts_provider.h"

#include <string>
#include <memory>
#include <cstdint>
#include <vector>
#include <functional>

namespace prosophor {

/// WebSocket-based Microsoft Edge TTS provider
/// Directly connects to speech.platform.bing.com, no Python/CLI needed.
/// Supports both streaming (Opus → PCM) and file-based synthesis.
class EdgeTtsProvider : public TtsProvider {
 public:
    EdgeTtsProvider();
    ~EdgeTtsProvider() override;

    std::string GetProviderName() const override { return "edge-tts"; }

    /// Synthesize full text to a WAV file (non-streaming)
    std::string Synthesize(const std::string& text,
                           const std::string& output_path) override;

    /// Supports true streaming via WebSocket + Opus → PCM
    bool SupportsStreaming() const override { return true; }

    /// Streaming synthesis: real-time PCM via WebSocket
    void SynthesizeStream(const std::string& text,
                          OnStreamStarted on_started,
                          OnAudioChunk on_chunk) override;

    void SetVoice(const std::string& voice) { voice_ = voice; }
    const std::string& GetVoice() const { return voice_; }

 private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string voice_ = "zh-CN-XiaoxiaoNeural";
};

}  // namespace prosophor