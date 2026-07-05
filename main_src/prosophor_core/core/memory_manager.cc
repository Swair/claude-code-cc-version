// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "core/memory_manager.h"
#include "core/agent_session.h"
#include "common/time_wrapper.h"
#include "common/file_utils.h"
#include "config/config.h"

#include <sstream>
#include <regex>
#include <algorithm>
#include <fstream>

namespace prosophor {

// ============================================================================
// 文件写入锁
// ============================================================================
namespace {
std::mutex g_file_mutex;
std::unordered_map<std::string, std::unique_ptr<std::mutex>> g_file_locks;

std::unique_lock<std::mutex> GetFileLock(const std::string& path) {
    std::lock_guard<std::mutex> lock(g_file_mutex);
    auto it = g_file_locks.find(path);
    if (it == g_file_locks.end()) {
        g_file_locks.emplace(path, std::make_unique<std::mutex>());
        it = g_file_locks.find(path);
    }
    return std::unique_lock<std::mutex>(*it->second);
}

std::string BuildExtractionPrompt(const std::vector<MessageSchema>& messages) {
    std::ostringstream p;
    p << "You are a memory consolidation assistant. Analyze the conversation below "
      << "and extract key decisions, code changes, and lessons learned.\n\n"
      << "For each key point, identify:\n"
      << "1. **Type**: design_decision | code_change | unresolved_issue | lesson_learned\n"
      << "2. **Content**: Brief description of what was decided/changed\n"
      << "3. **Related Files**: Any files mentioned or modified\n\n"
      << "Format your response as:\n"
      << "```\nTYPE: <type>\nCONTENT: <content>\nFILES: <file1>, <file2>\n```\n\n"
      << "Separate multiple decisions with `---`\n\nConversation:\n";
    int start = std::max(0, (int)messages.size() - 50);
    for (size_t i = start; i < messages.size(); ++i) {
        auto& msg = messages[i];
        std::string role = (msg.role == "user") ? "User" : (msg.role == "assistant") ? "Assistant" : msg.role;
        for (auto& block : msg.content) {
            if (block.type == "text" || block.type == "thinking") {
                auto text = block.text.substr(0, 500);
                p << role << ": " << text << "\n\n";
            } else if (block.type == "tool_use") {
                p << role << " [Using tool: " << block.name << "]\n";
            } else if (block.type == "tool_result") {
                p << "[Tool result]\n\n";
            }
        }
    }
    return p.str();
}

std::string BuildSummaryPrompt(const std::vector<MessageSchema>& messages) {
    std::ostringstream p;
    p << "Please provide a concise summary of this conversation session.\n"
      << "The summary will be stored as long-term memory.\n\nInclude:\n"
      << "1. Key decisions and conclusions\n2. Code changes discussed\n"
      << "3. Files modified\n4. Unresolved issues\n"
      << "5. Important context\n\nKeep it 200-500 words.\n\nConversation:\n";
    int start = std::max(0, (int)messages.size() - 50);
    for (size_t i = start; i < messages.size(); ++i) {
        auto& msg = messages[i];
        std::string role = (msg.role == "user") ? "User" : "Assistant";
        for (auto& block : msg.content)
            if (block.type == "text" || block.type == "thinking")
                p << role << ": " << block.text.substr(0, 500) << "\n\n";
    }
    return p.str();
}

std::vector<KeyDecision> ParseDecisions(const std::string& response) {
    std::vector<KeyDecision> decisions;
    std::regex sep(R"(^---\s*$)", std::regex_constants::multiline);
    std::sregex_token_iterator it(response.begin(), response.end(), sep, -1), end;
    std::vector<std::string> segs;
    while (it != end) segs.push_back(*it++);
    std::regex tr(R"(TYPE:\s*(\S+))", std::regex_constants::icase);
    std::regex cr(R"(CONTENT:\s*(.+?)(?=FILES:|$))", std::regex_constants::icase);
    std::regex fr(R"(FILES:\s*(.+?)(?=TYPE:|$))", std::regex_constants::icase);
    for (auto& s : segs) {
        if (s.find_first_not_of(" \t\n\r") == std::string::npos) continue;
        KeyDecision d;
        d.timestamp = SystemClock::GetCurrentTimestamp();
        std::smatch m;
        d.type = (std::regex_search(s, m, tr) && m.size() > 1) ? m[1].str() : "lesson_learned";
        if (std::regex_search(s, m, cr) && m.size() > 1) {
            d.content = std::regex_replace(m[1].str(), std::regex("^\\s+|\\s+$"), "");
        }
        if (std::regex_search(s, m, fr) && m.size() > 1) {
            std::istringstream fs(m[1].str()); std::string f;
            while (std::getline(fs, f, ',')) {
                f = std::regex_replace(f, std::regex("^\\s+|\\s+$"), "");
                if (!f.empty()) d.related_files.push_back(f);
            }
        }
        if (!d.content.empty()) decisions.push_back(d);
    }
    return decisions;
}

} // anonymous namespace

// ============================================================================
// 从 JSONL 日志读取完整会话消息
// ============================================================================

std::vector<MessageSchema> ReadSessionLogs(const std::string& role_id) {
    std::vector<MessageSchema> messages;
    auto session_dir = ProsophorConfig::BaseDir() / "sessions" / role_id;
    if (!std::filesystem::exists(session_dir)) return messages;

    // 按文件名排序（日期顺序）
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(session_dir)) {
        if (entry.path().extension() == ".jsonl")
            files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& f : files) {
        std::ifstream in(f);
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            try {
                auto j = nlohmann::json::parse(line);
                std::string role = j.value("role", "");
                if (role.empty()) continue;

                MessageSchema msg;
                msg.role = role;

                if (j.contains("content")) {
                    msg.AddTextContent(j["content"].get<std::string>());
                }
                if (j.contains("tool")) {
                    std::string tool_name = j["tool"].get<std::string>();
                    auto args = j.value("args", nlohmann::json::object());
                    msg.AddToolUseContent("", tool_name, args);
                }
                if (j.contains("result")) {
                    msg.AddToolResultContent("", j["result"].get<std::string>());
                }

                messages.push_back(std::move(msg));
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to parse session log line: {}", e.what());
            }
        }
    }

    return messages;
}

