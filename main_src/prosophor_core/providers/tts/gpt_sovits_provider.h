// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "providers/tts/tts_provider.h"

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace prosophor {

/// GPT-SoVITS backend: HTTP API client for local GPT-SoVITS service
class GptSoVitsProvider : public TtsProvider {
 public:
    explicit GptSoVitsProvider(std::string api_url);

    std::string GetProviderName() const override { return "gpt-sovits"; }

    std::string Synthesize(const std::string& text,
                           const std::string& output_path) override;

    bool SupportsStreaming() const override { return true; }

    void SynthesizeStream(const std::string& text,
                          OnStreamStarted on_started,
                          OnAudioChunk on_chunk) override;

    // -- Configuration --

    void SetRefAudio(const std::string& path,
                     const std::string& text,
                     const std::string& lang = "zh");

    void SetTextLang(const std::string& lang) { text_lang_ = lang; }
    void SetApiUrl(const std::string& url) { api_url_ = url; }

    /// WAV header parser — extracts format info from first chunk
    struct WavHeaderInfo {
        int sample_rate = 0;
        int channels = 0;
        int bits_per_sample = 0;
        size_t data_offset = 44;

        bool Parse(const uint8_t* data, size_t len);
    };

 private:
    nlohmann::json BuildRequestBody(const std::string& text, bool streaming) const;

    std::string api_url_;
    std::string ref_audio_path_;
    std::string ref_audio_text_;
    std::string ref_audio_lang_ = "zh";
    std::string text_lang_ = "zh";

    static constexpr int kStreamingMode = 2;  // true streaming mode
    static constexpr int kRequestTimeoutSec = 120;
};

}  // namespace prosophor
