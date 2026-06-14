// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/llm/llamacpp_provider.h"
#include "common/log_wrapper.h"
#include "common/thread_pool.h"
#include "common/time_wrapper.h"

#ifdef PROSOPHOR_HAS_LOCAL_MODEL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "llama.h"
#include "common/reasoning-budget.h"
#include "common/chat-auto-parser.h"
#include "common/chat.h"
#include "ggml-backend.h"
#pragma GCC diagnostic pop

// Redirect llama.cpp log output to spdlog so it doesn't flood the TUI terminal.
// Called once before any llama API.
static void llama_log_to_spdlog(ggml_log_level level,
                                 const char*    text,
                                 void*          /*user_data*/) {
    if (!text || text[0] == '\0') return;
    // Strip trailing newline for cleaner spdlog output
    std::string msg(text);
    if (!msg.empty() && msg.back() == '\n') msg.pop_back();
    if (msg.empty()) return;

    switch (level) {
        case GGML_LOG_LEVEL_ERROR: LOG_ERROR("[llama] {}", msg); break;
        case GGML_LOG_LEVEL_WARN:  LOG_WARN ("[llama] {}", msg); break;
        default:                   LOG_DEBUG("[llama] {}", msg); break;
    }
}
#endif

#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>

namespace prosophor {

// PIMPL to keep llama.h C types out of the public header.
struct LlamacppProvider::Impl {
#ifdef PROSOPHOR_HAS_LOCAL_MODEL
    llama_model*        model   = nullptr;
    llama_context*      ctx     = nullptr;
    llama_sampler*      sampler = nullptr;
    const llama_vocab*  vocab   = nullptr;   // owned by model, do not free

    // Thinking/reasoning markers detected from chat template
    std::string reason_start;
    std::string reason_end;
    bool        has_reasoning_markers = false;
    bool        thinking_enabled      = false;

    // Compiled chat template for proper prompt rendering with thinking support
    common_chat_templates_ptr chat_tmpls;

    ~Impl() { Release(); }

