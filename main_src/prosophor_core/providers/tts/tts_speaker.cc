// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/tts/tts_speaker.h"
#include "providers/tts/edge_tts_provider.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"
#include "config/config.h"

#include <openssl/evp.h>

#include <array>
#include <atomic>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

namespace prosophor {

namespace {

constexpr const char* kTtsCacheDir = "assets/tts_cache/";

std::string MakeOutputPath() {
    static std::atomic<int> seq{0};
    seq++;
    return std::string(kTtsCacheDir) + "tts_" + std::to_string(seq) + ".wav";
}

std::string MakeCachedOutputPath(const std::string& role_id,
                                 const std::string& backend,
                                 const std::string& voice,
                                 const std::string& text) {
    const std::string key = "tts-v1|role=" + role_id + "|provider=" + backend +
                            "|voice=" + voice + "|text=" + text;
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int digest_len = 0;
    EVP_Digest(key.data(), key.size(), digest.data(), &digest_len, EVP_md5(), nullptr);

    std::ostringstream oss;
    for (unsigned int i = 0; i < digest_len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    return std::string(kTtsCacheDir) + oss.str() + ".wav";
}

}  // namespace

// ---- Singleton ----

TtsSpeaker& TtsSpeaker::GetInstance() {
    static TtsSpeaker instance;
    return instance;
}

// ---- Initialize ----

void TtsSpeaker::Initialize() {
    EnsureDirectory(kTtsCacheDir);
    // Load backend from config
    auto& config = ProsophorConfig::GetInstance();
    backend_ = config.tts.backend;
    voice_ = "zh-CN-XiaoxiaoNeural";
    GetOrCreateProvider();
    LOG_INFO("TtsSpeaker initialized (backend: {}, voice: {}).", backend_, voice_);
}

// ---- Backend selection ----

void TtsSpeaker::SetBackend(const std::string& name) {
    if (name == backend_ && provider_) return;
    backend_ = name;
    provider_.reset();
    LOG_INFO("TtsSpeaker: switched backend to {}", backend_);
}

void TtsSpeaker::ApplyVoiceProfile(const std::string& backend, const std::string& voice) {
    if (backend != backend_) {
        backend_ = backend;
        provider_.reset();
        LOG_INFO("TtsSpeaker: switched backend to {}", backend_);
    }
    if (!voice.empty() && voice != voice_) {
        SetVoice(voice);
    }
}

TtsProvider* TtsSpeaker::GetOrCreateProvider() {
    if (provider_) return provider_.get();

    if (backend_ == "edge-tts") {
        auto edge = std::make_unique<EdgeTtsProvider>();
        edge->SetVoice(voice_);
        LOG_INFO("TtsSpeaker: creating edge-tts provider voice='{}'", voice_);
        provider_ = std::move(edge);
    } else {
        LOG_ERROR("TtsSpeaker: unknown backend '{}', falling back to edge-tts", backend_);
        backend_ = "edge-tts";
        auto edge = std::make_unique<EdgeTtsProvider>();
        edge->SetVoice(voice_);
        provider_ = std::move(edge);
    }

    return provider_.get();
}

// ---- Speak ----

void TtsSpeaker::Speak(const std::string& text) {
    if (text.empty()) {
        LOG_INFO("TtsSpeaker: Speak skipped because text is empty");
        return;
    }
    if (speaking_) {
        LOG_INFO("TtsSpeaker: Speak skipped because synthesis is already active");
        return;
    }

    LOG_INFO("TtsSpeaker: Speak requested chars={} backend='{}'", text.size(), backend_);
    std::thread([this, text]() {
        SpeakAsync(text);
    }).detach();
}

void TtsSpeaker::SpeakCached(const std::string& role_id, const std::string& text) {
    if (text.empty()) {
        LOG_INFO("TtsSpeaker: cached synthesis skipped because text is empty");
        return;
    }
    const std::string output_path = MakeCachedOutputPath(role_id, backend_, voice_, text);
    if (FileExists(output_path) && FileSize(output_path) > 0) {
        LOG_INFO("TtsSpeaker: cache hit role='{}' backend='{}' voice='{}' output='{}'", role_id,
                 backend_, voice_, output_path);
        if (on_synthesized_) {
            on_synthesized_(output_path);
        }
        return;
    }

    if (speaking_) {
        LOG_INFO("TtsSpeaker: cached synthesis skipped because synthesis is already active");
        return;
    }

    LOG_INFO("TtsSpeaker: cache miss role='{}' backend='{}' voice='{}' chars={} output='{}'", role_id,
             backend_, voice_, text.size(), output_path);
    std::thread([this, text, output_path]() {
        SpeakAsync(text, output_path);
    }).detach();
}

void TtsSpeaker::SpeakAsync(const std::string& text) {
    SpeakAsync(text, MakeOutputPath());
}

void TtsSpeaker::SpeakAsync(const std::string& text, const std::string& output_path) {
    speaking_ = true;

    auto* provider = GetOrCreateProvider();
    LOG_INFO("TtsSpeaker: synthesis start chars={} backend='{}' output='{}'", text.size(), backend_, output_path);
    const std::string result = provider->Synthesize(text, output_path);

    speaking_ = false;

    if (result.empty()) {
        LOG_ERROR("TtsSpeaker: synthesis failed backend='{}' output='{}'", backend_, output_path);
        return;
    }

    LOG_INFO("TtsSpeaker: synthesis ready output='{}' callback={}", result,
             on_synthesized_ ? "yes" : "no");
    if (on_synthesized_) {
        on_synthesized_(result);
    }
}

// ---- SpeakStream ----

void TtsSpeaker::SpeakStream(const std::string& text) {
    if (text.empty()) {
        LOG_INFO("TtsSpeaker: SpeakStream skipped because text is empty");
        return;
    }

    auto segments = SplitSentences(text, min_segment_chars_);
    if (segments.empty()) {
        LOG_INFO("TtsSpeaker: SpeakStream skipped because no segments were produced");
        return;
    }

    const size_t segment_count = segments.size();
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (queue_running_) {
            std::queue<std::string> empty;
            std::swap(speech_queue_, empty);
            for (auto& seg : segments) {
                speech_queue_.push(std::move(seg));
            }
            LOG_INFO("TtsSpeaker: stream queue replaced chars={} segments={} backend='{}'", text.size(),
                     segment_count, backend_);
            return;
        }
        for (auto& seg : segments) {
            speech_queue_.push(std::move(seg));
        }
        queue_running_ = true;
    }

