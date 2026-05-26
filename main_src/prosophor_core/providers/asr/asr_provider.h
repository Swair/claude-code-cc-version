// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "common/noncopyable.h"

#include <string>

namespace prosophor {

/// Abstract base class for ASR backends
class AsrProvider : public Noncopyable {
 public:
    virtual ~AsrProvider() = default;

    virtual std::string GetProviderName() const = 0;

    /// Synchronous transcription: audio file → recognized text
    /// Returns transcribed text on success, empty string on failure
    virtual std::string Transcribe(const std::string& audio_path) = 0;
};

}  // namespace prosophor