// ============================================================================
// LLM 回调工厂（从 AgentSession 构造）
// ============================================================================

namespace {
MemoryLlmCallback BuildLlmCallback(AgentSession& session) {
    return [&session](const std::string& prompt) -> std::string {
        ChatRequest req;
        if (auto* r = session.GetRole()) {
            req.model = r->model;
            req.temperature = r->temperature;
            req.max_tokens = 4096;
        }
        if (!session.GetBaseUrl().empty())
            req.base_url = session.GetBaseUrl();
        req.AddUserMessage(
            "你是一个记忆整合助手。从对话中提取关键信息，分类为：\n"
            "1. design_decision — 架构/设计决策\n"
            "2. code_change — 代码变更\n"
            "3. unresolved_issue — 未解决的问题\n"
            "4. lesson_learned — 经验教训\n"
            "忽略：日常问候、临时命令、调试输出。\n"
            "每条记录必须包含时间戳上下文。\n\n"
            + prompt);
        return session.GetProvider()->Chat(req).content_text;
    };
}
} // anonymous namespace

// ============================================================================
// ConsolidateSegment
// ============================================================================

ConsolidationResult ConsolidateSegment(
    const std::vector<MessageSchema>& messages,
    int start_index, int end_index,
    MemoryLlmCallback llm) {

    ConsolidationResult result;
    if (start_index >= end_index || start_index >= (int)messages.size())
        return result;

    int end = std::min(end_index, (int)messages.size());
    std::vector<MessageSchema> seg;
    for (int i = start_index; i < end; ++i) seg.push_back(messages[i]);
    result.messages_processed = (int)seg.size();
    if (seg.empty()) return result;

    if (llm) {
        try { result.decisions = ParseDecisions(llm(BuildExtractionPrompt(seg)));
              LOG_INFO("Extracted {} decisions", result.decisions.size());
        } catch (std::exception& e) { LOG_ERROR("Extraction failed: {}", e.what()); }
        try { result.summary = llm(BuildSummaryPrompt(seg));
        } catch (std::exception& e) { LOG_ERROR("Summary failed: {}", e.what()); }
    }
    if (result.summary.empty()) {
        std::ostringstream fb;
        fb << "[Segment " << start_index << "-" << end << " " << seg.size() << " msgs]";
        result.summary = fb.str();
    }
    int chars = 0;
    for (auto& msg : seg) for (auto& b : msg.content)
        if (b.type == "text" || b.type == "thinking") chars += b.text.size();
    result.tokens_estimated = chars / 4;
    return result;
}

