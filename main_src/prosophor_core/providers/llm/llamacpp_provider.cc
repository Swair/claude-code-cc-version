// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "providers/llm/llamacpp_provider.h"
#include "common/log_wrapper.h"
#include "common/time_wrapper.h"

#ifdef PROSOPHOR_HAS_LOCAL_MODEL
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "llama.h"
#pragma GCC diagnostic pop
#include <thread>

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

// ─────────────────────────────────────────────────────────────────────────────
// PIMPL
// ─────────────────────────────────────────────────────────────────────────────

struct LlamacppProvider::Impl {
#ifdef PROSOPHOR_HAS_LOCAL_MODEL
    llama_model*        model   = nullptr;
    llama_context*      ctx     = nullptr;
    llama_sampler*      sampler = nullptr;
    const llama_vocab*  vocab   = nullptr;   // owned by model, do not free

    ~Impl() { Release(); }

    void Release() {
        if (sampler) { llama_sampler_free(sampler); sampler = nullptr; }
        if (ctx)     { llama_free(ctx);              ctx     = nullptr; }
        if (model)   { llama_model_free(model);      model   = nullptr; }
        vocab = nullptr;
    }
#endif
    bool loaded = false;
};

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

LlamacppProvider::LlamacppProvider(const LlamacppModelConfig& cfg)
    : impl_(std::make_unique<Impl>()), cfg_(cfg) {}

LlamacppProvider::~LlamacppProvider() = default;

// ─────────────────────────────────────────────────────────────────────────────
// Load / Unload
// ─────────────────────────────────────────────────────────────────────────────

bool LlamacppProvider::Load() {
#ifndef PROSOPHOR_HAS_LOCAL_MODEL
    LOG_ERROR("[local] Built without PROSOPHOR_BUILD_LLAMA");
    return false;
#else
    if (impl_->loaded) return true;

    const std::string& path = cfg_.model_path;

    if (path.empty()) { LOG_ERROR("[local] model_path is empty"); return false; }

    // Silence llama.cpp's verbose stdout — route to spdlog DEBUG instead
    llama_log_set(llama_log_to_spdlog, nullptr);

    LOG_INFO("[local] Loading model: {}", path);
    auto t0 = SteadyClock::Now();

    // Model
    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = cfg_.n_gpu_layers;
        impl_->model = llama_model_load_from_file(path.c_str(), mparams);
    if (!impl_->model) {
        LOG_ERROR("[local] Failed to load: {}", path);
        return false;
    }

    // Vocab — pointer owned by model
    impl_->vocab = llama_model_get_vocab(impl_->model);

    // Context
    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx           = static_cast<uint32_t>(cfg_.n_ctx);
    cparams.n_threads       = cfg_.n_threads > 0
                              ? cfg_.n_threads
                              : 8;                              // decode: fewer threads, less contention
    cparams.n_threads_batch = std::max(cparams.n_threads * 4, 32); // prefill: more threads
    cparams.n_batch         = static_cast<uint32_t>(cfg_.n_ctx); // match context window (crash if prompt > n_batch)
    cparams.n_ubatch        = 512;                              // micro-batch for GPU parallelism
    cparams.type_k          = GGML_TYPE_Q8_0;                   // KV cache Q8 quant — 50% less VRAM
    cparams.type_v          = GGML_TYPE_Q8_0;
    cparams.offload_kqv     = true;                             // KQV on GPU
    cparams.flash_attn_type = LLAMA_FLASH_ATTN_TYPE_ENABLED;    // Flash Attention
    impl_->ctx = llama_init_from_model(impl_->model, cparams);
    if (!impl_->ctx) {
        LOG_ERROR("[local] Failed to create context");
        impl_->Release();
        return false;
    }

    // Sampler: min_p → temperature → distribution (min_p better than top_p)
    llama_sampler_chain_params sparams = llama_sampler_chain_default_params();
    impl_->sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(impl_->sampler, llama_sampler_init_min_p(0.05f, 1));
    llama_sampler_chain_add(impl_->sampler, llama_sampler_init_temp(cfg_.temperature));
    llama_sampler_chain_add(impl_->sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

    impl_->loaded = true;
    auto ms = SteadyClock::ElapsedMillis(t0);
    LOG_INFO("[local] Loaded in {} ms  gpu_layers={}", ms, cfg_.n_gpu_layers);
    return true;
#endif
}

void LlamacppProvider::Unload() {
#ifdef PROSOPHOR_HAS_LOCAL_MODEL
    impl_->Release();
    impl_->loaded = false;
    LOG_INFO("[local] Model unloaded");
#endif
}

bool LlamacppProvider::IsLoaded() const { return impl_->loaded; }

// ─────────────────────────────────────────────────────────────────────────────
// BuildPrompt — applies model's chat template via llama_chat_apply_template
// ─────────────────────────────────────────────────────────────────────────────

std::string LlamacppProvider::BuildPrompt(const ChatRequest& request) const {
#ifndef PROSOPHOR_HAS_LOCAL_MODEL
    return {};
#else
    if (!impl_->model) return {};

    // Keep strings alive until llama_chat_apply_template returns
    struct MsgStore { std::string role; std::string content; };
    std::vector<MsgStore>          store;
    std::vector<llama_chat_message> msgs;

    // System prompt
    std::string sys_text;
    for (const auto& s : request.system) sys_text += s.text + "\n";
    if (!sys_text.empty())
        store.push_back({"system", std::move(sys_text)});

    // Conversation
    for (const auto& m : request.messages) {
        std::string text = m.text();
        if (!text.empty()) store.push_back({m.role, std::move(text)});
    }

    msgs.reserve(store.size());
    for (const auto& s : store)
        msgs.push_back({s.role.c_str(), s.content.c_str()});

    // Get the model's built-in chat template
    const char* tmpl = llama_model_chat_template(impl_->model, /*name=*/nullptr);

    // Fallback: try detecting template from model filename
    if (!tmpl) {
        std::string path_lower = cfg_.model_path;
        std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), ::tolower);
        if (path_lower.find("gemma") != std::string::npos) {
            tmpl = llama_model_chat_template(impl_->model, "gemma");
        }
    }

    if (tmpl) {
        LOG_DEBUG("[local] Chat template: {}", tmpl);
    }

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
                tmpl,
                msgs.data(), static_cast<size_t>(msgs.size()),
                true,
                buf.data(), static_cast<int32_t>(buf.size()));
        }
    }

    if (n < 0) {
        // Detect model family for template fallback
        std::string path_lower = cfg_.model_path;
        std::transform(path_lower.begin(), path_lower.end(), path_lower.begin(), ::tolower);

        // Gemma format: <start_of_turn>user\n{content}<end_of_turn>\n<start_of_turn>model\n
        if (path_lower.find("gemma") != std::string::npos) {
            std::string gemma;
            for (const auto& s : store) {
                auto role = (s.role == "assistant") ? "model" : s.role;
                gemma += "<start_of_turn>" + role + "\n" + s.content + "<end_of_turn>\n";
            }
            gemma += "<start_of_turn>model\n";
            return gemma;
        }

        // Last resort: ChatML-like raw concat
        LOG_WARN("[local] llama_chat_apply_template failed, using raw concat");
        std::string fallback;
        for (const auto& s : store)
            fallback += "<|" + s.role + "|>\n" + s.content + "\n";
        return fallback + "<|assistant|>\n";
    }

    return std::string(buf.data(), static_cast<size_t>(n));
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// ChatStream
// ─────────────────────────────────────────────────────────────────────────────