    void Release() {
        if (sampler) { llama_sampler_free(sampler); sampler = nullptr; }
        if (ctx)     { llama_free(ctx);              ctx     = nullptr; }
        if (model)   { llama_model_free(model);      model   = nullptr; }
        vocab = nullptr;
    }
#endif
};

// Convert KV cache type string to ggml_type enum.
// Config accepts "f16" (2 bytes), "q8_0" (1 byte), "q4_0" (0.5 byte, default).
#ifdef PROSOPHOR_HAS_LOCAL_MODEL
static ggml_type kv_cache_type_from_string(const std::string& s) {
    if (s == "q8_0") return GGML_TYPE_Q8_0;
    if (s == "f16")  return GGML_TYPE_F16;
    return GGML_TYPE_Q4_0;
}
#endif

LlamacppProvider::LlamacppProvider(const LlamacppModelConfig& cfg)
    : impl_(std::make_unique<Impl>()), cfg_(cfg) {}

LlamacppProvider::~LlamacppProvider() {
    if (load_future_.valid()) {
        load_future_.wait();
    }
}

bool LlamacppProvider::Load() {
#ifndef PROSOPHOR_HAS_LOCAL_MODEL
    LOG_ERROR("[local] Built without PROSOPHOR_BUILD_LLAMA");
    return false;
#else
    bool expected = false;
    if (!loading_.compare_exchange_strong(expected, true)) {
        LOG_WARN("[local] Load already in progress");
        return false;
    }

    // If already loaded, just return
    if (loaded_) {
        loading_ = false;
        return true;
    }

    load_future_ = GetGlobalThreadPool().SubmitWithFuture([this]() {
        try {
            const std::string& path = cfg_.model_path;
            if (path.empty()) {
                LOG_ERROR("[local] model_path is empty");
                load_error_msg_ = "model_path is empty";
                loading_ = false;
                return;
            }

            llama_log_set(llama_log_to_spdlog, nullptr);

            LOG_INFO("[local] Loading model: {}", path);
            auto t0 = SteadyClock::Now();

            llama_model_params mparams = llama_model_default_params();
            mparams.n_gpu_layers = cfg_.n_gpu_layers;

            // Route MoE expert weights to CPU to save VRAM (needed for large MoE models
            // like Qwen 35B-A3B on 8GB GPUs; no effect on dense models like Gemma).
            llama_model_tensor_buft_override moe_overrides[2] = {};
            if (cfg_.cpu_moe) {
                moe_overrides[0] = llm_ffn_exps_cpu_override();
                mparams.tensor_buft_overrides = moe_overrides;
                LOG_INFO("[local] MoE experts routed to CPU (cpu_moe=true)");
            }
            if (cfg_.no_mmap) {
                mparams.use_mmap = false;
                LOG_INFO("[local] mmap disabled (no_mmap=true)");
            }

            std::lock_guard<std::mutex> lock(model_mutex_);
            impl_->model = llama_model_load_from_file(path.c_str(), mparams);
            if (!impl_->model) {
                LOG_ERROR("[local] Failed to load: {}", path);
                load_error_msg_ = "Failed to load model: " + path;
                loading_ = false;
                return;
            }
            impl_->vocab = llama_model_get_vocab(impl_->model);

            llama_context_params cparams = llama_context_default_params();
            cparams.n_ctx           = static_cast<uint32_t>(cfg_.context_window);
            cparams.n_threads       = cfg_.threads > 0 ? cfg_.threads : 8;
            cparams.n_threads_batch = cfg_.n_threads_batch > 0
                                      ? cfg_.n_threads_batch
                                      : std::max(cparams.n_threads * 4, 32);
            cparams.n_batch         = static_cast<uint32_t>(cfg_.n_batch);
            cparams.n_ubatch        = static_cast<uint32_t>(cfg_.n_ubatch);
            cparams.type_k          = kv_cache_type_from_string(cfg_.type_k);
            cparams.type_v          = kv_cache_type_from_string(cfg_.type_v);
            cparams.offload_kqv     = cfg_.offload_kqv;
            cparams.flash_attn_type = cfg_.flash_attn
                                      ? LLAMA_FLASH_ATTN_TYPE_ENABLED
                                      : LLAMA_FLASH_ATTN_TYPE_DISABLED;
            impl_->ctx = llama_init_from_model(impl_->model, cparams);
            if (!impl_->ctx) {
                LOG_ERROR("[local] Failed to create context");
                load_error_msg_ = "Failed to create context";
                impl_->Release();
                loading_ = false;
                return;
            }

            llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
            impl_->sampler = llama_sampler_chain_init(sparams);
            if (cfg_.min_p > 0.0f) {
                llama_sampler_chain_add(impl_->sampler, llama_sampler_init_min_p(cfg_.min_p, 1));
            }
            llama_sampler_chain_add(impl_->sampler, llama_sampler_init_temp(cfg_.temperature));
            llama_sampler_chain_add(impl_->sampler,
                cfg_.seed >= 0
                    ? llama_sampler_init_dist(static_cast<uint32_t>(cfg_.seed))
                    : llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

            // Initialize the full chat template for proper prompt rendering (supports thinking).
            impl_->chat_tmpls = common_chat_templates_init(impl_->model, "");
            if (impl_->chat_tmpls) {
                LOG_DEBUG("[local] Chat template initialized");
            }

            // Detect thinking/reasoning markers from the model's chat template.
            if (cfg_.thinking) {
                try {
                    const char* tmpl_str = llama_model_chat_template(impl_->model, nullptr);
                    if (tmpl_str && tmpl_str[0]) {
                        char buf[64];
                        int n;
                        n = llama_token_to_piece(impl_->vocab, llama_vocab_bos(impl_->vocab),
                                                 buf, (int32_t)sizeof(buf), 0, true);
                        std::string bos(n > 0 ? std::string(buf, (size_t)n) : "");
                        n = llama_token_to_piece(impl_->vocab, llama_vocab_eos(impl_->vocab),
                                                 buf, (int32_t)sizeof(buf), 0, true);
                        std::string eos(n > 0 ? std::string(buf, (size_t)n) : "");

                        common_chat_template chat_tmpl(tmpl_str, bos, eos);
                        autoparser::analyze_reasoning reasoning(chat_tmpl, /*supports_tools=*/false);

                        impl_->has_reasoning_markers = (reasoning.mode != autoparser::reasoning_mode::NONE);
                        impl_->reason_start          = reasoning.start;
                        impl_->reason_end            = reasoning.end;
                        impl_->thinking_enabled      = true;

                        LOG_INFO("[local] Reasoning markers: mode={} start='{}' end='{}'",
                                 static_cast<int>(reasoning.mode),
                                 reasoning.start, reasoning.end);
                    }
                } catch (const std::exception& e) {
                    LOG_WARN("[local] Reasoning marker detection failed: {}", e.what());
                }
            }

            loaded_ = true;
            loading_ = false;
            auto ms = SteadyClock::ElapsedMillis(t0);
            LOG_INFO("[local] Loaded in {} ms  n_gpu_layers={}", ms, cfg_.n_gpu_layers);
        } catch (const std::exception& e) {
            LOG_ERROR("[local] Load exception: {}", e.what());
            load_error_msg_ = e.what();
            loading_ = false;
        }
    });

    return true;
#endif
}

void LlamacppProvider::Unload() {
    if (load_future_.valid()) {
        load_future_.wait();
    }
#ifdef PROSOPHOR_HAS_LOCAL_MODEL
    {
        std::lock_guard<std::mutex> lock(model_mutex_);
        impl_->Release();
    }
    loaded_ = false;
    loading_ = false;
    load_error_msg_.clear();
    LOG_INFO("[local] Model unloaded");
#endif
}

bool LlamacppProvider::IsLoaded() const { return loaded_; }

bool LlamacppProvider::IsLoading() const { return loading_; }

std::string LlamacppProvider::GetLoadError() const { return load_error_msg_; }

std::string LlamacppProvider::BuildPrompt(const ChatRequest& request) const {
#ifndef PROSOPHOR_HAS_LOCAL_MODEL
    return {};
#else
    if (!impl_->model) return {};

    // Prefer compiled chat template (supports thinking via enable_thinking)
    if (impl_->chat_tmpls) {
        try {
            common_chat_templates_inputs inputs;
            inputs.add_generation_prompt = true;
            inputs.enable_thinking       = cfg_.thinking;
            inputs.use_jinja             = true;
            inputs.add_bos               = false;

            std::vector<common_chat_msg> msgs;
            for (const auto& s : request.system) {
                if (s.text.empty()) { continue; }
                common_chat_msg m;
                m.role    = "system";
                m.content = s.text;
                msgs.push_back(std::move(m));
            }
            for (const auto& m : request.messages) {
                std::string text = m.text();
                if (text.empty()) { continue; }
                common_chat_msg cm;
                cm.role    = m.role;
                cm.content = std::move(text);
                msgs.push_back(std::move(cm));
            }
            inputs.messages = std::move(msgs);

            auto result = common_chat_templates_apply(impl_->chat_tmpls.get(), inputs);
            if (!result.prompt.empty()) {
                LOG_DEBUG("[local] Built prompt via common_chat_templates_apply ({} bytes, thinking={})",
                         result.prompt.size(), result.supports_thinking);
                return result.prompt;
            }
        } catch (const std::exception& e) {
            LOG_WARN("[local] common_chat_templates_apply failed: {}", e.what());
        }
    }

    // Fallback: use raw llama_chat_apply_template (no thinking support)
    struct MsgStore { std::string role; std::string content; };
    std::vector<MsgStore>          store;
    std::vector<llama_chat_message> msgs;

    std::string sys_text;
    for (const auto& s : request.system) sys_text += s.text + "\n";
    if (!sys_text.empty()) {
        store.push_back({"system", std::move(sys_text)});
    }

    for (const auto& m : request.messages) {
        std::string text = m.text();
        if (!text.empty()) store.push_back({m.role, std::move(text)});
    }

    msgs.reserve(store.size());
    for (const auto& s : store)
        msgs.push_back({s.role.c_str(), s.content.c_str()});

    const char* tmpl = llama_model_chat_template(impl_->model, /*name=*/nullptr);
    if (!tmpl) {
        std::string path_lower = cfg_.model_path;
        std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), ::tolower);
        if (path_lower.find("gemma") != std::string::npos) {
            tmpl = llama_model_chat_template(impl_->model, "gemma");
        }
    }

