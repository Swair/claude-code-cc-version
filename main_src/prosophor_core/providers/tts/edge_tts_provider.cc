// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/tts/edge_tts_provider.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"
#include "platform/platform.h"

namespace prosophor {

namespace {

std::string ParentDir(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

}  // namespace

std::string EdgeTtsProvider::Synthesize(const std::string& text,
                                         const std::string& output_path) {
    EnsureDirectory(ParentDir(output_path));

    std::string cmd = "edge-tts"
        " -t " + Platform::ShellEscape(text) +
        " -v " + Platform::ShellEscape(voice_) +
        " --write-media " + Platform::ShellEscape(output_path);

    LOG_DEBUG("EdgeTtsProvider: running edge-tts ...");

    auto result = Platform::RunCommandWithOutput(cmd, 30);

    if (result.exit_code == -2) {
        LOG_WARN("EdgeTtsProvider: edge-tts timeout");
    } else if (result.exit_code != 0) {
        LOG_ERROR("EdgeTtsProvider: edge-tts exit code={}", result.exit_code);
    }

    if (FileExists(output_path)) {
        LOG_INFO("EdgeTtsProvider: synthesized {}", output_path);
        return output_path;
    }

    LOG_ERROR("EdgeTtsProvider: output file not found: {}", output_path);
    return {};
}

}  // namespace prosophor
