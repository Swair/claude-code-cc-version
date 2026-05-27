// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/tts/edge_tts_provider.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"
#include "platform/platform.h"

#include <curl/curl.h>
#include <openssl/evp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace prosophor {

namespace {

constexpr const char* kEdgeHost = "speech.platform.bing.com";
constexpr const char* kEdgePath = "/consumer/speech/synthesize/readaloud/edge/v1";
constexpr const char* kTrustedClientToken = "6A5AA1D4EAFF4E9FB37E23D68491D6F4";
constexpr const char* kGecVersion = "1-143.0.3650.75";
constexpr int kSampleRate = 24000;
constexpr int kChannels = 1;

// ffmpeg candidates
constexpr const char* kFfmpegCandidates[] = {
    "ffmpeg",
    "/e/devtool/ffmpeg-master-latest-win64-gpl-shared/bin/ffmpeg",
};

// ---- DRM token generation (matches Python edge-tts exactly) ----

std::string Sha256Hex(const std::string& input) {
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int len = 0;
    EVP_Digest(input.data(), input.size(), digest.data(), &len, EVP_sha256(), nullptr);
    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i) {
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string GenerateGecToken() {
    using namespace std::chrono;
    // Python: dt.now(tz.utc).timestamp() + clock_skew_seconds
    auto now = system_clock::now().time_since_epoch();
    double ticks = duration_cast<duration<double>>(now).count();
    // Python: ticks += WIN_EPOCH (11644473600)
    ticks += 11644473600.0;
    // Round down to nearest 5 minutes
    ticks -= std::fmod(ticks, 300.0);
    // Convert to 100-nanosecond intervals: ticks *= 1e9 / 100 = 1e7
    // Python: f"{ticks:.0f}{TRUSTED_CLIENT_TOKEN}"
    uint64_t filetime = static_cast<uint64_t>(ticks * 10000000.0);
    return Sha256Hex(std::to_string(filetime) + kTrustedClientToken);
}

std::string GenerateUuid() {
    std::array<uint8_t, 16> bytes;
    std::random_device rd;
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = rd();
        std::memcpy(bytes.data() + i, &r, 4);
    }
    bytes[6] = (bytes[6] & 0x0F) | 0x40;
    bytes[8] = (bytes[8] & 0x3F) | 0x80;

    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

/// Generate a random MUID (Machine Unique ID) — 32 uppercase hex chars
/// Used as a cookie to satisfy Microsoft's anti-abuse / rate-limiting.
std::string GenerateMuid() {
    std::array<uint8_t, 16> bytes;
    std::random_device rd;
    for (int i = 0; i < 16; i += 4) {
        uint32_t r = rd();
        std::memcpy(bytes.data() + i, &r, 4);
    }
    std::ostringstream oss;
    for (int i = 0; i < 16; ++i) {
        oss << std::uppercase << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(bytes[i]);
    }
    return oss.str();
}

// Python-style date: "Wed May 27 2026 10:30:00 GMT+0000 (Coordinated Universal Time)"
std::string DateToString() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm* gmt = std::gmtime(&t);
    char buf[128];
    // Python: time.strftime("%a %b %d %Y %H:%M:%S GMT+0000 (Coordinated Universal Time)", time.gmtime())
    std::strftime(buf, sizeof(buf), "%a %b %d %Y %H:%M:%S GMT+0000 (Coordinated Universal Time)", gmt);
    return buf;
}

std::string BuildSsml(const std::string& text, const std::string& voice) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&':  escaped += "&amp;";  break;
            case '<':  escaped += "&lt;";   break;
            case '>':  escaped += "&gt;";   break;
            case '"':  escaped += "&quot;"; break;
            case '\'': escaped += "&apos;"; break;
            default:   escaped += c;        break;
        }
    }
    return "<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='en-US'>"
           "<voice name='" + voice + "'>"
           "<prosody pitch='+0Hz' rate='+0%' volume='+0%'>"
           + escaped +
           "</prosody></voice></speak>";
}

