// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace prosophor {

/// In-process MP3 decoder wrapper around dr_mp3.
struct Mp3Decoder {
    void* impl = nullptr;  // opaque drmp3 pointer

    /// Initialize decoder from raw MP3 data in memory.
    /// Returns true on success.
    bool Init(const uint8_t* data, size_t size);

    /// Get total PCM frame count.
    uint64_t GetPcmFrameCount();

    /// Get the sample rate of the decoded audio.
    int SampleRate();

    /// Get the number of channels.
    int Channels();

    /// Read all PCM frames into output vector (int16).
    /// Returns frames actually read.
    uint64_t ReadAllS16(std::vector<int16_t>& out);

    /// Uninitialize and free decoder.
    void Uninit();
};

}  // namespace prosophor