ChatResponse LlamacppProvider::ChatStream(
    const ChatRequest& request,
    std::function<void(StreamEvent, std::string)> callback) {

    ChatResponse response;

#ifndef PROSOPHOR_HAS_LOCAL_MODEL
    response.error_msg = "Built without PROSOPHOR_BUILD_LLAMA";
    if (callback) callback(StreamEvent::kError, response.error_msg);
    return response;
#else
    if (!impl_->loaded) {
        response.error_msg = "Model not loaded";
        if (callback) callback(StreamEvent::kError, response.error_msg);
        return response;
    }

    std::string prompt = BuildPrompt(request);
    if (prompt.empty()) {
        response.error_msg = "Empty prompt";
        if (callback) callback(StreamEvent::kError, response.error_msg);
        return response;
    }
    PrintRequestLog(request);

    // ── Tokenize ───────────────────────────────────────────────────────────
    const int n_vocab_tokens = llama_vocab_n_tokens(impl_->vocab);
    std::vector<llama_token> prompt_tokens(
        std::max(static_cast<int>(prompt.size()), n_vocab_tokens));

    int n_prompt = llama_tokenize(
        impl_->vocab,
        prompt.c_str(), static_cast<int32_t>(prompt.size()),
        prompt_tokens.data(), static_cast<int32_t>(prompt_tokens.size()),
        /*add_special=*/true,
        /*parse_special=*/false);

    if (n_prompt < 0) {
        response.error_msg = "Tokenize failed";
        if (callback) callback(StreamEvent::kError, response.error_msg);
        return response;
    }
    prompt_tokens.resize(static_cast<size_t>(n_prompt));

    // ── Clear KV memory from previous turn ────────────────────────────────
    llama_memory_clear(llama_get_memory(impl_->ctx), /*data=*/true);

    // ── Prefill ────────────────────────────────────────────────────────────
    LOG_DEBUG("[local] Prefill: {} tokens (CPU, please wait...)", n_prompt);
    LOG_DEBUG("[local] Prompt tail: ...{}", prompt.size() > 120 ? prompt.substr(prompt.size() - 120) : prompt);
    auto t_prefill = SteadyClock::Now();

    llama_batch batch = llama_batch_get_one(
        prompt_tokens.data(), static_cast<int32_t>(prompt_tokens.size()));
    if (llama_decode(impl_->ctx, batch) != 0) {
        response.error_msg = "llama_decode (prefill) failed";
        if (callback) callback(StreamEvent::kError, response.error_msg);
        return response;
    }

    auto prefill_ms = SteadyClock::ElapsedMillis(t_prefill);
    LOG_DEBUG("[local] Prefill done in {} ms, generating...", prefill_ms);

    // ── Generation loop ────────────────────────────────────────────────────
    const int max_new = request.max_tokens > 0 ? request.max_tokens : cfg_.max_new_tokens;
    int   n_generated = 0;
    char  piece_buf[256];
    auto  t_gen_start = SteadyClock::Now();
    auto  t_last_log  = t_gen_start;

    if (callback) callback(StreamEvent::kContentStart, {});

    while (n_generated < max_new) {
        llama_token token = llama_sampler_sample(impl_->sampler, impl_->ctx, -1);

        if (llama_vocab_is_eog(impl_->vocab, token)) break;

        int n_piece = llama_token_to_piece(
            impl_->vocab, token,
            piece_buf, static_cast<int32_t>(sizeof(piece_buf)),
            /*lstrip=*/0,
            /*special=*/false);

        if (n_piece > 0) {
            std::string piece(piece_buf, static_cast<size_t>(n_piece));
            // Filter residual special tokens that slipped past is_eog
            // (e.g. <|im_end|>, <|endoftext|>, <turn|> etc.)
            if (piece.size() >= 3 && piece.front() == '<' && piece.back() == '>') break;
            response.content_text += piece;
            if (callback) callback(StreamEvent::kContentDelta, std::move(piece));
        }

        llama_batch next = llama_batch_get_one(&token, 1);
        if (llama_decode(impl_->ctx, next) != 0) {
            LOG_WARN("[local] llama_decode failed at token {}", n_generated);
            break;
        }
        ++n_generated;

        // Progress log every 5 seconds so user knows it's still running
        if (SteadyClock::IsExpired(t_last_log, static_cast<int64_t>(5000))) {
            double elapsed = SteadyClock::ElapsedSeconds(t_gen_start);
            double tps = n_generated / (elapsed > 0 ? elapsed : 1.0);
            LOG_DEBUG("[local] Generating... {} tokens  ({:.2f} t/s)",
                     n_generated, tps);
            t_last_log = SteadyClock::Now();
        }
    }

    if (callback) callback(StreamEvent::kContentEnd, {});

    auto total_ms = SteadyClock::ElapsedMillis(t_gen_start);
    double tps = n_generated / (total_ms > 0 ? total_ms / 1000.0 : 1.0);

    response.usage.prompt_tokens     = n_prompt;
    response.usage.completion_tokens = n_generated;
    response.usage.total_tokens      = n_prompt + n_generated;
    response.stop_reason = (n_generated >= max_new) ? "max_tokens" : "stop";
    LOG_DEBUG("[local] Done: {} tokens in {} ms ({:.2f} t/s)",
             n_generated, total_ms, tps);
    return response;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Chat / Serialize / misc
// ─────────────────────────────────────────────────────────────────────────────

ChatResponse LlamacppProvider::Chat(const ChatRequest& request) {
    return ChatStream(request, nullptr);
}

std::string LlamacppProvider::Serialize(const ChatRequest& request) const {
    return BuildPrompt(request);
}

HeaderList LlamacppProvider::CreateHeaders(const ChatRequest&) const {
    return {};
}

ChatResponse LlamacppProvider::Deserialize(const std::string&) const {
    ChatResponse r;
    r.error_msg = "LlamacppProvider: Deserialize not supported (in-process)";
    return r;
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

void LlamacppProvider::PrintRequestLog(const ChatRequest& request) const {
    LOG_DEBUG("[local] Chat  model={}  max_tokens={}", cfg_.model_path, request.max_tokens);
}

}  // namespace prosophor