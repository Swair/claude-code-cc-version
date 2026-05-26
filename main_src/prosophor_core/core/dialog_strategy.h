// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>

#include "core/messages_schema.h"

namespace prosophor {

class AgentSession;

/// 对话策略：消息预处理 + 上下文压缩
class DialogStrategy {
public:
    // ── 策略回调类型 ────────────────────────────────────
    using PreprocessorFn = std::function<void(const std::string&, AgentSession&)>;
    using CompactorFn = std::function<void(const MessageSchema&, AgentSession&)>;

    // ── 压缩策略枚举 ────────────────────────────────────
    enum class Strategy {
        Summary,      // Generate AI summary of old messages
        Truncate,     // Keep only recent N messages
        Hybrid        // Summary + keep recent
    };

    // ── 压缩配置 ────────────────────────────────────────
    struct Config {
        Strategy strategy = Strategy::Hybrid;
        int max_messages = 100;
        int keep_recent = 20;
        int max_tokens = 100000;
        std::string summary_prompt;

        static Config Default() {
            return {Strategy::Hybrid, 100, 20, 100000,
                "Please provide a concise summary of the conversation so far, "
                "including: key decisions made, code changes discussed, "
                "and any unresolved issues. Keep it brief but comprehensive."};
        }
    };

    // ── 压缩结果 ────────────────────────────────────────
    struct Result {
        std::string summary;
        int tokens_saved = 0;
        int messages_removed = 0;
        std::vector<MessageSchema> kept_messages;
    };

    // ═══════════════════════════════════════════════════
    // 内置预处理策略（静态）
    // ═══════════════════════════════════════════════════
    static void DefaultPreprocessor(const std::string& processed_message, AgentSession& session);
    static void SummarizingPreprocessor(const std::string& processed_message, AgentSession& session);

    // ═══════════════════════════════════════════════════
    // 内置压缩工具（静态）
    // ═══════════════════════════════════════════════════
    static bool NeedsCompaction(const std::vector<MessageSchema>& messages, const Config& config);
    static int EstimateTokens(const std::vector<MessageSchema>& messages);
    static std::vector<MessageSchema> KeepRecentMessages(
        const std::vector<MessageSchema>& messages, int keep_count);

    // ═══════════════════════════════════════════════════
    // 配置
    // ═══════════════════════════════════════════════════
    void SetConfig(const Config& config) { config_ = config; }
    const Config& GetConfig() const { return config_; }
    void SetPreprocessor(PreprocessorFn p) { preprocessor_ = std::move(p); }
    void SetCompactor(CompactorFn c) { compactor_ = std::move(c); }
    void SetAutoCompactEnabled(bool enabled) { auto_compact_enabled_ = enabled; }
    bool IsAutoCompactEnabled() const { return auto_compact_enabled_; }

    // ═══════════════════════════════════════════════════
    // 应用策略
    // ═══════════════════════════════════════════════════
    void Preprocess(const std::string& message, AgentSession& session) const;
    void Compact(AgentSession& session, const MessageSchema& processed_message) const;
    void CompactIfNeeded(AgentSession& session) const;

    // ═══════════════════════════════════════════════════
    // LLM 压缩
    // ═══════════════════════════════════════════════════
    Result CompactWithLLM(const std::vector<MessageSchema>& messages,
                           std::function<std::string(const std::string& prompt)> llm_callback);
    Result CompressToTokenLimit(const std::vector<MessageSchema>& messages,
                                 int max_tokens,
                                 std::function<std::string(const std::string& prompt)> llm_callback);

    // ═══════════════════════════════════════════════════
    // 工厂
    // ═══════════════════════════════════════════════════
    /// 创建默认策略（无自定义回调，Preprocess 使用内置 fallback 链）
    static std::shared_ptr<DialogStrategy> CreateDefault();

    // ═══════════════════════════════════════════════════
    // 遥测
    // ═══════════════════════════════════════════════════
    int GetCompactionCount() const { return compaction_count_; }
    bool HasPreprocessor() const { return !!preprocessor_; }
    bool HasCompactor() const { return !!compactor_; }

private:
    PreprocessorFn preprocessor_;
    CompactorFn compactor_;
    Config config_;
    bool auto_compact_enabled_ = true;
    int compaction_count_ = 0;

    /// Generate summary of old messages using LLM
    std::string GenerateSummary(const std::vector<MessageSchema>& old_messages,
                                 std::function<std::string(const std::string& prompt)> llm_callback);

    /// Build compaction prompt
    std::string BuildCompactionPrompt(const std::vector<MessageSchema>& messages);
};

}  // namespace prosophor