    if (tmpl) LOG_DEBUG("[local] Chat template: {}", tmpl);

    std::vector<char> buf(8192);
    int n = -1;
    if (tmpl) {
        n = llama_chat_apply_template(
            tmpl,
            msgs.data(), static_cast<size_t>(msgs.size()),
            /*add_ass=*/true,
            buf.data(), static_cast<int32_t>(buf.size()));
        if (n > static_cast<int>(buf.size())) {
            buf.resize(static_cast<size_t>(n) + 1);
            n = llama_chat_apply_template(
                tmpl, msgs.data(), static_cast<size_t>(msgs.size()),
                true, buf.data(), static_cast<int32_t>(buf.size()));
        }
    }

    if (n < 0) {
        std::string path_lower = cfg_.model_path;
        std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), ::tolower);

        if (path_lower.find("gemma") != std::string::npos) {
            std::string gemma;
            for (const auto& s : store) {
                auto role = (s.role == "assistant") ? "model" : s.role;
                gemma += "<start_of_turn>" + role + "\n" + s.content + "<end_of_turn>\n";
            }
            gemma += "<start_of_turn>model\n";
            return gemma;
        }

        LOG_WARN("[local] llama_chat_apply_template failed, using raw concat");
        std::string fallback;
        for (const auto& s : store)
            fallback += "<|" + s.role + "|>\n" + s.content + "\n";
        return fallback + "<|assistant|>\n";
    }

    return std::string(buf.data(), static_cast<size_t>(n));
