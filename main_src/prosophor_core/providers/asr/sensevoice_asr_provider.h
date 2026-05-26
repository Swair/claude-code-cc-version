// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "providers/asr/asr_provider.h"

#include <string>

namespace prosophor {

/// SenseVoice ASR backend: subprocess call to funasr Python inference
class SenseVoiceAsrProvider : public AsrProvider {
 public:
    explicit SenseVoiceAsrProvider(std::string script_path,
                                   std::string model_dir);

    std::string GetProviderName() const override { return "sensevoice"; }

    std::string Transcribe(const std::string& audio_path) override;

 private:
    std::string script_path_;  // path to run_asr.py
    std::string model_dir_;     // path to asr/models/ directory
};

}  // namespace prosophor
