// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "rtc/webrtc_vad.h"

#include <cstdint>
#include <vector>

namespace prosophor {

/// WebRTC VAD wrapper — frame-level voice activity detection.
///
/// Delegates to WebRTC VAD (GMM-based, mode 3 by default).
class VadProcessor {
public:
    static constexpr int kSampleRate = 16000;
    static constexpr int kFrameSize  = 480;  // 30ms @ 16kHz

    VadProcessor();
    ~VadProcessor();

    /// Process one frame (480 int16 samples @ 16kHz).
    /// Returns true if voice activity is detected.
    bool ProcessFrame(const int16_t* data, size_t samples);

    /// Set VAD aggressiveness threshold (0=quality ~ 3=very aggressive).
    void SetThreshold(float level);

    /// Reset internal state (call on capture restart).
    void Reset();

private:
    VadInst* handle_ = nullptr;
    int mode_ = 3;
};

}  // namespace prosophor
