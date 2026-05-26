// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/asr/asr_service.h"
#include "providers/asr/sensevoice_asr_provider.h"
#include "config/config.h"
#include "common/log_wrapper.h"

#include <thread>

namespace prosophor {

// ---- Singleton ----

AsrService& AsrService::GetInstance() {
    static AsrService instance;
    return instance;
}

// ---- Initialize ----

void AsrService::Initialize() {
    LOG_INFO("AsrService initializing...");
    // Default paths if not set
    if (script_path_.empty()) {
        script_path_ = "assets/asr/run_asr.py";
    }
    if (model_dir_.empty()) {
        model_dir_ = "assets/asr";
    }
    GetOrCreateProvider();
    LOG_INFO("AsrService initialized (script={}, model_dir={})",
             script_path_, model_dir_);
}

// ---- Provider ----

AsrProvider* AsrService::GetOrCreateProvider() {
    if (provider_) return provider_.get();

    // TODO: add sherpa-onnx provider
    provider_ = std::make_unique<SenseVoiceAsrProvider>(script_path_, model_dir_);
    LOG_INFO("AsrService: using {} backend", provider_->GetProviderName());
    return provider_.get();
}

// ---- Transcribe (sync) ----

std::string AsrService::Transcribe(const std::string& audio_path) {
    transcribing_ = true;

    auto* provider = GetOrCreateProvider();
    LOG_INFO("AsrService: transcribing: {}", audio_path);
    std::string result = provider->Transcribe(audio_path);

    transcribing_ = false;

    if (result.empty()) {
        LOG_ERROR("AsrService: transcription failed: {}", audio_path);
        if (on_error_) {
            on_error_("transcription failed");
        }
        return {};
    }

    LOG_INFO("AsrService: transcription done ({} chars)", result.size());
    if (on_result_) {
        on_result_(result);
    }
    return result;
}

// ---- Transcribe (async) ----

void AsrService::TranscribeAsync(const std::string& audio_path) {
    if (transcribing_) {
        LOG_WARN("AsrService: already transcribing, skipping: {}", audio_path);
        if (on_error_) {
            on_error_("already transcribing");
        }
        return;
    }

    LOG_INFO("AsrService: async transcribe start: {}", audio_path);
    std::thread([this, audio_path]() {
        Transcribe(audio_path);
    }).detach();
}

}  // namespace prosophor