// Build speech.config message (sent BEFORE SSML)
std::string BuildSpeechConfig(const std::string& date) {
    return "X-Timestamp:" + date + "\r\n"
           "Content-Type:application/json; charset=utf-8\r\n"
           "Path:speech.config\r\n\r\n"
           R"({"context":{"synthesis":{"audio":{"metadataoptions":{)"
           R"("sentenceBoundaryEnabled":"false","wordBoundaryEnabled":"true"},)"
           R"("outputFormat":"audio-24khz-48kbitrate-mono-mp3"}}}})";
}

// Build SSML message with proper headers (matching Python edge-tts)
std::string BuildSsmlMessage(const std::string& ssml, const std::string& request_id,
                              const std::string& date) {
    return "X-RequestId:" + request_id + "\r\n"
           "Content-Type:application/ssml+xml\r\n"
           "X-Timestamp:" + date + "Z\r\n"  // trailing Z is a known Microsoft Edge bug
           "Path:ssml\r\n\r\n"
           + ssml;
}

std::string ParentDir(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

// ---- Find ffmpeg ----

std::string FindFfmpeg() {
    std::string null_dev = Platform::NullDevice();
    for (const char* candidate : kFfmpegCandidates) {
        int ret = std::system((std::string(candidate) + " -version >" + null_dev + " 2>&1").c_str());
        if (ret == 0) return candidate;
    }
    return {};
}

// ---- Decode MP3 file to WAV via ffmpeg ----

bool DecodeMp3ToWav(const std::string& ffmpeg, const std::string& mp3_path,
                     const std::string& wav_path) {
    std::string null_dev = Platform::NullDevice();
    std::string cmd = ffmpeg + " -y -i \"" + mp3_path + "\""
        + " -acodec pcm_s16le -ar " + std::to_string(kSampleRate)
        + " -ac " + std::to_string(kChannels)
        + " \"" + wav_path + "\""
        + " >" + null_dev + " 2>&1";
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        LOG_ERROR("EdgeTts: ffmpeg decode failed (ret={}) mp3={}", ret, mp3_path);
        return false;
    }
    if (!FileExists(wav_path) || FileSize(wav_path) == 0) {
        LOG_ERROR("EdgeTts: ffmpeg produced empty WAV: {}", wav_path);
        return false;
    }
    return true;
}

}  // namespace

// ---- PIMPL ----

struct EdgeTtsProvider::Impl {
    /// Connect WebSocket, send speech.config, send SSML.
    /// Returns CURL* on success, nullptr on failure.
    CURL* ConnectAndExchange(const std::string& text, const std::string& voice) {
        std::lock_guard<std::mutex> lock(ws_mutex_);

        std::string gec = GenerateGecToken();
        std::string conn_id = GenerateUuid();
        std::string url = "wss://" + std::string(kEdgeHost) + kEdgePath
            + "?TrustedClientToken=" + kTrustedClientToken
            + "&Sec-MS-GEC=" + gec
            + "&Sec-MS-GEC-Version=" + kGecVersion
            + "&ConnectionId=" + conn_id;

        CURL* curl = curl_easy_init();
        if (!curl) {
            LOG_ERROR("EdgeTts: curl_easy_init failed");
            return nullptr;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf_);
        errbuf_[0] = 0;

        // Headers matching Python edge-tts: BASE_HEADERS + WSS_HEADERS
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers,
            "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
            " (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36 Edg/143.0.0.0");
        headers = curl_slist_append(headers, "Accept-Encoding: gzip, deflate, br, zstd");
        headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
        headers = curl_slist_append(headers, "Pragma: no-cache");
        headers = curl_slist_append(headers, "Cache-Control: no-cache");
        headers = curl_slist_append(headers,
            "Origin: chrome-extension://jdiccldimpdaibmpdkjnbmckianbfold");
        headers = curl_slist_append(headers, "Sec-WebSocket-Version: 13");
        // Microsoft now requires a muid cookie for rate-limiting / anti-abuse
        std::string muid_header = "Cookie: muid=" + GenerateMuid() + ";";
        headers = curl_slist_append(headers, muid_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        if (headers) curl_slist_free_all(headers);
        if (res != CURLE_OK) {
            LOG_ERROR("EdgeTts: WebSocket connect failed: {} (errbuf: {})",
                      curl_easy_strerror(res), errbuf_);
            curl_easy_cleanup(curl);
            return nullptr;
        }

        // Step 1: Send speech.config
        std::string date_str = DateToString();
        std::string config_msg = BuildSpeechConfig(date_str);
        size_t sent = 0;
        res = curl_ws_send(curl, config_msg.data(), config_msg.size(), &sent, 0, CURLWS_TEXT);
        if (res != CURLE_OK) {
            LOG_ERROR("EdgeTts: failed to send speech.config: {} (errbuf: {})",
                      curl_easy_strerror(res), errbuf_);
            curl_easy_cleanup(curl);
            return nullptr;
        }
        LOG_DEBUG("EdgeTts: speech.config sent ({} bytes)", sent);

        // Step 2: Send SSML with headers
        std::string ssml = BuildSsml(text, voice);
        std::string req_id = GenerateUuid();
        std::string ssml_msg = BuildSsmlMessage(ssml, req_id, date_str);
        sent = 0;
        res = curl_ws_send(curl, ssml_msg.data(), ssml_msg.size(), &sent, 0, CURLWS_TEXT);
        if (res != CURLE_OK) {
            LOG_ERROR("EdgeTts: failed to send SSML: {} (errbuf: {})",
                      curl_easy_strerror(res), errbuf_);
            curl_easy_cleanup(curl);
            return nullptr;
        }
        LOG_INFO("EdgeTts: connected and exchanged ({} chars text, {} B SSML msg)", text.size(), sent);
        return curl;
    }

