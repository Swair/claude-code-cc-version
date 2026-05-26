// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "common/noncopyable.h"
#include "providers/tts/tts_provider.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

namespace prosophor {

/// TtsSpeaker: multi-backend TTS facade
/// Routes to edge-tts, supports both streaming and non-streaming
class TtsSpeaker : public Noncopyable {
 public:
    static TtsSpeaker& GetInstance();

    /// Initialize — creates the configured backend provider
    void Initialize();

    /// Switch TTS backend at runtime
    void SetBackend(const std::string& name);
    void ApplyVoiceProfile(const std::string& backend, const std::string& voice);

    /// Current backend name
    const std::string& GetBackend() const { return backend_; }

    /// Non-streaming: synthesize full WAV → wav_path callback
    void Speak(const std::string& text);
    void SpeakCached(const std::string& role_id, const std::string& text);

    /// Streaming: receive raw PCM audio chunks via callbacks
    void SpeakStream(const std::string& text);

    /// Whether audio is currently being generated
    bool IsSpeaking() const { return speaking_; }

    // -- Callbacks --

    /// Non-streaming: called when WAV file is ready
    using OnSynthesizedCallback = std::function<void(const std::string& wav_path)>;
    void SetOnSynthesized(OnSynthesizedCallback cb) { on_synthesized_ = std::move(cb); }

    /// Streaming: called once with format info before first chunk
    void SetOnStreamStarted(TtsProvider::OnStreamStarted cb) { on_stream_started_ = std::move(cb); }

    /// Streaming: called for each raw PCM chunk
    void SetOnAudioChunk(TtsProvider::OnAudioChunk cb) { on_audio_chunk_ = std::move(cb); }

    // -- Edge-tts specific --

    void SetVoice(const std::string& voice);
    const std::string& GetVoice() const { return voice_; }

 private:
    TtsSpeaker() = default;

    TtsProvider* GetOrCreateProvider();

    void SpeakAsync(const std::string& text);
    void SpeakAsync(const std::string& text, const std::string& output_path);
    void SpeakStreamAsync(const std::string& text);
    void ProcessSpeechQueue();
    bool HasQueueItems();

    /// Split text into short segments (by punctuation, min chars per segment)
    static std::vector<std::string> SplitSentences(const std::string& text, size_t min_chars);

    /// Internal streaming callbacks (forwarded from provider → external)
    void OnStreamStarted(int sample_rate, int channels);
    void OnAudioChunk(const uint8_t* data, size_t len);

    std::string backend_ = "edge-tts";
    std::string voice_ = "zh-CN-XiaoxiaoNeural";
    std::unique_ptr<TtsProvider> provider_;

    OnSynthesizedCallback on_synthesized_;
    TtsProvider::OnStreamStarted on_stream_started_;
    TtsProvider::OnAudioChunk on_audio_chunk_;

    std::atomic<bool> speaking_{false};
    std::atomic<bool> queue_running_{false};
    std::atomic<bool> first_audio_chunk_pending_{false};
    std::queue<std::string> speech_queue_;
    std::mutex queue_mutex_;
    size_t min_segment_chars_ = 5;
};

}  // namespace prosophor
