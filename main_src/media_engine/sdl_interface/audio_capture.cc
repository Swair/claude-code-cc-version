// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "media/audio_capture.h"
#include "log_wrapper.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <string>

namespace media_engine {

struct AudioCapture::Impl {
    SDL_AudioStream* stream = nullptr;
};

AudioCapture::AudioCapture()
    : impl_(std::make_unique<Impl>()) {}

AudioCapture::~AudioCapture() {
    if (capturing_) { Stop(); }
}

bool AudioCapture::Start() {
    if (capturing_) {
        LOG_WARN("[AudioCapture] already capturing");
        return false;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = 16000;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = 1;

    impl_->stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &spec, nullptr, nullptr);

    if (!impl_->stream) {
        LOG_ERROR("[AudioCapture] SDL_OpenAudioDeviceStream failed: {}", SDL_GetError());
        if (on_error_) { on_error_(std::string("SDL_OpenAudioDeviceStream: ") + SDL_GetError()); }
        return false;
    }

    SDL_ResumeAudioStreamDevice(impl_->stream);
    capturing_ = true;

    LOG_INFO("[AudioCapture] started continuous capture");
    return true;
}

void AudioCapture::Stop() {
    if (!capturing_) return;
    capturing_ = false;

    if (impl_->stream) {
        SDL_PauseAudioStreamDevice(impl_->stream);
        SDL_ClearAudioStream(impl_->stream);
        SDL_DestroyAudioStream(impl_->stream);
        impl_->stream = nullptr;
    }
    LOG_INFO("[AudioCapture] stopped");
}

int AudioCapture::Read(int16_t* buf, int n_samples) {
    if (!impl_->stream || !capturing_) return 0;

    int need = n_samples * (int)sizeof(int16_t);
    int waited = 0;
    while (capturing_) {
        int avail = SDL_GetAudioStreamAvailable(impl_->stream);
        if (avail >= need) break;
        SDL_Delay(5);
        waited += 5;
        if (waited > 3000) { // 3s timeout
            LOG_WARN("[AudioCapture] Read timeout after {}ms", waited);
            return 0;
        }
    }
    if (!capturing_) return 0;

    int got = SDL_GetAudioStreamData(impl_->stream, buf, need);
    if (got <= 0) return 0;
    return got / (int)sizeof(int16_t);
}

}  // namespace media_engine