    LOG_INFO("TtsSpeaker: stream queue started chars={} segments={} backend='{}'", text.size(),
             segment_count, backend_);
    std::thread([this]() {
        ProcessSpeechQueue();
    }).detach();
}

void TtsSpeaker::ProcessSpeechQueue() {
    size_t processed_count = 0;
    do {
        while (true) {
            std::string seg;
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (speech_queue_.empty()) break;
                seg = speech_queue_.front();
                speech_queue_.pop();
            }
            ++processed_count;
            LOG_INFO("TtsSpeaker: stream segment start index={} chars={} backend='{}'", processed_count,
                     seg.size(), backend_);
            SpeakStreamAsync(seg);
        }
    } while (HasQueueItems());

    queue_running_ = false;
    LOG_INFO("TtsSpeaker: stream queue finished segments={} backend='{}'", processed_count, backend_);
}

bool TtsSpeaker::HasQueueItems() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !speech_queue_.empty();
}

void TtsSpeaker::SpeakStreamAsync(const std::string& text) {
    speaking_ = true;

    auto* provider = GetOrCreateProvider();
    if (!provider->SupportsStreaming()) {
        LOG_INFO("TtsSpeaker: backend '{}' has no streaming support, falling back to WAV synthesis", backend_);
        SpeakAsync(text);
        return;
    }

    first_audio_chunk_pending_ = true;
    LOG_INFO("TtsSpeaker: streaming synthesis start chars={} backend='{}' callbacks stream={} chunk={}",
             text.size(), backend_, on_stream_started_ ? "yes" : "no", on_audio_chunk_ ? "yes" : "no");
    provider->SynthesizeStream(text,
        [this](int sr, int ch) { OnStreamStarted(sr, ch); },
        [this](const uint8_t* d, size_t len) { OnAudioChunk(d, len); });

    speaking_ = false;
    LOG_INFO("TtsSpeaker: streaming synthesis finished backend='{}'", backend_);
}

std::vector<std::string> TtsSpeaker::SplitSentences(const std::string& text, size_t min_chars) {
    std::vector<std::string> segments;
    std::string buf;

    for (size_t i = 0; i < text.size(); i++) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        bool is_split = false;
        if (c < 0x80) {
            buf += text[i];
            is_split = (c == '.' || c == '!' || c == '?' || c == ',' || c == '\n');
        } else if ((c & 0xF0) == 0xE0 && i + 2 < text.size()) {
            unsigned char c2 = static_cast<unsigned char>(text[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(text[i + 2]);
            buf.append(text, i, 3);
            i += 2;
            is_split = ((c == 0xE3 && c2 == 0x80 && c3 == 0x82) ||
                        (c == 0xEF && c2 == 0xBC && (c3 == 0x81 || c3 == 0x8C || c3 == 0x9F)));
        } else {
            buf += text[i];
        }
        if (is_split && buf.size() >= min_chars) {
            segments.push_back(buf);
            buf.clear();
        }
    }
    if (!buf.empty()) {
        segments.push_back(buf);
    }
    return segments;
}

// ---- Internal callbacks ----

void TtsSpeaker::OnStreamStarted(int sample_rate, int channels) {
    LOG_INFO("TtsSpeaker: stream started sample_rate={} channels={} callback={}", sample_rate,
             channels, on_stream_started_ ? "yes" : "no");
    if (on_stream_started_) {
        on_stream_started_(sample_rate, channels);
    }
}

void TtsSpeaker::OnAudioChunk(const uint8_t* data, size_t len) {
    if (first_audio_chunk_pending_.exchange(false)) {
        LOG_INFO("TtsSpeaker: first audio chunk bytes={} callback={}", len,
                 on_audio_chunk_ ? "yes" : "no");
    }
    if (on_audio_chunk_) {
        on_audio_chunk_(data, len);
    }
}

// ---- Voice ----

void TtsSpeaker::SetVoice(const std::string& voice) {
    voice_ = voice;
    if (provider_ && backend_ == "edge-tts") {
        static_cast<EdgeTtsProvider*>(provider_.get())->SetVoice(voice);
    }
    LOG_INFO("TtsSpeaker: voice set to '{}'", voice_);
}

}  // namespace prosophor
