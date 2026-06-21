// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "core/agent_session.h"

#include "common/log_wrapper.h"

namespace {

std::vector<std::string> TtsSplitSentences(const std::string& text, size_t min_chars) {
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

}  // anonymous namespace

namespace prosophor {

AgentSession::AgentSession(const std::string& sid,
                            const std::string& task, AgentRole* r)
    : role_(r), session_id_(sid), task_description_(task) {
    created_at_ = SteadyClock::Now();
    last_active_ = SteadyClock::Now();

    if (r) {
        provider_ = LlmProviderRouter::GetInstance().GetProviderByName(r->provider_prot);
        use_tools_ = r->enable_tools;
        working_directory_.clear();
        messages_.clear();
        system_prompt_.clear();
        session_history_dir_.clear();
    }
}

AgentSession::AgentSession(AgentSession&& other) noexcept
    : tool_executor_(std::move(other.tool_executor_)),
      output_callback_(std::move(other.output_callback_)),
      role_(other.role_),
      use_tools_(other.use_tools_),
      auto_confirm_tools_(other.auto_confirm_tools_),
      working_directory_(std::move(other.working_directory_)),
      consolidation_service_(other.consolidation_service_),
      related_files_(std::move(other.related_files_)),
      stop_requested_(other.stop_requested_.load()),
      session_id_(std::move(other.session_id_)),
      task_description_(std::move(other.task_description_)),
      provider_(std::move(other.provider_)),
      base_url_(std::move(other.base_url_)),
      api_key_(std::move(other.api_key_)),
      timeout_(other.timeout_),
      session_history_dir_(std::move(other.session_history_dir_)),
      is_active_(other.is_active_),
      created_at_(other.created_at_),
      last_active_(other.last_active_),
      messages_(std::move(other.messages_)),
      system_prompt_(std::move(other.system_prompt_)),
      state_(other.state_),
      state_message_(std::move(other.state_message_)),
      streaming_text_(std::move(other.streaming_text_)),
      streaming_thinking_(std::move(other.streaming_thinking_)),
      tts_chunks_(std::move(other.tts_chunks_)),
      tts_speak_callback_(std::move(other.tts_speak_callback_)),
      mutable_role_(std::move(other.mutable_role_)) {
}

AgentSession& AgentSession::operator=(AgentSession&& other) noexcept {
    if (this == &other) return *this;

    tool_executor_ = std::move(other.tool_executor_);
    output_callback_ = std::move(other.output_callback_);
    role_ = other.role_;
    use_tools_ = other.use_tools_;
    auto_confirm_tools_ = other.auto_confirm_tools_;
    working_directory_ = std::move(other.working_directory_);
    consolidation_service_ = other.consolidation_service_;
    related_files_ = std::move(other.related_files_);
    stop_requested_.store(other.stop_requested_.load());
    session_id_ = std::move(other.session_id_);
    task_description_ = std::move(other.task_description_);
    provider_ = std::move(other.provider_);
    base_url_ = std::move(other.base_url_);
    api_key_ = std::move(other.api_key_);
    timeout_ = other.timeout_;
    session_history_dir_ = std::move(other.session_history_dir_);
    is_active_ = other.is_active_;
    created_at_ = other.created_at_;
    last_active_ = other.last_active_;
    messages_ = std::move(other.messages_);
    system_prompt_ = std::move(other.system_prompt_);
    state_ = other.state_;
    state_message_ = std::move(other.state_message_);
    streaming_text_ = std::move(other.streaming_text_);
    streaming_thinking_ = std::move(other.streaming_thinking_);
    tts_chunks_ = std::move(other.tts_chunks_);
    tts_speak_callback_ = std::move(other.tts_speak_callback_);
    mutable_role_ = std::move(other.mutable_role_);
    return *this;
}

void AgentSession::SetOutput(AgentRuntimeState new_state,
                              const std::string& state_msg,
                              const std::optional<MessageSchema>& reply) {

    // 更新输出信号
    {
        std::lock_guard<std::mutex> lock(render_mutex_);
        state_ = new_state;
        state_message_ = state_msg;

        if (new_state == AgentRuntimeState::STREAM_CONTENT_TYPING && reply) {
            streaming_text_ += reply->text();
            tts_chunks_.Push(reply->text());
            // Token/s tracking: count chars, estimate ~4 chars per token
            streaming_char_count_ += reply->text().size();
            auto elapsed = std::chrono::duration<float>(
                SteadyClock::Now() - stream_start_time_).count();
            if (elapsed > 0.1f) {
                streaming_token_speed_ = (streaming_char_count_ / 4.0f) / elapsed;
            }
        }
        if (new_state == AgentRuntimeState::STREAM_CONTENT_START) {
            stream_start_time_ = SteadyClock::Now();
            streaming_char_count_ = 0;
            streaming_token_speed_ = 0.0f;
            streaming_text_.clear();
            tts_chunks_.Clear();
        }
        if (new_state == AgentRuntimeState::STREAM_THINKING && reply) {
            streaming_thinking_ += reply->text();
        }
        if (new_state == AgentRuntimeState::STREAM_THINKING_START) {
            stream_start_time_ = SteadyClock::Now();
            streaming_char_count_ = 0;
            streaming_token_speed_ = 0.0f;
            streaming_thinking_.clear();
        }
        if (reply && (
            new_state == AgentRuntimeState::STREAM_MODE_COMPLETE ||
            new_state == AgentRuntimeState::COMPLETE ||
            new_state == AgentRuntimeState::TOOL_USE ||
            new_state == AgentRuntimeState::STATE_ERROR)) {
            messages_.push_back(*reply);
            streaming_text_.clear();
            streaming_thinking_.clear();
        }
    }

    // 输出信号驱动动作或渲染
    if (output_callback_) {
        output_callback_(session_id_, role_ ? role_->id : "",
                        new_state, state_msg, reply);
    }

    // ── TTS: pop chunks, join, split, send complete sentences ──
    if (tts_speak_callback_) {
        // Determine effective voice — if "none", discard all chunks and skip TTS
        std::string voice = role_ && !role_->tts_voice.empty()
            ? role_->tts_voice : "zh-CN-XiaoxiaoNeural";
        if (voice == "none") {
            tts_chunks_.Clear();
            return;
        }

        auto items = tts_chunks_.PopAll();
        if (items.empty()) return;

        std::string pending;
        for (auto& c : items) pending += std::move(c);

        bool done = (new_state == AgentRuntimeState::COMPLETE ||
                     new_state == AgentRuntimeState::STREAM_MODE_COMPLETE);
        std::string to_send;

        if (done) {
            // 流式结束：整段 flush，无需分句
            to_send = std::move(pending);
        } else {
            // 流式进行中：分句，完整句子发送，末尾不完整句保留到下次
            auto segments = TtsSplitSentences(pending, 5);
            if (segments.size() >= 2) {
                // 至少有两段 → 前面的都是完整句子，最后一段可能不完整，因为还没有done结束
                for (size_t i = 0; i + 1 < segments.size(); ++i)
                    to_send += segments[i];
                tts_chunks_.Push(segments.back());
            } else {
                // 只有一段且不完整 → 全部保留等后续 chunk
                tts_chunks_.Push(std::move(pending));
            }
        }

        if (to_send.empty()) return;

        std::string backend = role_ && !role_->tts_backend.empty()
            ? role_->tts_backend : "edge-tts";
        tts_speak_callback_(to_send, backend, voice);
    }
}

RenderSnapshot AgentSession::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(render_mutex_);
    RenderSnapshot snap;
    snap.session_id = session_id_;
    snap.role_id = role_ ? role_->id : "";
    snap.state = state_;
    snap.state_message = state_message_;
    // 只拷贝最后 10 条用于 UI 渲染，避免随对话增长的全量拷贝
    constexpr size_t kMaxVisibleMessages = 10;
    if (messages_.size() > kMaxVisibleMessages) {
        snap.messages.assign(messages_.end() - kMaxVisibleMessages, messages_.end());
    } else {
        snap.messages = messages_;
    }
    snap.streaming_text = streaming_text_;
    snap.streaming_thinking = streaming_thinking_;
    snap.streaming_token_speed = streaming_token_speed_;
    return snap;
}

void AgentSession::AddUserMessage(const std::string& text) {
    std::lock_guard<std::mutex> lock(render_mutex_);
    messages_.emplace_back("user", text);
}

void AgentSession::CleanupInterruptedLoop() {
    std::lock_guard<std::mutex> lock(render_mutex_);
    if (!messages_.empty() && messages_.back().role == "user") {
        messages_.pop_back();
    }
}

void AgentSession::CompactHistory(const std::vector<MessageSchema>& kept_messages,
                                   const std::string& summary) {
    std::lock_guard<std::mutex> lock(render_mutex_);
    messages_ = kept_messages;
    if (!summary.empty()) {
        system_prompt_.clear();
        system_prompt_.push_back({"text", summary, false});
    }
}

void AgentSession::SetSystemPrompt(const std::vector<SystemSchema>& prompt) {
    system_prompt_ = prompt;
}

void AgentSession::ApplyProviderOverride(const std::string& provider_name,
                                          const std::string& model) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    auto& router = LlmProviderRouter::GetInstance();
    provider_ = router.GetProviderByName(provider_name);

