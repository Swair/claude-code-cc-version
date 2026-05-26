// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "providers/llm/llm_provider.h"
#include "config/config.h"

namespace prosophor {

/// In-process LLM provider using llama.cpp C API directly.
/// Owns model, context, and sampler — no external server needed.
class LlamacppProvider : public LLMProvider {
public:
    explicit LlamacppProvider(const LlamacppModelConfig& cfg);
    ~LlamacppProvider() override;

    bool Load();
    void Unload();
    bool IsLoaded() const;

    // LLMProvider interface
    std::string GetProviderName() const override { return "llamacpp"; }
    std::vector<std::string> GetSupportedModels() const override;
    std::string Serialize(const ChatRequest& request) const override;
    ChatResponse Deserialize(const std::string& json_str) const override;
    ChatResponse ChatStream(const ChatRequest& request,
        std::function<void(StreamEvent, std::string)> callback) override;

    ChatResponse Chat(const ChatRequest& request);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    LlamacppModelConfig cfg_;

    HeaderList CreateHeaders(const ChatRequest& request) const override;
    void PrintRequestLog(const ChatRequest& request) const override;
    std::string BuildPrompt(const ChatRequest& request) const;
};

}  // namespace prosophor
