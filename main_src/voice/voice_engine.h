// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "common/list_buffer.h"
#include "common/noncopyable.h"
#include "core/agent_types.h"
#include "providers/provider_router/asr_provider_router.h"
#include "providers/provider_router/tts_provider_router.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace prosophor {

class VadProcessor;

/// VoiceEngine: unified ASR + TTS orchestrator.
///
/// Audio I/O (mic capture + speaker playback) is handled by
/// media_engine::VoiceChannel. VoiceEngine registers callbacks:
///   - Capture callback: feeds mic PCM into VAD/ASR pipeline
///   - Playback callback: drains TTS PCM to AudioStreamer
///
/// Render thread polls PollResult() each frame for ASR results.
class VoiceEngine : public Noncopyable {
public:
    static VoiceEngine& GetInstance();
    ~VoiceEngine();

    // ── TTS text utilities (public static) ──

    static std::string SanitizeTtsText(const std::string& text);

    // ═══════════════════════════════════════════════════════════════
    //  ASR
    // ═══════════════════════════════════════════════════════════════

    /// Feed PCM data into VAD + accumulation.
    void FeedPCM(const int16_t* data, size_t samples);

    /// Poll for the latest transcription result (thread-safe).
    std::string PollResult();

    bool IsVadSpeaking() const { return vad_speaking_; }
    void ResetVad();

    // ═══════════════════════════════════════════════════════════════
    //  TTS
    // ═══════════════════════════════════════════════════════════════

    /// Full synthesis: text in → PCM audio out.
    TtsResponse Synthesize(const std::string& text,
                           const std::string& backend = "edge-tts",
                           const std::string& voice = "zh-CN-XiaoxiaoNeural");

    bool IsSpeaking() const { return tts_speaking_; }

    // ═══════════════════════════════════════════════════════════════
    //  Audio Capture (mic) — delegates to VoiceChannel
    // ═══════════════════════════════════════════════════════════════

    void StartCapture();
    void StopCapture();
    bool IsCapturing();

    // ═══════════════════════════════════════════════════════════════
    //  Audio Playback (TTS → VoiceChannel → AudioStreamer)
    // ═══════════════════════════════════════════════════════════════

    void Speak(const std::string& text,
               const std::string& backend = "edge-tts",
               const std::string& voice = "zh-CN-XiaoxiaoNeural");

private:
    VoiceEngine();

    void FlushInterim();
    void FlushFinal();

    // VAD
    std::unique_ptr<VadProcessor> vad_;
    std::vector<int16_t> stream_buf_;
    bool vad_speaking_ = false;
    int vad_confirm_ = 0;
    int vad_silence_frames_ = 0;

    static constexpr int kCatchupFrames = 5;
    FixedBuffer<std::array<int16_t, 480>, 5> catchup_buf_;

    // ASR & TTS routers
    AsrProviderRouter& asr_;
    TtsProviderRouter& tts_;

    int flush_counter_ = 0;
    std::atomic<bool> asr_in_flight_{false};

    // Result queue
    std::mutex result_mtx_;
    std::string pending_result_;
    bool has_result_ = false;

    // TTS state
    std::atomic<bool> tts_speaking_{false};
    std::mutex speak_mutex_;
};

}  // namespace prosophor
