// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

// DRMP3_API=static gives all dr_mp3 symbols internal linkage, avoiding
// "multiple definition" conflicts with SDL3_mixer's built-in dr_mp3.
//
// The #pragma is needed because DRMP3_API=static triggers
// -Wunused-function for dr_mp3 functions we don't call directly.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define DRMP3_API static
#define DR_MP3_IMPLEMENTATION
#include "common/dr_mp3.h"
#pragma GCC diagnostic pop

#include "common/mp3_decoder.h"

namespace prosophor {

bool Mp3Decoder::Init(const uint8_t* data, size_t size) {
    auto* mp3 = new drmp3();
    if (!drmp3_init_memory(mp3, data, size, nullptr)) {
        delete mp3;
        return false;
    }
    impl = mp3;
    return true;
}

uint64_t Mp3Decoder::GetPcmFrameCount() {
    return impl ? drmp3_get_pcm_frame_count(static_cast<drmp3*>(impl)) : 0;
}

int Mp3Decoder::SampleRate() {
    return impl ? static_cast<int>(static_cast<drmp3*>(impl)->sampleRate) : 0;
}

int Mp3Decoder::Channels() {
    return impl ? static_cast<int>(static_cast<drmp3*>(impl)->channels) : 0;
}

uint64_t Mp3Decoder::ReadAllS16(std::vector<int16_t>& out) {
    if (!impl) return 0;
    auto* mp3 = static_cast<drmp3*>(impl);
    auto frames = drmp3_get_pcm_frame_count(mp3);
    if (frames == 0) return 0;
    out.resize(static_cast<size_t>(frames) * mp3->channels);
    return drmp3_read_pcm_frames_s16(mp3, frames, out.data());
}

void Mp3Decoder::Uninit() {
    if (impl) {
        drmp3_uninit(static_cast<drmp3*>(impl));
        delete static_cast<drmp3*>(impl);
        impl = nullptr;
    }
}

}  // namespace prosophor