    /// Retry curl_ws_recv on CURLE_AGAIN until data arrives or timeout.
    /// Some proxy / network conditions may delay the response.
    CURLcode WaitForRecv(CURL* curl, uint8_t* buf, size_t bufsize,
                          size_t* received, const struct curl_ws_frame** meta,
                          int max_wait_ms = 5000) const {
        int waited = 0;
        constexpr int kSleepMs = 50;
        while (waited < max_wait_ms) {
            CURLcode res = curl_ws_recv(curl, buf, bufsize, received, meta);
            if (res == CURLE_AGAIN) {
                std::this_thread::sleep_for(std::chrono::milliseconds(kSleepMs));
                waited += kSleepMs;
                continue;
            }
            return res;
        }
        LOG_WARN("EdgeTts: recv timeout after {} ms", max_wait_ms);
        return CURLE_OPERATION_TIMEDOUT;
    }

    /// Drain text frames until binary (audio) data starts arriving.
    /// Buffers the first binary frame if received.
    void DrainMetadata(CURL* curl) {
        constexpr size_t kBufSize = 16384;
        std::vector<uint8_t> buf(kBufSize);

        while (true) {
            size_t received = 0;
            const struct curl_ws_frame* meta = nullptr;
            CURLcode res = WaitForRecv(curl, buf.data(), buf.size(), &received, &meta);
            if (res != CURLE_OK) return;
            if (received == 0) continue;

            if (meta && (meta->flags & CURLWS_BINARY)) {
                // Store first binary frame
                first_binary_frame_.assign(buf.data(), buf.data() + received);
                return;
            }
            if (meta && (meta->flags & CURLWS_TEXT)) {
                buf[received] = '\0';
                LOG_DEBUG("EdgeTts: metadata: {}", reinterpret_cast<const char*>(buf.data()));
                // Check for turn.end — empty response
                if (std::strstr(reinterpret_cast<const char*>(buf.data()), "turn.end")) {
                    LOG_WARN("EdgeTts: received turn.end without audio");
                    return;
                }
                continue;
            }
            if (meta && (meta->flags & CURLWS_CLOSE)) {
                LOG_WARN("EdgeTts: WebSocket closed during drain");
                return;
            }
        }
    }

    /// Extract MP4/audio payload from an Edge TTS binary frame.
    /// Binary format: first 2 bytes = header length (big-endian),
    /// then headers, then \r\n\r\n, then audio data.
    /// Returns the audio payload, or empty vector on failure.
    static std::vector<uint8_t> ExtractAudioPayload(const uint8_t* data, size_t len) {
        if (len < 2) return {};
        uint16_t header_len = static_cast<uint16_t>(data[0]) << 8 | data[1];
        if (static_cast<size_t>(header_len) + 2 > len) {
            LOG_WARN("EdgeTts: header len {} exceeds frame size {}", header_len, len);
            return {};
        }
        // Find \r\n\r\n after header
        const uint8_t* payload_start = data + 2 + header_len;
        // Skip past \r\n\r\n
        if (payload_start + 4 > data + len) return {};
        // Edge sends \r\n\r\n after headers — skip past it
        const uint8_t* body = payload_start;
        // \r\n\r\n is the separator
        if (len >= 4 && body[0] == '\r' && body[1] == '\n') {
            body += 2;
        }
        if (len >= 4 && body[0] == '\r' && body[1] == '\n') {
            body += 2;
        }
        size_t body_len = (data + len) - body;
        if (body_len == 0) return {};

        return {body, body + body_len};
    }

