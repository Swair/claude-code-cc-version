// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "voice/voice_engine.h"
#include "media_engine/media/voice_channel.h"
#include "voice/vad_processor.h"
#include "voice/resampler.h"
#include "common/log_wrapper.h"
#include "common/thread_pool.h"
#include "common/time_wrapper.h"

namespace prosophor {

// ══════════════════════════════════════════════════════════════════════════
//  Construction / Destruction
// ══════════════════════════════════════════════════════════════════════════

VoiceEngine& VoiceEngine::GetInstance() {
    static VoiceEngine instance;
    return instance;
}

VoiceEngine::VoiceEngine()
    : asr_(AsrProviderRouter::GetInstance()),
      tts_(TtsProviderRouter::GetInstance()) {
    vad_ = std::make_unique<VadProcessor>();
}

VoiceEngine::~VoiceEngine() {
    media_engine::VoiceChannel::GetInstance().Stop();
}

// ── TTS text utilities ──

std::string VoiceEngine::SanitizeTtsText(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);

        // Skip 4-byte UTF-8 (emoji and decorative symbols)
        if ((c & 0xF0) == 0xF0) {
            i += 3;  // skip remaining 3 bytes of this 4-byte char
            continue;
        }

        if (c == '*' || c == '`' || c == '~' || c == '\\' || c == '|')
            continue;
        if (c == '#') {
            if (i == 0 || (i > 0 && (static_cast<unsigned char>(text[i-1]) == ' ' ||
                                      static_cast<unsigned char>(text[i-1]) == '\n')))
                continue;
        }
        if (c == '_' && result.size() > 0 && result.back() == ' ') continue;
        result += static_cast<char>(c);
    }
    return result;
}

// ══════════════════════════════════════════════════════════════════════════
//  ASR — VAD + accumulation
// ══════════════════════════════════════════════════════════════════════════

void VoiceEngine::FeedPCM(const int16_t* data, size_t samples) {
    if (samples != 480) return;

    bool speech = vad_->ProcessFrame(data, samples);

    // Catchup ring buffer (always keeps last 5 frames)
    std::array<int16_t, 480> buf;
    std::copy(data, data + samples, buf.begin());
    catchup_buf_.Push(buf);

    // ── VAD state machine ──
    if (!vad_speaking_) {
        if (speech) {
            vad_confirm_++;
        } else {
            vad_confirm_ = 0;
            return;
        }
        if (vad_confirm_ < 3) return;

        // Speech started: flush catchup buffer into stream buffer
        auto catchup = catchup_buf_.ReadAll();
        for (auto& f : catchup) {
            stream_buf_.insert(stream_buf_.end(), f.begin(), f.end());
        }
        vad_speaking_ = true;
        vad_silence_frames_ = 0;
        flush_counter_ = 0;
        return;
    }

    // Speaking: accumulate PCM
    stream_buf_.insert(stream_buf_.end(), data, data + samples);

    if (speech) {
        vad_silence_frames_ = 0;
        flush_counter_++;

        if (flush_counter_ >= 10 && !asr_in_flight_.exchange(true)) {
            flush_counter_ = 0;
            FlushInterim();
        }
    } else {
        vad_silence_frames_++;
        flush_counter_++;

        if (vad_silence_frames_ <= 3 && flush_counter_ >= 3 && !asr_in_flight_.exchange(true)) {
            flush_counter_ = 0;
            FlushInterim();
        }

        if (vad_silence_frames_ < 20) return;

        vad_speaking_ = false;
        vad_silence_frames_ = 0;
        vad_confirm_ = 0;
        flush_counter_ = 0;
        FlushFinal();
    }
}

void VoiceEngine::ResetVad() {
    vad_confirm_ = 0;
    vad_silence_frames_ = 0;
    vad_speaking_ = false;
    flush_counter_ = 0;
    stream_buf_.clear();
    catchup_buf_.Clear();
    if (vad_) {
        vad_->Reset();
    }
}

