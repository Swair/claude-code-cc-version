// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "common/noncopyable.h"

#include <cstdint>
#include <string>
#include <vector>

namespace prosophor {

/// Abstract base class for ASR backends
class AsrProvider : public Noncopyable {
 public:
    virtual ~AsrProvider() = default;

    virtual std::string GetProviderName() const = 0;

    /// Synchronous transcription: audio file → recognized text
    /// Returns transcribed text on success, empty string on failure
    virtual std::string TranscribeFile(const std::string& audio_path) = 0;

    /// Direct PCM transcription: int16 mono 16kHz samples → text
    /// Used for streaming/real-time ASR where PCM is already in memory.
    virtual std::string AsrProcess(const std::vector<int16_t>& pcm) = 0;
};

}  // namespace prosophor