    mutable_role_ = *role_;
    role_ = &mutable_role_.value();
    role_->provider_prot = provider_name;

    auto& config = ProsophorConfig::GetInstance();
    auto prov_it = config.llm_providers.find(provider_name);
    if (prov_it == config.llm_providers.end()) return;

    auto& model_configs = prov_it->second.model_configs;
    base_url_ = prov_it->second.base_url;
    api_key_ = prov_it->second.api_key;
    timeout_ = prov_it->second.timeout;

    const ModelConfig* matched = nullptr;
    auto model_it = model_configs.find(model);
    if (model_it != model_configs.end()) {
        matched = &model_it->second;
    } else if (!model.empty()) {
        for (auto& [k, v] : model_configs) {
            if (v.model == model) {
                matched = &v;
                break;
            }
        }
    }

    if (matched) {
        role_->model = matched->model;
        std::string entry_base_url;
        std::string entry_api_key;
        int entry_timeout = 0;
        if (prov_it->second.FindEntryForModel(provider_name, model,
                                               entry_base_url,
                                               entry_api_key,
                                               entry_timeout)) {
            base_url_ = entry_base_url;
            api_key_ = entry_api_key;
            timeout_ = entry_timeout;
        }
        role_->temperature = matched->temperature;
        role_->max_tokens = matched->max_tokens;
        // Don't override thinking from model config — role JSON is the source of truth
        LOG_DEBUG("Applied provider override: provider={}, model={}, base_url={}",
                 provider_name, matched->model, base_url_);
    } else {
        LOG_WARN("No matching agent for '{}' in provider '{}', using provider defaults",
                 model, provider_name);
    }
}

}  // namespace prosophor