#endif
}

void LlamacppProvider::PrintRequestLog(const ChatRequest& request) const {
    LOG_DEBUG("[local] Chat  model={}  max_tokens={}", cfg_.model_path, request.max_tokens);
}

HeaderList LlamacppProvider::CreateHeaders(const ChatRequest&) const {
    return {};
}

std::string LlamacppProvider::Serialize(const ChatRequest& request) const {
    return BuildPrompt(request);
}

ChatResponse LlamacppProvider::Deserialize(const std::string&) const {
    ChatResponse r;
    r.error_msg = "LlamacppProvider: Deserialize not supported (in-process)";
    return r;
}

ChatResponse LlamacppProvider::ChatStream(
    const ChatRequest& request,
    std::function<void(StreamEvent, std::string)> callback) {

    ChatResponse response;

#ifndef PROSOPHOR_HAS_LOCAL_MODEL
    response.error_msg = "Built without PROSOPHOR_BUILD_LLAMA";
    return response;
#else
    if (!loaded_) {
        response.error_msg = "Model not loaded";
        return response;
    }

    std::string prompt;

    // Build prompt via compiled chat template (supports thinking + PEG parser).
    // We do this here rather than in BuildPrompt() so we can also extract the
    // PEG parser and thinking markers from the result.
    if (impl_->chat_tmpls) {
        try {
            common_chat_templates_inputs inputs;
            inputs.add_generation_prompt = true;
            inputs.enable_thinking       = cfg_.thinking;
            inputs.use_jinja             = true;
            inputs.add_bos               = false;

            std::vector<common_chat_msg> msgs;
            for (const auto& s : request.system) {
                if (s.text.empty()) { continue; }
                common_chat_msg m;
                m.role = "system"; m.content = s.text;
                msgs.push_back(std::move(m));
            }
            for (const auto& m : request.messages) {
                std::string text = m.text();
                if (text.empty()) { continue; }
                common_chat_msg cm;
                cm.role = m.role; cm.content = std::move(text);
                msgs.push_back(std::move(cm));
            }
            inputs.messages = std::move(msgs);

            auto result = common_chat_templates_apply(impl_->chat_tmpls.get(), inputs);
            prompt = result.prompt;

            // Set thinking markers for output-based thinking/content separation
            if (result.supports_thinking && !result.thinking_start_tag.empty()) {
                impl_->has_reasoning_markers = true;
                impl_->reason_start         = result.thinking_start_tag;
                impl_->reason_end           = result.thinking_end_tag;
                impl_->thinking_enabled     = true;
                LOG_DEBUG("[local] Template thinking markers: start='{}' end='{}'",
                         impl_->reason_start, impl_->reason_end);
            }
        } catch (const std::exception& e) {
            LOG_WARN("[local] Template apply failed: {}", e.what());
        }
    }

    // Fallback: use BuildPrompt (llama_chat_apply_template or manual Gemma format)
    if (prompt.empty()) {
        prompt = BuildPrompt(request);
    }
    if (prompt.empty()) {
        response.error_msg = "Empty prompt";
        return response;
    }
    PrintRequestLog(request);

    // Tokenize
    const int n_vocab_tokens = llama_vocab_n_tokens(impl_->vocab);
    std::vector<llama_token> prompt_tokens(
        std::max(static_cast<int>(prompt.size()), n_vocab_tokens));
    int n_prompt = llama_tokenize(
        impl_->vocab,
        prompt.c_str(), static_cast<int32_t>(prompt.size()),
        prompt_tokens.data(), static_cast<int32_t>(prompt_tokens.size()),
        /*add_special=*/true,
        /*parse_special=*/true);
    if (n_prompt < 0) {
        response.error_msg = "Tokenize failed";
        return response;
    }
    prompt_tokens.resize(static_cast<size_t>(n_prompt));

    // Safety check: ensure prompt + max_tokens fits within context window
    uint32_t n_ctx_max = llama_n_ctx(impl_->ctx);
    uint32_t n_reserve = static_cast<uint32_t>(request.max_tokens > 0 ? request.max_tokens : cfg_.max_tokens);
    if (static_cast<uint32_t>(n_prompt) + n_reserve > n_ctx_max) {
        int allowed = static_cast<int>(n_ctx_max - n_reserve);
        if (allowed > 0) {
            int trim = n_prompt - allowed;
            LOG_WARN("[local] Prompt {} tokens exceeds available context ({}), trimming {} tokens from head",
                     n_prompt, n_ctx_max - n_reserve, trim);
            prompt_tokens.erase(prompt_tokens.begin(),
                                prompt_tokens.begin() + std::min(trim, n_prompt - 1));
            n_prompt = static_cast<int>(prompt_tokens.size());
        } else {
            response.error_msg = "Prompt exceeds context window even with zero reserve";
            return response;
        }
    }

    // Clear KV cache from previous turn
    llama_memory_clear(llama_get_memory(impl_->ctx), /*data=*/true);

    // Chunked prefill
    uint32_t batch_max = llama_n_batch(impl_->ctx);
    LOG_DEBUG("[local] Prefill: {} tokens (n_batch={})", n_prompt, batch_max);
    LOG_DEBUG("[local] Prompt tail: ...{}", prompt.size() > 120 ? prompt.substr(prompt.size() - 120) : prompt);
    auto t_prefill = SteadyClock::Now();
    {
        int32_t batch_sz = static_cast<int32_t>(batch_max);
        for (int32_t i = 0; i < n_prompt; i += batch_sz) {
            int32_t n_tokens = std::min(batch_sz, n_prompt - i);
            llama_batch batch = llama_batch_get_one(prompt_tokens.data() + i, n_tokens);
            if (llama_decode(impl_->ctx, batch) != 0) {
                response.error_msg = "llama_decode (prefill) failed";
                return response;
            }
        }
    }
    LOG_DEBUG("[local] Prefill done in {} ms", SteadyClock::ElapsedMillis(t_prefill));

    // Token-by-token generation loop
    const int max_new = request.max_tokens > 0 ? request.max_tokens : cfg_.max_tokens;
    int   n_generated = 0;
    char  piece_buf[256];
    auto  t_gen_start = SteadyClock::Now();
    ThrottleLog tlog;

    // -- Streaming output via text-based marker detection --
    // Markers (e.g. <|channel>thought / <channel|> for Gemma 4) are
    // extracted from the chat template during prompt building above.
    // The pending buffer with kPendingTail handles markers that span
    // across BPE subword token boundaries.
    bool  thinking       = false;
    bool  has_thinking   = false;
    bool  response_done  = false;
    // After thinking ends, some models emit the end marker token again as
    // the first content piece. Strip it once to prevent it from leaking.
    bool  skip_end_marker_in_content = false;

    auto emit_content  = [&](std::string&& t) {
        response.content_text += t;
        callback(StreamEvent::kContentDelta, std::move(t));
    };
    auto emit_thinking = [&](std::string&& t) {
        response.AddThinking(t);
        callback(StreamEvent::kThinkingDelta, std::move(t));
    };

    std::string   pending;
    constexpr int kPendingTail = 64;

    const std::string& kReasonStart = impl_->reason_start;
    const std::string& kReasonEnd   = impl_->reason_end;
    const bool use_thinking = impl_->has_reasoning_markers && impl_->thinking_enabled;

    auto find_and_flush = [&](const std::string& marker, bool* is_marker) -> bool {
        if (marker.empty()) { *is_marker = false; return false; }
        auto pos = pending.find(marker);
        if (pos == std::string::npos) { *is_marker = false; return false; }
        *is_marker = true;
        if (pos > 0) {
            if (thinking) { emit_thinking(pending.substr(0, pos)); }
            else          { emit_content(pending.substr(0, pos)); }
        }
        return true;
    };

    auto has_content = [&] { return !response.content_text.empty() || has_thinking; };

    auto end_content = [&] {
        if (thinking) { callback(StreamEvent::kThinkingEnd, {}); thinking = false; }
        callback(StreamEvent::kContentEnd, {});
    };

    callback(StreamEvent::kContentStart, {});

    while (n_generated < max_new) {
        llama_token token = llama_sampler_sample(impl_->sampler, impl_->ctx, -1);
        if (llama_vocab_is_eog(impl_->vocab, token)) { break; }

        int n_piece = llama_token_to_piece(
            impl_->vocab, token,
            piece_buf, static_cast<int32_t>(sizeof(piece_buf)),
            /*lstrip=*/0,
            /*special=*/true);
        if (n_piece <= 0) { continue; }

        pending += std::string(piece_buf, static_cast<size_t>(n_piece));

        // --- end_of_turn: model finished its response ---
        {   bool found;
            if (find_and_flush("<end_of_turn>", &found) && found) {
                if (has_content()) { end_content(); }
                else               { callback(StreamEvent::kContentEnd, {}); }
                response_done = true;
                break;
            }
        }

        // --- start_of_turn: unexpected mid-response, treat as end ---
        {   bool found;
            if (find_and_flush("<start_of_turn>", &found) && found) {
                if (has_content()) { end_content(); }
                else               { callback(StreamEvent::kContentEnd, {}); }
                response_done = true;
                break;
            }
        }

        // --- thinking start marker (detected from chat template) ---
        if (use_thinking && !thinking && !kReasonStart.empty()) {
            bool found;
            if (find_and_flush(kReasonStart, &found) && found) {
                if (has_content()) { callback(StreamEvent::kContentEnd, {}); }
                callback(StreamEvent::kThinkingStart, {});
                thinking = true;
                has_thinking = true;
                pending.clear();
                continue;
            }
        }

        // --- thinking end marker (detected from chat template) ---
        if (use_thinking && thinking && !kReasonEnd.empty()) {
            bool found;
            if (find_and_flush(kReasonEnd, &found) && found) {
                callback(StreamEvent::kThinkingEnd, {});
                callback(StreamEvent::kContentStart, {});
                thinking = false;
                skip_end_marker_in_content = true;
                pending.clear();
                // Do NOT continue — fall through to llama_decode so the
                // end-marker token gets processed in the KV cache and
                // the next sample produces real content, not a duplicate.
            }
        }

        // After thinking ends, some models emit the end marker token
        // again as the first content piece. Strip it once.
        if (skip_end_marker_in_content && !thinking && !kReasonEnd.empty()) {
            auto pos = pending.find(kReasonEnd);
            if (pos == 0) {
                pending = pending.substr(kReasonEnd.size());
            }
            skip_end_marker_in_content = false;
        }

        // Flush all except the last kPendingTail bytes
        if (pending.size() > kPendingTail) {
            auto safe = pending.substr(0, pending.size() - kPendingTail);
            if (thinking) { emit_thinking(std::move(safe)); }
            else          { emit_content(std::move(safe)); }
            pending = pending.substr(pending.size() - kPendingTail);
        }

        llama_batch next = llama_batch_get_one(&token, 1);
        if (llama_decode(impl_->ctx, next) != 0) {
            LOG_WARN("[local] llama_decode failed at token {}", n_generated);
            break;
        }
        ++n_generated;

        if (tlog.Check(5000)) {
            double elapsed = SteadyClock::ElapsedSeconds(t_gen_start);
            LOG_DEBUG("[local] Generating... {} tokens  ({:.2f} t/s)",
                     n_generated, n_generated / (elapsed > 0 ? elapsed : 1.0));
        }
    }

    // Flush any text still in the safety buffer
    if (!response_done && !pending.empty()) {
        // Strip end marker one last time if skip flag is still set
        if (skip_end_marker_in_content && !thinking && !kReasonEnd.empty()) {
            auto pos = pending.find(kReasonEnd);
            if (pos == 0) {
                pending = pending.substr(kReasonEnd.size());
            }
        }
        if (thinking) { emit_thinking(std::move(pending)); }
        else          { emit_content(std::move(pending)); }
        pending.clear();
    }

    if (!response_done) {
        if (thinking) {
            callback(StreamEvent::kThinkingEnd, {});
        } else {
            callback(StreamEvent::kContentEnd, {});
        }
    }
    if (has_thinking) { response.has_thinking = true; }

    auto total_ms = SteadyClock::ElapsedMillis(t_gen_start);
    response.usage.prompt_tokens     = n_prompt;
    response.usage.completion_tokens = n_generated;
    response.usage.total_tokens      = n_prompt + n_generated;
    response.stop_reason = (n_generated >= max_new) ? "max_tokens" : "stop";
    LOG_DEBUG("[local] Done: {} tokens in {} ms ({:.2f} t/s)",
             n_generated, total_ms,
             n_generated / (total_ms > 0 ? total_ms / 1000.0 : 1.0));
    return response;
#endif
}

ChatResponse LlamacppProvider::Chat(const ChatRequest& request) {
    return ChatStream(request, [](StreamEvent, std::string) {});
}

std::vector<std::string> LlamacppProvider::GetSupportedModels() const {
#ifdef PROSOPHOR_HAS_LOCAL_MODEL
    if (impl_->model) {
        char buf[256] = {};
        llama_model_desc(impl_->model, buf, sizeof(buf));
        return {buf[0] ? std::string(buf) : cfg_.model_path};
    }
#endif
    return {cfg_.model_path};
}

}  // namespace prosophor