// ============================================================================
// AppendToRoleMemory
// ============================================================================

void AppendToRoleMemory(const AgentSession& session,
                        const ConsolidationResult& result,
                        const std::string& category) {
    auto* role = session.GetRole();
    if (!role || role->memory_dir.empty()) { LOG_WARN("No memory dir"); return; }

    auto dir = std::filesystem::path(role->memory_dir) / category;
    std::filesystem::create_directories(dir);
    std::ostringstream e;
    e << "## " << SystemClock::GetCurrentTimestamp() << "\n\n";
    if (!result.summary.empty()) e << "### Summary\n\n" << result.summary << "\n\n";
    if (!result.decisions.empty()) {
        e << "### Key Decisions\n\n";
        for (auto& d : result.decisions) {
            e << "- **[" << d.type << "]** " << d.content;
            if (!d.related_files.empty()) {
                e << " (Files: ";
                for (size_t i = 0; i < d.related_files.size(); ++i) {
                    if (i > 0) e << ", ";
                    e << d.related_files[i];
                }
                e << ")";
            }
            e << "\n";
        }
        e << "\n";
    }
    auto file = dir / (SystemClock::GetCurrentDate() + ".md");
    auto lock = GetFileLock(file.string());
    if (!WriteFile(file.string(), e.str(), true))
        LOG_ERROR("Failed write: {}", file.string());
}

// ============================================================================
// SaveDailyMemory (free function)
// ============================================================================

void SaveDailyMemory(const std::string& dir, const std::string& content,
                     const std::string& date_str) {
    auto d = date_str.empty() ? SystemClock::GetCurrentDate() : date_str;
    auto mem_dir = std::filesystem::path(dir) / "consolidation";
    std::filesystem::create_directories(mem_dir);
    std::ostringstream e;
    e << "## " << SystemClock::GetCurrentTimestamp() << "\n\n" << content << "\n";
    auto file = mem_dir / (d + ".md");
    auto lock = GetFileLock(file.string());
    if (!WriteFile(file.string(), e.str(), true))
        LOG_ERROR("Failed daily memory write: {}", file.string());
}

// ============================================================================
// LengthMemoryStrategy
// ============================================================================

void LengthMemoryStrategy::TrySegmentConsolidation(AgentSession& session) {
    auto* role = session.GetRole();
    if (!role) return;

    // 从 JSONL 日志 + 当前消息合并为完整会话历史
    auto jsonl_msgs = ReadSessionLogs(role->id);
    const auto& current_msgs = session.GetMessages();

    int total = static_cast<int>(jsonl_msgs.size() + current_msgs.size());
    int consolidated = session.GetConsolidatedCount();

    if (total < consolidated + threshold_) return;

    auto llm = BuildLlmCallback(session);
    if (!llm) return;

    // 合并消息列表
    std::vector<MessageSchema> all_msgs = std::move(jsonl_msgs);
    all_msgs.insert(all_msgs.end(), current_msgs.begin(), current_msgs.end());

    LOG_INFO("Consolidating [{},{}) total={}", consolidated, total, total);
    auto r = ConsolidateSegment(all_msgs, consolidated, total, llm);
    if (r.summary.empty()) {
        std::ostringstream fb;
        fb << "[Segment " << consolidated << "-" << total << "]";
        r.summary = fb.str();
    }
    AppendToRoleMemory(session, r, "segments");
    session.SetConsolidatedCount(total);
}