void VoiceEngine::FlushInterim() {
    if (stream_buf_.empty()) {
        asr_in_flight_ = false;
        return;
    }

    auto pcm = stream_buf_;
    GetGlobalThreadPool().Submit([this, pcm = std::move(pcm), &asr = asr_]() {
        auto t0 = SteadyClock::Now();
        std::string text = asr.AsrProcess(pcm);
        auto ms = SteadyClock::ElapsedMillis(t0);
        if (!text.empty()) {
            LOG_INFO("[ASR] interim: {} chars in {}ms", text.size(), ms);
            std::lock_guard<std::mutex> lock(result_mtx_);
            pending_result_ = std::move(text);
            has_result_ = true;
        } else {
            LOG_DEBUG("[ASR] interim: empty result in {}ms", ms);
        }
        asr_in_flight_ = false;
    });
}

void VoiceEngine::FlushFinal() {
    if (stream_buf_.empty()) {
        asr_in_flight_ = false;
        return;
    }

    auto pcm = std::move(stream_buf_);
    stream_buf_.clear();

    GetGlobalThreadPool().Submit([this, pcm = std::move(pcm), &asr = asr_]() {
        auto t0 = SteadyClock::Now();
        std::string text = asr.AsrProcess(pcm);
        auto ms = SteadyClock::ElapsedMillis(t0);
        if (!text.empty()) {
            LOG_INFO("[ASR] final: {} chars in {}ms", text.size(), ms);
            std::lock_guard<std::mutex> lock(result_mtx_);
            pending_result_ = std::move(text);
            has_result_ = true;
        } else {
            LOG_DEBUG("[ASR] final: empty result in {}ms", ms);
        }
        asr_in_flight_ = false;
    });
}

std::string VoiceEngine::PollResult() {
    std::lock_guard<std::mutex> lock(result_mtx_);
    if (!has_result_) return {};
    has_result_ = false;
    return std::move(pending_result_);
}

// ══════════════════════════════════════════════════════════════════════════
//  TTS — Full synthesis
// ══════════════════════════════════════════════════════════════════════════

TtsResponse VoiceEngine::Synthesize(const std::string& text,
                                     const std::string& backend,
                                     const std::string& voice) {
    std::string clean = SanitizeTtsText(text);
    LOG_INFO("[VoiceEngine] Synthesize text='{}', clean='{}', voice='{}'", text, clean, voice);
    if (clean.empty()) {
        return {false, "empty text after sanitization", {}, 0, 0};
    }

    auto provider = tts_.GetProvider();
    if (!provider) {
        LOG_ERROR("[VoiceEngine] no TTS provider available for backend='{}'", backend);
        return {false, "no provider", {}, 0, 0};
    }

    THROTTLE_LOG(1000, "[VoiceEngine] text = {}, clean = {}", text, clean);
    TtsRequest req{clean, voice};
    auto result = provider->Synthesize(req);
    if (!result.success) {
        return {false, result.error_msg, {}, 0, 0};
    }
    return result;
}

// ══════════════════════════════════════════════════════════════════════════
//  Audio Capture — delegates to VoiceChannel
// ══════════════════════════════════════════════════════════════════════════

void VoiceEngine::StartCapture() {
    ResetVad();
    auto& vc = media_engine::VoiceChannel::GetInstance();
    vc.InitCapture(
        [this](const int16_t* data, size_t n) { FeedPCM(data, n); }
    );
}

void VoiceEngine::StopCapture() {
    media_engine::VoiceChannel::GetInstance().EnableCapture(false);
}

bool VoiceEngine::IsCapturing() {
    return media_engine::VoiceChannel::GetInstance().IsRunning();
}

void VoiceEngine::Speak(const std::string& text,
                         const std::string& backend,
                         const std::string& voice) {
    GetGlobalThreadPool().Submit([this, text, backend, voice]() {
        LOG_INFO("[VoiceEngine::Speak] text='{}', backend='{}', voice='{}'", text, backend, voice);

        std::vector<int16_t> pcm;
        {
            std::lock_guard<std::mutex> lock(speak_mutex_);
            tts_speaking_ = true;
            auto result = Synthesize(text, backend, voice);
            if (result.success && !result.pcm.empty())
                pcm = Resample24kTo16k(result.pcm);
            tts_speaking_ = false;
        }
        if (!pcm.empty())
            media_engine::VoiceChannel::GetInstance().PlayAudio(pcm);
    });
}

}  // namespace prosophor
