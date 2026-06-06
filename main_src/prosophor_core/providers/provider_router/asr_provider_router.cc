// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/provider_router/asr_provider_router.h"
#include "providers/asr/http_asr_provider.h"
#include "config/config.h"
#include "common/log_wrapper.h"
#include "common/thread_pool.h"

namespace prosophor {

// ---- Singleton ----

AsrProviderRouter& AsrProviderRouter::GetInstance() {
    static AsrProviderRouter instance;
    return instance;
}

// ---- Initialize ----

void AsrProviderRouter::Initialize() {
    LOG_INFO("AsrProviderRouter initializing...");

    // Read server URL from config
    auto& cfg = ProsophorConfig::GetInstance();
    if (!cfg.asr.server_url.empty()) {
        server_url_ = cfg.asr.server_url;
    }

    GetOrCreateProvider();
    LOG_INFO("AsrProviderRouter initialized (server={})", server_url_);
}

// ---- Provider ----

AsrProvider* AsrProviderRouter::GetOrCreateProvider() {
    if (provider_) return provider_.get();

    provider_ = std::make_unique<HttpAsrProvider>(server_url_);
    LOG_INFO("AsrProviderRouter: using HTTP ASR backend -> {}", server_url_);
    return provider_.get();
}

// ---- TranscribeFile (sync) ----

std::string AsrProviderRouter::TranscribeFile(const std::string& audio_path) {
    if (transcribing_.exchange(true)) {
        LOG_WARN("AsrProviderRouter: already transcribing, skipping: {}", audio_path);
        return {};
    }

    auto* provider = GetOrCreateProvider();
    LOG_INFO("AsrProviderRouter: transcribing: {}", audio_path);
    std::string result = provider->TranscribeFile(audio_path);

    transcribing_ = false;

    if (result.empty()) {
        LOG_ERROR("AsrProviderRouter: transcription failed: {}", audio_path);
        if (on_error_) {
            on_error_("transcription failed");
        }
        return {};
    }

    LOG_INFO("AsrProviderRouter: transcription done ({} chars)", result.size());
    if (on_result_) {
        on_result_(result);
    }
    return result;
}

// ---- TranscribePCM ----

std::string AsrProviderRouter::AsrProcess(const std::vector<int16_t>& pcm) {
    if (transcribing_.exchange(true)) {
        LOG_WARN("AsrProviderRouter: already transcribing, skipping PCM ({})", pcm.size());
        return {};
    }

    auto* provider = GetOrCreateProvider();
    LOG_DEBUG("AsrProviderRouter: transcribing PCM ({} samples)", pcm.size());
    std::string result = provider->AsrProcess(pcm);

    transcribing_ = false;

    if (result.empty()) {
        LOG_DEBUG("AsrProviderRouter: PCM transcription empty result");
        if (on_error_) {
            on_error_("PCM transcription failed");
        }
        return {};
    }

    LOG_INFO("AsrProviderRouter: PCM transcription done ({} chars)", result.size());
    if (on_result_) {
        on_result_(result);
    }
    return result;
}

// ---- TranscribeFileAsync ----

void AsrProviderRouter::TranscribeFileAsync(const std::string& audio_path) {
    LOG_INFO("AsrProviderRouter: async transcribe start: {}", audio_path);
    GetGlobalThreadPool().Submit([this, audio_path]() {
        TranscribeFile(audio_path);
    });
}

}  // namespace prosophor