    /// Receive all binary frames, accumulate MP3 data, call callback for each chunk.
    /// Returns total bytes of MP3 data received.
    size_t ReceiveMp3(CURL* curl,
                       const std::function<void(const uint8_t*, size_t)>& mp3_callback) {
        constexpr size_t kBufSize = 16384;
        std::vector<uint8_t> buf(kBufSize);
        size_t total_bytes = 0;

        // Process buffered first frame
        if (!first_binary_frame_.empty()) {
            auto payload = ExtractAudioPayload(first_binary_frame_.data(),
                                                first_binary_frame_.size());
            if (!payload.empty()) {
                mp3_callback(payload.data(), payload.size());
                total_bytes += payload.size();
            }
            first_binary_frame_.clear();
        }

        while (true) {
            size_t received = 0;
            const struct curl_ws_frame* meta = nullptr;
            CURLcode res = WaitForRecv(curl, buf.data(), buf.size(), &received, &meta,
                                        30000);  // longer timeout for audio data
            if (res != CURLE_OK) {
                LOG_WARN("EdgeTts: recv error: {}", curl_easy_strerror(res));
                break;
            }
            if (received == 0) continue;

            if (meta && (meta->flags & CURLWS_BINARY)) {
                auto payload = ExtractAudioPayload(buf.data(), received);
                if (!payload.empty()) {
                    mp3_callback(payload.data(), payload.size());
                    total_bytes += payload.size();
                }
                continue;
            }
            if (meta && (meta->flags & CURLWS_TEXT)) {
                buf[received] = '\0';
                LOG_DEBUG("EdgeTts: text frame: {}", reinterpret_cast<const char*>(buf.data()));
                if (std::strstr(reinterpret_cast<const char*>(buf.data()), "turn.end")) {
                    break;
                }
                continue;
            }
            if (meta && (meta->flags & CURLWS_CLOSE)) {
                LOG_DEBUG("EdgeTts: WebSocket close frame");
                break;
            }
        }

        return total_bytes;
    }

    std::mutex ws_mutex_;
    std::vector<uint8_t> first_binary_frame_;
    char errbuf_[CURL_ERROR_SIZE] = {};
};

// ---- Constructor / Destructor ----

EdgeTtsProvider::EdgeTtsProvider()
    : impl_(std::make_unique<Impl>()) {}

EdgeTtsProvider::~EdgeTtsProvider() = default;

// ---- Synthesize (non-streaming: MP3 → ffmpeg → WAV) ----