// ============================================================================
// ExitMemoryStrategy
// ============================================================================

void ExitMemoryStrategy::TryExitConsolidation(AgentSession& session) {
    auto* role = session.GetRole();
    if (!role) return;

    auto llm = BuildLlmCallback(session);
    if (!llm) return;

    // 从 JSONL 日志 + 当前消息获取完整历史
    auto jsonl_msgs = ReadSessionLogs(role->id);
    const auto& current_msgs = session.GetMessages();

    std::vector<MessageSchema> all_msgs = std::move(jsonl_msgs);
    all_msgs.insert(all_msgs.end(), current_msgs.begin(), current_msgs.end());

    if (all_msgs.empty()) return;
    LOG_INFO("Exit consolidation ({} msgs from log + {} current)",
             all_msgs.size() - current_msgs.size(), current_msgs.size());
    auto r = ConsolidateSegment(all_msgs, 0, (int)all_msgs.size(), llm);
    if (!r.summary.empty()) AppendToRoleMemory(session, r, "exit_summary");
}

// ============================================================================
// Factory
// ============================================================================

std::shared_ptr<MemoryStrategy> CreateDefaultMemoryStrategy() {
    auto c = std::make_shared<CompositeMemoryStrategy>();
    c->Add(std::make_shared<LengthMemoryStrategy>(30));
    c->Add(std::make_shared<ExitMemoryStrategy>());
    return c;
}

// ============================================================================
// MemoryManager (workspace file management)
// ============================================================================

MemoryManager::MemoryManager(const std::filesystem::path& workspace_path)
    : workspace_path_(workspace_path) {
    base_dir_ = workspace_path.parent_path();
    std::filesystem::create_directories(workspace_path_);
    LOG_INFO("MemoryManager initialized: {}", workspace_path_.string());
}

MemoryManager::~MemoryManager() { StopFileWatcher(); }

void MemoryManager::LoadWorkspaceFiles() {
    LOG_DEBUG("Loading workspace files from {}", workspace_path_.string());
    LoadProsophorFilesRecursively(workspace_path_);
}

std::string MemoryManager::ReadIdentityFile(const std::string& filename) const {
    auto path = workspace_path_ / filename;
    auto content = ReadFile(path.string());
    return content.value_or("");
}

std::string MemoryManager::ReadAgentsFile() const {
    auto path = workspace_path_ / "AGENTS.md";
    auto content = ReadFile(path.string());
    return content.value_or("");
}

std::string MemoryManager::ReadToolsFile() const {
    auto path = workspace_path_ / "TOOLS.md";
    auto content = ReadFile(path.string());
    return content.value_or("");
}

std::vector<std::string> MemoryManager::SearchMemory(const std::string& query) const {
    std::vector<std::string> results;
    if (!std::filesystem::exists(workspace_path_)) return results;
    try {
        std::string lower_query = query;
        std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
        for (auto& entry : std::filesystem::recursive_directory_iterator(workspace_path_)) {
            if (!entry.is_regular_file()) continue;
            auto ext = entry.path().extension().string();
            if (ext != ".md" && ext != ".txt") continue;
            auto content = ReadFile(entry.path().string());
            if (content) {
                std::string lower_content = content.value();
                std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);
                if (lower_content.find(lower_query) != std::string::npos)
                    results.push_back(entry.path().string());
            }
        }
    } catch (std::exception& e) {
        LOG_ERROR("SearchMemory error: {}", e.what());
    }
    return results;
}

