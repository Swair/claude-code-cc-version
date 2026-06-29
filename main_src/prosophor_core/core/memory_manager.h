// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <filesystem>
#include <thread>
#include <unordered_map>
#include <mutex>

#include "core/messages_schema.h"
#include "common/log_wrapper.h"
#include "config/config.h"

namespace prosophor {

class AgentSession;

/// LLM 回调类型（用于 ConsolidateSegment）
using MemoryLlmCallback = std::function<std::string(const std::string& prompt)>;

// ============================================================================
// 数据结构
// ============================================================================

/// Key decision extracted from conversation
struct KeyDecision {
    std::string type;       // "design_decision" | "code_change" | "unresolved_issue" | "lesson_learned"
    std::string content;
    std::vector<std::string> related_files;
    std::string timestamp;
};

/// Memory consolidation result
struct ConsolidationResult {
    std::string summary;
    std::vector<KeyDecision> decisions;
    int messages_processed = 0;
    int tokens_estimated = 0;
};

// ============================================================================
// MemoryStrategy — 记忆策略接口
// ============================================================================

class MemoryStrategy {
public:
    virtual ~MemoryStrategy() = default;

    /// 对话中定期触发（内部自行构造 LLM 调用）
    virtual void TrySegmentConsolidation(AgentSession& /*session*/) {}
    /// 会话退出时触发
    virtual void TryExitConsolidation(AgentSession& /*session*/) {}
};

// 定长触发策略
class LengthMemoryStrategy : public MemoryStrategy {
public:
    explicit LengthMemoryStrategy(int threshold = 30) : threshold_(threshold) {}
    std::string Name() const { return "length"; }
    void SetThreshold(int n) { threshold_ = n; }
    int GetThreshold() const { return threshold_; }
    void TrySegmentConsolidation(AgentSession& session) override;
private:
    int threshold_ = 30;
};

// 退出总结策略
class ExitMemoryStrategy : public MemoryStrategy {
public:
    void TryExitConsolidation(AgentSession& session) override;
};

// 复合策略
class CompositeMemoryStrategy : public MemoryStrategy {
public:
    void Add(std::shared_ptr<MemoryStrategy> s) { strategies_.push_back(std::move(s)); }
    void TrySegmentConsolidation(AgentSession& session) override {
        for (auto& s : strategies_) s->TrySegmentConsolidation(session);
    }
    void TryExitConsolidation(AgentSession& session) override {
        for (auto& s : strategies_) s->TryExitConsolidation(session);
    }
private:
    std::vector<std::shared_ptr<MemoryStrategy>> strategies_;
};

// ============================================================================
// 记忆工具函数
// ============================================================================

/// 提取消息段摘要 + 关键决策
ConsolidationResult ConsolidateSegment(const std::vector<MessageSchema>& messages,
                                        int start_index, int end_index,
                                        MemoryLlmCallback llm_callback);

/// 追加结果到角色记忆文件
void AppendToRoleMemory(const AgentSession& session,
                        const ConsolidationResult& result,
                        const std::string& category = "decisions");

/// 保存每日记忆
void SaveDailyMemory(const std::string& role_memory_dir,
                     const std::string& content,
                     const std::string& date_str = "");

/// 创建默认记忆策略
std::shared_ptr<MemoryStrategy> CreateDefaultMemoryStrategy();

// ============================================================================
// MemoryManager — 工作区文件管理
// ============================================================================

class MemoryManager {
 public:
    explicit MemoryManager(const std::filesystem::path& workspace_path);
    ~MemoryManager();

    void LoadWorkspaceFiles();

    std::string ReadIdentityFile(const std::string& filename) const;
    std::string ReadAgentsFile() const;
    std::string ReadToolsFile() const;
    std::vector<std::string> SearchMemory(const std::string& query) const;

    void SaveDailyMemory(const std::string& content);

    using FileChangeCallback = std::function<void(const std::string& filename)>;
    void StartFileWatcher();
    void StopFileWatcher();
    void SetFileChangeCallback(FileChangeCallback cb);

    const std::filesystem::path& GetWorkspacePath() const;
    void SetAgentWorkspace(const std::string& agent_id);
    std::filesystem::path GetBaseDir() const;
    std::filesystem::path GetSessionsDir(const std::string& agent_id = "main") const;
    std::string GetWorkspace() const { return workspace_path_.string(); }

 private:
    bool IsMemoryFile(const std::filesystem::path& filepath) const;
    std::string ReadFileContent(const std::filesystem::path& filepath) const;
    void WriteFileContent(const std::filesystem::path& filepath,
                          const std::string& content) const;
    void LoadProsophorFilesRecursively(const std::filesystem::path& dir);

    std::filesystem::path workspace_path_;
    std::filesystem::path base_dir_;
    std::string agent_id_;
    std::atomic<bool> watching_{false};
    std::mutex watcher_mutex_;
    FileChangeCallback change_callback_;
    std::unordered_map<std::string, std::filesystem::file_time_type> file_mtimes_;
    std::unique_ptr<std::thread> watcher_thread_;
};

}  // namespace prosophor
