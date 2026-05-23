// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/tts/tts_provider.h"
#include "common/file_utils.h"

#include <cstdint>
#include <fstream>
#include <vector>

namespace prosophor {

/// Default streaming fallback: do full synthesis then read file in chunks
void TtsProvider::SynthesizeStream(const std::string& text,
                                    OnStreamStarted on_started,
                                    OnAudioChunk on_chunk) {
    std::string cache_dir = "assets/tts_cache/";
    EnsureDirectory(cache_dir);
    std::string tmp_path = cache_dir + "tts_stream_tmp.wav";

    std::string result = Synthesize(text, tmp_path);
    if (result.empty()) return;

    std::ifstream file(result, std::ios::binary);
    if (!file) return;

    // Read header to get format info
    uint8_t header[44];
    file.read(reinterpret_cast<char*>(header), 44);
    if (file.gcount() < 44) return;

    // Parse WAV header
    int sample_rate = *reinterpret_cast<const int*>(header + 24);
    int channels = header[22];
    if (on_started) on_started(sample_rate, channels);

    // Read and forward raw PCM data (after header)
    file.seekg(44);
    std::vector<uint8_t> buf(4096);
    while (file) {
        file.read(reinterpret_cast<char*>(buf.data()), buf.size());
        std::streamsize got = file.gcount();
        if (got > 0 && on_chunk) {
            on_chunk(buf.data(), static_cast<size_t>(got));
        }
    }
}

}  // namespace prosophor