void MemoryManager::SaveDailyMemory(const std::string& content) {
    auto date_str = SystemClock::GetCurrentDate();
    auto ts = SystemClock::GetCurrentTimestamp();
    auto mem_dir = base_dir_ / "memory";
    std::filesystem::create_directories(mem_dir);
    std::ostringstream e;
    e << "## " << ts << "\n" << content << "\n";
    auto file = mem_dir / (date_str + ".md");
    if (!WriteFile(file.string(), e.str(), true))
        throw std::runtime_error("Failed to write to memory file: " + file.string());
    spdlog::debug("Saved memory entry to {}", file.string());
}

void MemoryManager::StartFileWatcher() {
    if (watching_) return;
    watching_ = true;
    watcher_thread_ = std::make_unique<std::thread>([this]() {
        while (watching_) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            std::lock_guard<std::mutex> lock(watcher_mutex_);
            if (!watching_) break;
            try {
                for (auto& entry : std::filesystem::recursive_directory_iterator(workspace_path_)) {
                    if (!entry.is_regular_file()) continue;
                    auto path = entry.path();
                    auto fname = path.filename().string();
                    if (fname == "PROSOPHOR.md" || fname == "AGENTS.md" ||
                        fname == "TOOLS.md" || fname == "SOUL.md" || fname == "USER.md" ||
                        fname == "MEMORY.md") {
                        auto mtime = std::filesystem::last_write_time(path);
                        auto& last = file_mtimes_[path.string()];
                        if (last != std::filesystem::file_time_type{} && last != mtime) {
                            if (change_callback_) change_callback_(path.string());
                        }
                        last = mtime;
                    }
                }
            } catch (...) {}
        }
    });
}

void MemoryManager::StopFileWatcher() {
    if (!watching_) return;
    watching_ = false;
    if (watcher_thread_ && watcher_thread_->joinable()) watcher_thread_->join();
}

void MemoryManager::SetFileChangeCallback(FileChangeCallback cb) {
    change_callback_ = std::move(cb);
}

const std::filesystem::path& MemoryManager::GetWorkspacePath() const {
    return workspace_path_;
}

void MemoryManager::SetAgentWorkspace(const std::string& agent_id) {
    agent_id_ = agent_id;
    workspace_path_ = base_dir_ / "agents" / agent_id / "workspace";
    std::filesystem::create_directories(workspace_path_);
    LOG_DEBUG("SetAgentWorkspace: {}", workspace_path_.string());
}

std::filesystem::path MemoryManager::GetBaseDir() const { return base_dir_; }

std::filesystem::path MemoryManager::GetSessionsDir(const std::string& agent_id) const {
    return base_dir_ / "agents" / agent_id / "sessions";
}

bool MemoryManager::IsMemoryFile(const std::filesystem::path& filepath) const {
    auto name = filepath.filename().string();
    return name == "PROSOPHOR.md" || name == "AGENTS.md" || name == "TOOLS.md" ||
           name == "SOUL.md" || name == "USER.md" || name == "MEMORY.md";
}

std::string MemoryManager::ReadFileContent(const std::filesystem::path& path) const {
    auto content = ReadFile(path.string());
    return content.value_or("");
}

void MemoryManager::WriteFileContent(const std::filesystem::path& path,
                                      const std::string& content) const {
    WriteFile(path.string(), content);
}

void MemoryManager::LoadProsophorFilesRecursively(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) return;
    try {
        for (auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().filename() == "PROSOPHOR.md") {
                LOG_DEBUG("Found PROSOPHOR.md: {}", entry.path().string());
            }
        }
    } catch (std::exception& e) {
        LOG_ERROR("LoadProsophorFiles error: {}", e.what());
    }
}

}  // namespace prosophor
