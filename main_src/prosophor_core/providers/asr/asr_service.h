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

/// AsrService: singleton facade for speech-to-text
/// Routes to SenseVoice ASR backend
class AsrService : public Noncopyable {
 public:
    static AsrService& GetInstance();

    /// Initialize — creates the ASR provider
    void Initialize();

    /// Set paths (call before Initialize)
    void SetScriptPath(const std::string& path) { script_path_ = path; }
    void SetModelDir(const std::string& dir) { model_dir_ = dir; }

    /// Synchronous transcription
    std::string Transcribe(const std::string& audio_path);

    /// Asynchronous transcription with callback
    void TranscribeAsync(const std::string& audio_path);

    /// Whether transcription is in progress
    bool IsTranscribing() const { return transcribing_; }

    // -- Callbacks --
    using OnResultCallback = std::function<void(const std::string& text)>;
    using OnErrorCallback = std::function<void(const std::string& error)>;

    void SetOnResult(OnResultCallback cb) { on_result_ = std::move(cb); }
    void SetOnError(OnErrorCallback cb) { on_error_ = std::move(cb); }

 private:
    AsrService() = default;

    AsrProvider* GetOrCreateProvider();

    std::string script_path_;
    std::string model_dir_;
    std::unique_ptr<AsrProvider> provider_;

    OnResultCallback on_result_;
    OnErrorCallback on_error_;

    std::atomic<bool> transcribing_{false};
};

}  // namespace prosophor
