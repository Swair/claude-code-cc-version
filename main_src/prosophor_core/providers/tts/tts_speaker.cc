// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/tts/tts_speaker.h"
#include "providers/tts/edge_tts_provider.h"
#include "providers/tts/gpt_sovits_provider.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"
#include "config/config.h"

#include <atomic>
#include <filesystem>
#include <thread>

namespace prosophor {

namespace {

constexpr const char* kTtsCacheDir = "assets/tts_cache/";

std::string MakeOutputPath() {
    static std::atomic<int> seq{0};
    seq++;
    return std::string(kTtsCacheDir) + "tts_" + std::to_string(seq) + ".wav";
}

}  // namespace

// ---- Singleton ----

TtsSpeaker& TtsSpeaker::GetInstance() {
    static TtsSpeaker instance;
    return instance;
}

// ---- Initialize ----

void TtsSpeaker::Initialize() {
    EnsureDirectory(kTtsCacheDir);
    // Load backend from config
    auto& config = ProsophorConfig::GetInstance();
    backend_ = config.tts.backend;
    GetOrCreateProvider();
    LOG_INFO("TtsSpeaker initialized (backend: {}).", backend_);
}

// ---- Backend selection ----

void TtsSpeaker::SetBackend(const std::string& name) {
    if (name == backend_ && provider_) return;
    backend_ = name;
    provider_.reset();
    LOG_INFO("TtsSpeaker: switched backend to {}", backend_);
}

TtsProvider* TtsSpeaker::GetOrCreateProvider() {
    if (provider_) return provider_.get();

    if (backend_ == "edge-tts") {
        auto edge = std::make_unique<EdgeTtsProvider>();
        edge->SetVoice(voice_);
        provider_ = std::move(edge);
    } else if (backend_ == "gpt-sovits") {
        auto& config = ProsophorConfig::GetInstance();
        auto gs = std::make_unique<GptSoVitsProvider>(config.tts.gs_url);
        if (!config.tts.gs_ref_audio_path.empty()) {
            gs->SetRefAudio(config.tts.gs_ref_audio_path,
                            config.tts.gs_ref_audio_text,
                            config.tts.gs_ref_audio_lang);
        }
        provider_ = std::move(gs);
    } else {
        LOG_ERROR("TtsSpeaker: unknown backend '{}', falling back to edge-tts", backend_);
        backend_ = "edge-tts";
        provider_ = std::make_unique<EdgeTtsProvider>();
    }

    return provider_.get();
}

// ---- Speak ----

void TtsSpeaker::Speak(const std::string& text) {
    if (text.empty() || speaking_) return;

    std::thread([this, text]() {
        SpeakAsync(text);
    }).detach();
}

void TtsSpeaker::SpeakAsync(const std::string& text) {
    speaking_ = true;

    auto* provider = GetOrCreateProvider();
    std::string output_path = MakeOutputPath();
    std::string result = provider->Synthesize(text, output_path);

    speaking_ = false;

    if (!result.empty() && on_synthesized_) {
        on_synthesized_(result);
    }
}

// ---- SpeakStream ----

void TtsSpeaker::SpeakStream(const std::string& text) {
    if (text.empty() || speaking_) return;

    std::thread([this, text]() {
        SpeakStreamAsync(text);
    }).detach();
}

void TtsSpeaker::SpeakStreamAsync(const std::string& text) {
    speaking_ = true;

    auto* provider = GetOrCreateProvider();
    if (!provider->SupportsStreaming()) {
        // Fallback: non-streaming
        SpeakAsync(text);
        return;
    }

    provider->SynthesizeStream(text,
        [this](int sr, int ch) { OnStreamStarted(sr, ch); },
        [this](const uint8_t* d, size_t len) { OnAudioChunk(d, len); });

    speaking_ = false;
}

// ---- Internal callbacks ----

void TtsSpeaker::OnStreamStarted(int sample_rate, int channels) {
    if (on_stream_started_) {
        on_stream_started_(sample_rate, channels);
    }
}

void TtsSpeaker::OnAudioChunk(const uint8_t* data, size_t len) {
    if (on_audio_chunk_) {
        on_audio_chunk_(data, len);
    }
}

// ---- Voice ----

void TtsSpeaker::SetVoice(const std::string& voice) {
    voice_ = voice;
    if (provider_ && backend_ == "edge-tts") {
        static_cast<EdgeTtsProvider*>(provider_.get())->SetVoice(voice);
    }
}

}  // namespace prosophor
