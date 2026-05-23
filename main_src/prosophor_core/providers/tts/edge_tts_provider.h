// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "providers/tts/tts_provider.h"

#include <string>

namespace prosophor {

/// edge-tts backend: subprocess call to Microsoft Edge TTS
class EdgeTtsProvider : public TtsProvider {
 public:
    std::string GetProviderName() const override { return "edge-tts"; }

    std::string Synthesize(const std::string& text,
                           const std::string& output_path) override;

    bool SupportsStreaming() const override { return false; }

    void SetVoice(const std::string& voice) { voice_ = voice; }
    const std::string& GetVoice() const { return voice_; }

 private:
    std::string voice_ = "zh-CN-XiaoxiaoNeural";
};

}  // namespace prosophor
