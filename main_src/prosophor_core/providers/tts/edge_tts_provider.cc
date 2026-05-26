// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/tts/edge_tts_provider.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"
#include "platform/platform.h"

#include <cstdio>
#include <cstring>

namespace prosophor {

namespace {

std::string ParentDir(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

/// Check if a file has a valid WAV RIFF header
bool IsWavFile(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    char header[12] = {};
    bool ok = (std::fread(header, 1, 12, fp) == 12)
           && std::memcmp(header, "RIFF", 4) == 0
           && std::memcmp(header + 8, "WAVE", 4) == 0;
    std::fclose(fp);
    return ok;
}

/// Convert non-WAV audio to WAV via ffmpeg (in-place)
bool ConvertToWav(const std::string& path) {
    std::string tmp = path + ".conv.wav";
    std::string cmd = "ffmpeg -y -i " + Platform::ShellEscape(path) +
                      " -acodec pcm_s16le -ar 24000 -ac 1 " +
                      Platform::ShellEscape(tmp) + " 2>&1";
    std::string ffmpeg_output = Platform::RunShellCommand(cmd.c_str());

    if (!FileExists(tmp) || FileSize(tmp) == 0) {
        LOG_ERROR("EdgeTtsProvider: ffmpeg conversion failed. Output:\n{}",
                  ffmpeg_output.empty() ? "(empty — ffmpeg not in PATH?)" : ffmpeg_output);
        RemoveFile(tmp);
        return false;
    }

    RemoveFile(path);
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        LOG_ERROR("EdgeTtsProvider: failed to rename converted WAV");
        RemoveFile(tmp);
        return false;
    }
    return true;
}

}  // namespace

std::string EdgeTtsProvider::Synthesize(const std::string& text,
                                         const std::string& output_path) {
    EnsureDirectory(ParentDir(output_path));

    // Write text to temp file to avoid encoding issues with non-ASCII
    // characters passing through cmd.exe on Windows (via _popen).
    std::string text_file = output_path + ".txt";
    WriteFile(text_file, text, false);

    std::string cmd = "edge-tts"
        " -f " + Platform::ShellEscape(text_file) +
        " -v " + Platform::ShellEscape(voice_) +
        " --write-media " + Platform::ShellEscape(output_path);

    LOG_DEBUG("EdgeTtsProvider: running edge-tts ...");

    std::string output = Platform::RunShellCommand(cmd.c_str());
    RemoveFile(text_file);

    if (!FileExists(output_path) || FileSize(output_path) == 0) {
        LOG_ERROR("EdgeTtsProvider: output file missing or empty after edge-tts. Output:\n{}",
                  output.empty() ? "(empty command output)" : output);
        RemoveFile(output_path);
        return {};
    }

    // edge-tts outputs WebM/Opus from the Microsoft TTS service, not WAV.
    // Verify WAV header and convert if needed.
    if (!IsWavFile(output_path)) {
        LOG_WARN("EdgeTtsProvider: output is not WAV format, attempting ffmpeg conversion...");
        if (!ConvertToWav(output_path)) {
            RemoveFile(output_path);
            return {};
        }
        LOG_INFO("EdgeTtsProvider: converted to WAV ({} bytes)", FileSize(output_path));
    }

    LOG_INFO("EdgeTtsProvider: synthesized {} ({} bytes)", output_path, FileSize(output_path));
    return output_path;
}

}  // namespace prosophor