std::string EdgeTtsProvider::Synthesize(const std::string& text,
                                        const std::string& output_path) {
    if (text.empty()) {
        LOG_WARN("EdgeTts: Synthesize skipped: empty text");
        return {};
    }

    EnsureDirectory(ParentDir(output_path));

    // Find ffmpeg
    std::string ffmpeg = FindFfmpeg();
    if (ffmpeg.empty()) {
        LOG_ERROR("EdgeTts: ffmpeg not found, cannot decode MP3 to WAV");
        return {};
    }

    CURL* curl = impl_->ConnectAndExchange(text, voice_);
    if (!curl) {
        LOG_ERROR("EdgeTts: Synthesize failed to connect");
        return {};
    }

    impl_->DrainMetadata(curl);

    // Collect all MP3 data
    std::vector<uint8_t> all_mp3;
    auto accumulate = [&all_mp3](const uint8_t* data, size_t len) {
        all_mp3.insert(all_mp3.end(), data, data + len);
    };

    size_t total_mp3 = impl_->ReceiveMp3(curl, accumulate);
    curl_easy_cleanup(curl);

    if (total_mp3 == 0) {
        LOG_ERROR("EdgeTts: Synthesize received no audio data");
        return {};
    }

    // Write MP3 to temp file, decode via ffmpeg to WAV
    std::string mp3_path = output_path + ".tmp.mp3";
    {
        FILE* fp = std::fopen(mp3_path.c_str(), "wb");
        if (!fp) {
            LOG_ERROR("EdgeTts: failed to write temp MP3: {}", mp3_path);
            return {};
        }
        bool ok = (std::fwrite(all_mp3.data(), 1, all_mp3.size(), fp) == all_mp3.size());
        std::fclose(fp);
        if (!ok) {
            RemoveFile(mp3_path);
            LOG_ERROR("EdgeTts: failed to write temp MP3 (partial write)");
            return {};
        }
    }

    // Decode MP3 → WAV via ffmpeg
    if (!DecodeMp3ToWav(ffmpeg, mp3_path, output_path)) {
        RemoveFile(mp3_path);
        LOG_ERROR("EdgeTts: ffmpeg decode failed for {}", output_path);
        return {};
    }

    RemoveFile(mp3_path);

    LOG_INFO("EdgeTts: synthesized '{}' ({} bytes MP3 -> {} bytes WAV)",
             output_path, total_mp3, FileSize(output_path));
    return output_path;
}

// ---- SynthesizeStream (real-time MP3 → PCM via ffmpeg pipe) ----

void EdgeTtsProvider::SynthesizeStream(const std::string& text,
                                       OnStreamStarted on_started,
                                       OnAudioChunk on_chunk) {
    if (text.empty()) {
        LOG_WARN("EdgeTts: SynthesizeStream skipped: empty text");
        return;
    }

    std::string ffmpeg = FindFfmpeg();
    if (ffmpeg.empty()) {
        LOG_ERROR("EdgeTts: ffmpeg not found, streaming unavailable");
        return;
    }

    CURL* curl = impl_->ConnectAndExchange(text, voice_);
    if (!curl) {
        LOG_ERROR("EdgeTts: SynthesizeStream failed to connect");
        return;
    }

    // Notify caller of audio format
    if (on_started) {
        on_started(kSampleRate, kChannels);
    }

    impl_->DrainMetadata(curl);

    // Build ffmpeg command: read MP3 from stdin, write PCM to stdout
    std::string ffmpeg_cmd = ffmpeg + " -i pipe:0 -f s16le -ar "
        + std::to_string(kSampleRate) + " -ac " + std::to_string(kChannels) + " pipe:1";

    // Launch ffmpeg with piped stdin/stdout (stderr passes through to parent)
    auto proc = Platform::CreatePipedSubprocess(ffmpeg_cmd);
    if (proc.pid < 0) {
        LOG_ERROR("EdgeTts: failed to launch ffmpeg for streaming");
        curl_easy_cleanup(curl);
        return;
    }

    // Writer thread: receive MP3 chunks from WebSocket, write to ffmpeg stdin
    std::thread write_thread([curl, &proc, this]() {
        auto writer = [&proc](const uint8_t* data, size_t len) {
            Platform::WritePipe(proc.stdin_fd,
                                reinterpret_cast<const char*>(data), len);
        };
        impl_->ReceiveMp3(curl, writer);
        // Signal EOF to ffmpeg by closing stdin
        Platform::ClosePipe(proc.stdin_fd);
    });

    // Main thread: read decoded PCM chunks from ffmpeg stdout
    {
        std::vector<uint8_t> pcm_buf(65536);
        int bytes_read = 0;
        while ((bytes_read = Platform::ReadPipe(proc.stdout_fd,
                    reinterpret_cast<char*>(pcm_buf.data()),
                    static_cast<int>(pcm_buf.size()))) > 0) {
            if (on_chunk) {
                on_chunk(pcm_buf.data(), bytes_read);
            }
        }
    }
    Platform::ClosePipe(proc.stdout_fd);

    write_thread.join();
    int exit_code = Platform::WaitProcessWithExitCode(proc.pid);
    curl_easy_cleanup(curl);

    if (exit_code != 0) {
        LOG_WARN("EdgeTts: ffmpeg exited with code {}", exit_code);
    }
    LOG_INFO("EdgeTts: streaming finished");
}

}  // namespace prosophor