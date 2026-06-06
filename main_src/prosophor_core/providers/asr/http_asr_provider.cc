// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/asr/http_asr_provider.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"
#include "network/curl_client.h"

#include <cstring>
#include <vector>

namespace prosophor {

HttpAsrProvider::HttpAsrProvider(std::string server_url)
    : server_url_(std::move(server_url)) {
    // Strip trailing slash
    if (!server_url_.empty() && server_url_.back() == '/') {
        server_url_.pop_back();
    }
}

std::string HttpAsrProvider::TranscribeFile(const std::string& audio_path) {
    auto pcm = prosophor::LoadWav(audio_path);
    if (pcm.empty()) {
        LOG_ERROR("HttpAsrProvider: failed to load WAV: {}", audio_path);
        return {};
    }
    return AsrProcess(pcm);
}

std::string HttpAsrProvider::AsrProcess(const std::vector<int16_t>& pcm) {
    if (pcm.empty()) return {};

    std::string url = server_url_ + "/v1/transcribe";

    HttpRequest req;
    req.url = url;
    req.body.assign(reinterpret_cast<const char*>(pcm.data()),
                    pcm.size() * sizeof(int16_t));
    req.timeout_seconds = 60;

    // Set content type for raw PCM
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/octet-stream");
    req.headers = headers;

    auto resp = HttpClient::Instance().Post(req);
    if (headers) curl_slist_free_all(headers);

    if (!resp.success()) {
        LOG_ERROR("HttpAsrProvider: HTTP {}: {}",
                  resp.status_code, resp.error_msg);
        return {};
    }

    // Parse JSON response
    try {
        auto j = nlohmann::json::parse(resp.body);
        std::string text = j.value("text", "");
        LOG_INFO("HttpAsrProvider: recognized {} chars", text.size());
        return text;
    } catch (const std::exception& e) {
        LOG_ERROR("HttpAsrProvider: JSON parse failed: {}", e.what());
        return {};
    }
}

}  // namespace prosophor
