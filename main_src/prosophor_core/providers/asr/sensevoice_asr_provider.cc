// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/asr/sensevoice_asr_provider.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"
#include "platform/platform.h"

#include <nlohmann/json.hpp>

namespace prosophor {

SenseVoiceAsrProvider::SenseVoiceAsrProvider(std::string script_path,
                                             std::string model_dir)
    : script_path_(std::move(script_path))
    , model_dir_(std::move(model_dir)) {}

std::string SenseVoiceAsrProvider::Transcribe(const std::string& audio_path) {
    if (!FileExists(audio_path)) {
        LOG_ERROR("SenseVoiceAsrProvider: audio file not found: {}", audio_path);
        return {};
    }

    // python run_asr.py <audio_path> <model_dir>
    std::string cmd = "python " + Platform::ShellEscape(script_path_)
                    + " " + Platform::ShellEscape(audio_path)
                    + " " + Platform::ShellEscape(model_dir_);

    LOG_DEBUG("SenseVoiceAsrProvider: running: {}", cmd);

    std::string output = Platform::RunShellCommand(cmd.c_str());

    if (output.empty()) {
        LOG_ERROR("SenseVoiceAsrProvider: empty output for: {}", audio_path);
        return {};
    }

    try {
        auto json = nlohmann::json::parse(output);
        if (json.contains("error")) {
            LOG_ERROR("SenseVoiceAsrProvider: error: {}", json["error"].get<std::string>());
            return {};
        }
        std::string text = json["text"].get<std::string>();
        LOG_INFO("SenseVoiceAsrProvider: transcribed {} chars from {}", text.size(), audio_path);
        return text;
    } catch (const std::exception& e) {
        LOG_ERROR("SenseVoiceAsrProvider: failed to parse output: {}. raw: {}", e.what(), output);
        return {};
    }
}

}  // namespace prosophor
