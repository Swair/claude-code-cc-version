// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "providers/asr/asr_provider.h"

#include <string>
#include <vector>

namespace prosophor {

/// HTTP client for whisper_server.
/// Sends PCM data to a remote whisper.cpp server for transcription.
class HttpAsrProvider : public AsrProvider {
 public:
    explicit HttpAsrProvider(std::string server_url);

    std::string GetProviderName() const override { return "http-whisper"; }

    /// Transcribe a WAV file by reading PCM and sending to server
    std::string TranscribeFile(const std::string& audio_path) override;

    /// Transcribe PCM int16 mono 16kHz samples
    std::string AsrProcess(const std::vector<int16_t>& pcm) override;

 private:
    std::string server_url_;
};

}  // namespace prosophor
