// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/tts/gpt_sovits_provider.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"
#include "network/curl_client.h"

#include <curl/curl.h>
#include <fstream>

namespace prosophor {
namespace {

struct StreamCtx {
    GptSoVitsProvider::OnStreamStarted on_started;
    GptSoVitsProvider::OnAudioChunk on_chunk;
    bool header_parsed = false;
};

std::string ParentDir(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

}  // namespace

// CURL write callback for streaming binary chunks
size_t gs_stream_write(char* contents, size_t size, size_t nmemb, void* userp) {
    auto* ctx = static_cast<StreamCtx*>(userp);
    size_t total = size * nmemb;
    auto* data = reinterpret_cast<const uint8_t*>(contents);

    if (!ctx->header_parsed) {
        GptSoVitsProvider::WavHeaderInfo header;
        if (header.Parse(data, total)) {
            if (ctx->on_started) {
                ctx->on_started(header.sample_rate, header.channels);
            }
            size_t pcm_len = total - header.data_offset;
            if (pcm_len > 0 && ctx->on_chunk) {
                ctx->on_chunk(data + header.data_offset, pcm_len);
            }
            ctx->header_parsed = true;
        }
        return total;
    }

    if (ctx->on_chunk) {
        ctx->on_chunk(data, total);
    }
    return total;
}

// ---- Constructor & Config ----

GptSoVitsProvider::GptSoVitsProvider(std::string api_url)
    : api_url_(std::move(api_url)) {}

void GptSoVitsProvider::SetRefAudio(const std::string& path,
                                     const std::string& text,
                                     const std::string& lang) {
    ref_audio_path_ = path;
    ref_audio_text_ = text;
    ref_audio_lang_ = lang;
}

// ---- Request builder ----

nlohmann::json GptSoVitsProvider::BuildRequestBody(const std::string& text,
                                                    bool streaming) const {
    nlohmann::json body;
    body["text"] = text;
    body["text_lang"] = text_lang_;
    body["media_type"] = "wav";

    if (streaming) {
        body["streaming_mode"] = kStreamingMode;
    } else {
        body["streaming_mode"] = false;
    }

    if (!ref_audio_path_.empty()) {
        body["ref_audio_path"] = ref_audio_path_;
        body["prompt_text"] = ref_audio_text_;
        body["prompt_lang"] = ref_audio_lang_;
    }

    return body;
}

// ---- Non-streaming ----

std::string GptSoVitsProvider::Synthesize(const std::string& text,
                                           const std::string& output_path) {
    nlohmann::json j = BuildRequestBody(text, false);
    std::string body_str = j.dump();

    HeaderList headers;
    headers.append("Content-Type: application/json");

    HttpRequest req;
    req.url = api_url_ + "/tts";
    req.body = body_str;
    req.headers = headers.get();
    req.timeout_seconds = kRequestTimeoutSec;

    HttpResponse resp = HttpClient::Instance().Post(req);
    if (!resp.success()) {
        LOG_ERROR("GptSoVitsProvider: HTTP {} — {}",
                  resp.status_code, resp.error_msg);
        return {};
    }

    EnsureDirectory(ParentDir(output_path));
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        LOG_ERROR("GptSoVitsProvider: cannot write {}", output_path);
        return {};
    }
    out.write(resp.body.data(), static_cast<std::streamsize>(resp.body.size()));
    out.close();

    LOG_INFO("GptSoVitsProvider: synthesized {} ({} bytes)", output_path, resp.body.size());
    return output_path;
}

// ---- Streaming ----

void GptSoVitsProvider::SynthesizeStream(const std::string& text,
                                          OnStreamStarted on_started,
                                          OnAudioChunk on_chunk) {
    nlohmann::json j = BuildRequestBody(text, true);
    std::string body_str = j.dump();

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("GptSoVitsProvider: curl_easy_init failed");
        return;
    }

    std::string url = api_url_ + "/tts";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());

    HeaderList headers;
    headers.append("Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get());

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(kRequestTimeoutSec));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

    StreamCtx ctx{std::move(on_started), std::move(on_chunk), false};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, gs_stream_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    LOG_DEBUG("GptSoVitsProvider: starting streaming TTS...");
    CURLcode res = curl_easy_perform(curl);

    long status_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        LOG_ERROR("GptSoVitsProvider: streaming request failed: {}", curl_easy_strerror(res));
    } else if (status_code < 200 || status_code >= 300) {
        LOG_ERROR("GptSoVitsProvider: streaming HTTP {}", status_code);
    } else {
        LOG_DEBUG("GptSoVitsProvider: streaming TTS completed");
    }
}

// ---- WAV header parser ----

bool GptSoVitsProvider::WavHeaderInfo::Parse(const uint8_t* data, size_t len) {
    if (len < 44) return false;

    if (data[0] != 'R' || data[1] != 'I' || data[2] != 'F' || data[3] != 'F') {
        return false;
    }
    if (data[8] != 'W' || data[9] != 'A' || data[10] != 'V' || data[11] != 'E') {
        return false;
    }

    channels       = data[22];
    sample_rate    = *reinterpret_cast<const int*>(data + 24);
    bits_per_sample = *reinterpret_cast<const short*>(data + 34);

    data_offset = 12;
    while (data_offset + 8 <= len) {
        uint32_t chunk_size = *reinterpret_cast<const uint32_t*>(data + data_offset + 4);
        if (data[data_offset] == 'd' && data[data_offset + 1] == 'a' &&
            data[data_offset + 2] == 't' && data[data_offset + 3] == 'a') {
            data_offset += 8;
            return true;
        }
        data_offset += 8 + chunk_size;
    }

    data_offset = 44;
    return true;
}

}  // namespace prosophor
