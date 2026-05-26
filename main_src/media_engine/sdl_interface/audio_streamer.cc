// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "media/audio_streamer.h"
#include "log_wrapper.h"

#include <SDL3/SDL.h>
#include <string>

namespace media_engine {

struct AudioStreamer::AudioStreamerImpl {
    SDL_AudioStream* stream = nullptr;
    bool opened = false;
};

AudioStreamer::AudioStreamer(int sample_rate, int channels)
    : impl_(std::make_unique<AudioStreamerImpl>()) {
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.freq = sample_rate;
    spec.format = SDL_AUDIO_S16LE;
    spec.channels = static_cast<Uint8>(channels);

    impl_->stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

    if (!impl_->stream) {
        LOG_ERROR("[AudioStreamer] SDL_OpenAudioDeviceStream failed: {}", SDL_GetError());
        return;
    }

    SDL_ResumeAudioStreamDevice(impl_->stream);
    impl_->opened = true;

    LOG_INFO("[AudioStreamer] opened: {} Hz, {} ch", sample_rate, channels);
}

AudioStreamer::~AudioStreamer() {
    if (impl_->stream) {
        SDL_DestroyAudioStream(impl_->stream);
    }
}

bool AudioStreamer::PushChunk(const uint8_t* data, size_t len) {
    if (!impl_->opened || !impl_->stream || !data || len == 0) {
        return false;
    }

    if (SDL_PutAudioStreamData(impl_->stream, data, static_cast<int>(len)) < 0) {
        LOG_ERROR("[AudioStreamer] SDL_PutAudioStreamData failed: {}", SDL_GetError());
        return false;
    }
    return true;
}

void AudioStreamer::Stop() {
    if (impl_->stream) {
        SDL_PauseAudioStreamDevice(impl_->stream);
        SDL_ClearAudioStream(impl_->stream);
    }
}

void AudioStreamer::Clear() {
    if (impl_->stream) {
        SDL_ClearAudioStream(impl_->stream);
    }
}

int AudioStreamer::QueuedBytes() const {
    if (!impl_->stream) return 0;
    return SDL_GetAudioStreamQueued(impl_->stream);
}

std::unique_ptr<AudioStreamer> AudioStreamer::PlayWav(const std::string& wav_path) {
    SDL_AudioSpec spec;
    Uint8* wav_data;
    Uint32 wav_len;
    if (!SDL_LoadWAV(wav_path.c_str(), &spec, &wav_data, &wav_len)) {
        LOG_ERROR("[AudioStreamer] SDL_LoadWAV failed: {}", SDL_GetError());
        return nullptr;
    }

    auto streamer = std::make_unique<AudioStreamer>(spec.freq, spec.channels);
    if (streamer->impl_->stream) {
        streamer->PushChunk(wav_data, wav_len);
    }
    SDL_free(wav_data);
    return streamer;
}

}  // namespace media_engine
