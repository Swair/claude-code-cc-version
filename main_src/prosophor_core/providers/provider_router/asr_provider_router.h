// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "common/noncopyable.h"
#include "providers/asr/asr_provider.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace prosophor {

/// AsrProviderRouter: singleton facade for speech-to-text
/// Routes to HTTP ASR server backend
class AsrProviderRouter : public Noncopyable {
 public:
    static AsrProviderRouter& GetInstance();

    /// Initialize — creates the ASR HTTP client
    void Initialize();

    /// Set server URL (call before Initialize)
    void SetServerUrl(const std::string& url) { server_url_ = url; }

    /// Direct PCM transcription: int16 mono 16kHz samples → text
    std::string AsrProcess(const std::vector<int16_t>& pcm);

    /// Synchronous transcription (audio file)
    std::string TranscribeFile(const std::string& audio_path);

    /// Transcribe an audio file asynchronously (fire-and-forget thread).
    void TranscribeFileAsync(const std::string& audio_path);

    /// Whether transcription is in progress
    bool IsTranscribing() const { return transcribing_; }

    /// Raw ASR provider pointer
    AsrProvider* GetRawProvider() { return provider_.get(); }

    // -- Callbacks --
    using OnResultCallback = std::function<void(const std::string& text)>;
    using OnErrorCallback = std::function<void(const std::string& error)>;

    void SetOnResult(OnResultCallback cb) { on_result_ = std::move(cb); }
    void SetOnError(OnErrorCallback cb) { on_error_ = std::move(cb); }

 private:
    AsrProviderRouter() = default;

    AsrProvider* GetOrCreateProvider();

    std::string server_url_ = "http://127.0.0.1:9100";
    std::unique_ptr<AsrProvider> provider_;

    OnResultCallback on_result_;
    OnErrorCallback on_error_;

    std::atomic<bool> transcribing_{false};
};

}  // namespace prosophor
