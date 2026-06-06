// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "media/voice_channel.h"
#include "media/audio_capture.h"
#include "media/audio_streamer.h"
#include "log_wrapper.h"

#include <chrono>

namespace media_engine {

VoiceChannel::VoiceChannel() {
    running_ = true;
    thread_ = std::thread(&VoiceChannel::AudioLoop, this);
    LOG_INFO("[VoiceChannel] audio thread started");
}

VoiceChannel& VoiceChannel::GetInstance() {
    static VoiceChannel instance;
    return instance;
}

VoiceChannel::~VoiceChannel() {
    Stop();
}

void VoiceChannel::PlayAudio(const std::vector<int16_t>& pcm) {
    if (!streamer_) {
        streamer_ = std::make_unique<AudioStreamer>(kSampleRate, kChannels);
    }
    streamer_->PlayAudio(pcm);
}

void VoiceChannel::InitCapture(CaptureHandler on_capture) {
    std::lock_guard<std::mutex> lock(start_mtx_);
    on_capture_ = std::move(on_capture);
    if (capture_) return;

    capture_ = std::make_unique<AudioCapture>();
    if (!capture_->Start()) {
        LOG_WARN("[VoiceChannel] AudioCapture unavailable");
        capture_.reset();
        on_capture_ = nullptr;
    }
}

void VoiceChannel::EnableCapture(bool on) {
    std::lock_guard<std::mutex> lock(start_mtx_);
    if (on) {
        if (capture_) return;
        capture_ = std::make_unique<AudioCapture>();
        if (!capture_->Start()) {
            LOG_WARN("[VoiceChannel] AudioCapture unavailable");
            capture_.reset();
            return;
        }
        LOG_INFO("[VoiceChannel] capture enabled");
    } else {
        on_capture_ = nullptr;
        if (capture_) {
            capture_->Stop();
            capture_.reset();
        }
        LOG_INFO("[VoiceChannel] capture disabled");
    }
}

void VoiceChannel::Stop() {
    std::lock_guard<std::mutex> lock(start_mtx_);
    if (!running_) return;

    running_ = false;
    if (thread_.joinable()) thread_.join();

    if (capture_) {
        capture_->Stop();
        capture_.reset();
    }
    if (streamer_) streamer_.reset();

    on_capture_ = nullptr;

    LOG_INFO("[VoiceChannel] stopped");
}

void VoiceChannel::AudioLoop() {
    alignas(16) int16_t cap_frame[kFrameSize];

    LOG_INFO("[VoiceChannel] audio loop started");

    while (running_) {
        if (capture_) {
            int got = capture_->Read(cap_frame, kFrameSize);
            if (got > 0 && on_capture_) on_capture_(cap_frame, got);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

}  // namespace media_engine
