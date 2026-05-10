// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "managers/agent_session_manager.h"

#include <algorithm>
#include <chrono>
#include <random>

#include "common/log_wrapper.h"
#include "config/config.h"
#include "managers/agent_role_loader.h"
#include "common/time_wrapper.h"
#include "common/file_utils.h"
#include "managers/permission_manager.h"
#include "core/memory_consolidation_service.h"
#include "providers/provider_router.h"

namespace prosophor {

AgentSessionManager& AgentSessionManager::GetInstance() {
    static AgentSessionManager instance;
    return instance;
}

void AgentSessionManager::Initialize(ToolExecutorCallback tool_executor) {
    tool_executor_ = tool_executor;
    LOG_DEBUG("AgentSessionManager initialized");

    // Load roles from ~/.prosophor/roles/
    auto roles_dir = prosophor::ProsophorConfig::BaseDir() / "roles";
    LoadRolesFromDirectory(roles_dir.string());
}

void AgentSessionManager::SetToolExecutor(ToolExecutorCallback tool_executor) {
    tool_executor_ = tool_executor;
}

void AgentSessionManager::SetOutputCallback(SessionOutputCallback callback) {
    output_callback_ = callback;
}

void AgentSessionManager::RegisterRole(const AgentRole& role) {
    std::lock_guard<std::mutex> lock(mutex_);
    roles_[role.id] = role;
    LOG_DEBUG("Registered role: {} ({})", role.name, role.id);
}

void AgentSessionManager::LoadRolesFromDirectory(const std::string& roles_dir) {
    auto& loader = AgentRoleLoader::GetInstance();
    auto roles = loader.LoadAllRoles(roles_dir);

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& role : roles) {
        roles_[role.id] = role;
    }

    LOG_DEBUG("Loaded roles from {}", roles.size(), roles_dir);
}

const AgentRole* AgentSessionManager::GetRole(const std::string& role_id) const {
    auto it = roles_.find(role_id);
    return it != roles_.end() ? &it->second : nullptr;
}

std::vector<std::string> AgentSessionManager::ListRoles() const {
    std::vector<std::string> role_ids;
    for (const auto& [id, role] : roles_) {
        role_ids.push_back(id);
    }
    return role_ids;
}

std::string AgentSessionManager::GenerateSessionId(const std::string& role_id) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(0, 999999);

    return role_id + "-" + std::to_string(dist(gen));
}

std::string AgentSessionManager::CreateSession(const std::string& role_id,
                                               const std::string& task_desc) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = roles_.find(role_id);
    if (it == roles_.end()) {
        throw std::runtime_error("Role not found: " + role_id);
    }

    AgentRole& role = it->second;
    std::string session_id = GenerateSessionId(role_id);

    AgentSession session(session_id, task_desc, &role);
    session.SetAutoConfirmTools(role.auto_confirm_tools);
    // Per-session tool executor: temporarily elevates permission when auto_confirm is set
    {
        bool auto_confirm = session.GetAutoConfirmTools();
        ToolExecutorCallback base_executor = tool_executor_;
        session.tool_executor_ = [auto_confirm, base_executor](
                const std::string& tool_name, const nlohmann::json& args) -> std::string {
            if (auto_confirm) {
                auto& perm = PermissionManager::GetInstance();
                auto prev = perm.GetMode();
                perm.SetMode("auto");
                auto result = base_executor(tool_name, args);
                perm.SetMode(prev);
                return result;
            }
            return base_executor(tool_name, args);
        };
    }
    // Per-session output callback: set after map insert (see below)

    // Inject memory consolidation service (singleton instance)
    session.SetConsolidationService(&MemoryConsolidationService::GetInstance());

    // 初始化 Session History 目录
    auto base_dir = prosophor::ProsophorConfig::BaseDir();
    session.SetSessionHistoryDir((base_dir / "sessions" / session_id / "history").string());
    std::filesystem::create_directories(session.GetSessionHistoryDir());

    // 初始化工作目录（默认为当前工作目录）
    session.SetWorkingDirectory(std::filesystem::current_path().string());

    // 初始化 base_url/api_key/timeout（从 provider entry 中按 model 查找）
    {
        auto& config = ProsophorConfig::GetInstance();
        auto prov_it = config.providers.find(role.provider_prot);
        if (prov_it != config.providers.end()) {
            LOG_INFO("Looking up provider '{}' for model '{}'", role.provider_prot, role.model);
            std::string entry_base_url;
            std::string entry_api_key;
            int entry_timeout = 0;
            if (prov_it->second.FindEntryForModel(role.provider_prot, role.model,
                                                    entry_base_url, entry_api_key, entry_timeout)) {
                session.SetBaseUrl(entry_base_url);
                session.SetApiKey(entry_api_key);
                session.SetTimeout(entry_timeout);
                LOG_INFO("Found model-specific config: url='{}', api_key='{}...', timeout={}s",
                         entry_base_url,
                         entry_api_key.size() > 8 ? entry_api_key.substr(0, 8) : entry_api_key,
                         entry_timeout);
            } else {
                LOG_INFO("Model '{}' not found in provider '{}', using provider-level fallback",
                         role.model, role.provider_prot);
            }
            // Fallback to provider-level config if model-specific lookup fails
            if (session.GetBaseUrl().empty() && !prov_it->second.base_url.empty()) {
                session.SetBaseUrl(prov_it->second.base_url);
                LOG_INFO("Fallback to provider-level base_url='{}'", session.GetBaseUrl());
            }
            if (session.GetApiKey().empty() && !prov_it->second.api_key.empty()) {
                session.SetApiKey(prov_it->second.api_key);
                LOG_INFO("Fallback to provider-level api_key='{}...'",
                         session.GetApiKey().size() > 8 ? session.GetApiKey().substr(0, 8) : session.GetApiKey());
            }
            if (session.GetTimeout() <= 0 && prov_it->second.timeout > 0) {
                session.SetTimeout(prov_it->second.timeout);
            }
        } else {
            LOG_ERROR("Provider '{}' not found in config for session '{}'",
                      role.provider_prot, session_id);
        }
        // Final validation
        if (session.GetBaseUrl().empty()) {
            LOG_FATAL("Failed to set base_url for session '{}' (role: {}, provider: '{}')",
                      session_id, role_id, role.provider_prot);
        }
        bool is_local = session.GetBaseUrl().find("localhost") != std::string::npos
                        || session.GetBaseUrl().find("127.0.0.1") != std::string::npos;
        if (session.GetApiKey().empty() && !is_local) {
            LOG_FATAL("Failed to set api_key for session '{}' (role: {}, provider: '{}'). "
                      "Please check your settings.json provider configuration.",
                      session_id, role_id, role.provider_prot);
        }
    }

    // 构建 system prompt
    session.SetSystemPrompt(BuildSystemPrompt(session));

    sessions_[session_id] = std::make_unique<AgentSession>(std::move(session));

    // Set output_callback after map insert: raw pointer is now stable for the session's lifetime
    sessions_[session_id]->output_callback_ = output_callback_;

    LOG_DEBUG("Created session: {} for role: {} (task: {})",
             session_id, role_id, task_desc);
    LOG_DEBUG("  Role Memory: {}", role.memory_dir);
    LOG_DEBUG("  Session History: {}", sessions_[session_id]->GetSessionHistoryDir());

    return session_id;
}

std::string AgentSessionManager::SendToSession(const std::string& session_id,
                                               const std::string& message) {
    auto session = GetSessionShared(session_id);
    if (!session) {
        throw std::runtime_error("Session not found: " + session_id);
    }

    session->UpdateLastActive();
    {
        auto lock = session->ScopedLock();
        AgentCore::Loop(message, *session);
    }

    // 返回最后一条消息（assistant 回复）
    const auto& msgs = session->GetMessages();
    if (!msgs.empty()) {
        return msgs.back().text();
    }

    return "";
}

void AgentSessionManager::SendToSessionAsync(const std::string& session_id,
                                             const std::string& message) {
    // pending buffer 模式 —— 连续输入合并为一次 LLM 调用
    //
    // 时序:
    //   输入 A → 追加 → task_active=false → 提交 StartChain（只取 A，跑 Loop(A)）
    //   输入 B → 追加 → task_active=true  → return
    //   输入 C → 追加 → task_active=true  → return
    //   Loop(A) 完成后 → ContinueChain → drain [B,C] 合并 → Loop("B\n\nC")
    bool should_start;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_inputs_[session_id].push_back(message);
        should_start = !task_active_[session_id];
        task_active_[session_id] = true;
    }
    if (!should_start) return;

    auto session = GetSessionShared(session_id);
    if (!session) {
        LOG_ERROR("Session not found for async task: {}", session_id);
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_inputs_.erase(session_id);
        task_active_.erase(session_id);
        if (output_callback_) {
            output_callback_(session_id, "", AgentRuntimeState::STATE_ERROR,
                            "Session not found: " + session_id, std::nullopt);
        }
        return;
    }

    StartChain(session_id, std::move(session));
}

void AgentSessionManager::StartChain(const std::string& session_id,
                                      std::shared_ptr<AgentSession> session) {
    thread_pool_.Submit([this, session_id, session]() {
        // 生产-消费循环：每次 swap 一批 → 持锁 Loop，中间释放锁
        for (;;) {
            std::vector<std::string> batch;
            {
                std::lock_guard<std::mutex> plock(pending_mutex_);
                auto& buf = pending_inputs_[session_id];
                if (buf.empty()) {
                    task_active_[session_id] = false;
                    break;
                }
                std::swap(batch, buf);  // O(1) 取出全部，不持锁迭代
            }
            std::string merged;
            for (auto& m : batch) {
                if (!merged.empty()) merged += "\n\n";
                merged += std::move(m);
            }
            session->UpdateLastActive();
            {
                auto lock = session->ScopedLock();
                session->ClearStopRequested();
                AgentCore::Loop(merged, *session);
            }
        }
    });
}

AgentSession* AgentSessionManager::GetSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    return it != sessions_.end() ? it->second.get() : nullptr;
}

const AgentSession* AgentSessionManager::GetSession(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    return it != sessions_.end() ? it->second.get() : nullptr;
}

std::shared_ptr<AgentSession> AgentSessionManager::GetSessionShared(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return nullptr;
    }
    // unique_ptr 管理对象生命周期，shared_ptr 用 no-op deleter 共享访问
    // 安全：map rehash 只移动 unique_ptr，对象地址不变
    return std::shared_ptr<AgentSession>(it->second.get(), [](AgentSession*){});
}

std::vector<AgentSession*> AgentSessionManager::GetSessionsByRole(const std::string& role_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AgentSession*> result;
    for (auto& [id, session] : sessions_) {
        if (session->GetRole()->id == role_id && session->IsActive()) {
            result.push_back(session.get());
        }
    }
    return result;
}

std::vector<const AgentSession*> AgentSessionManager::GetSessionsByRole(const std::string& role_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<const AgentSession*> result;
    for (const auto& [id, session] : sessions_) {
        if (session->GetRole()->id == role_id && session->IsActive()) {
            result.push_back(session.get());
        }
    }
    return result;
}

std::vector<AgentSession*> AgentSessionManager::GetActiveSessions(int minutes) {
    auto now = SteadyClock::Now();
    auto threshold = std::chrono::minutes(minutes);

    std::vector<AgentSession*> result;
    for (auto& [id, session] : sessions_) {
        if (session->IsActive() && (now - session->GetLastActive()) < threshold) {
            result.push_back(session.get());
        }
    }
    return result;
}

void AgentSessionManager::CloseSession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        AgentSession& session = *it->second;

        {   // session_mutex: 确保与 Loop 路径不并发读写 session 状态
            auto session_lock = session.ScopedLock();

            auto* consolidation_service = session.GetConsolidationService();

            if (consolidation_service) {
                auto llm_callback = [&session](const std::string& prompt) -> std::string {
                    ChatRequest req;
                    if (session.GetRole()) {
                        req.model = session.GetRole()->model;
                        req.temperature = session.GetRole()->temperature;
                        req.max_tokens = 4096;
                    }
                    if (!session.GetBaseUrl().empty()) {
                        req.base_url = session.GetBaseUrl();
                    }
                    req.AddUserMessage(prompt);
                    return session.GetProvider()->Chat(req).content_text;
                };

                auto result = consolidation_service->ConsolidateSessionExit(session, llm_callback);

                if (!result.summary.empty()) {
                    LOG_DEBUG("Session exit consolidation completed for {}: {} decisions saved",
                             session_id, result.decisions.size());
                }
            }

            session.SetActive(false);
        }
        LOG_INFO("Closed session: {}", session_id);
    }

    // 清理 pending buffer
    std::lock_guard<std::mutex> plock(pending_mutex_);
    pending_inputs_.erase(session_id);
    task_active_.erase(session_id);
}

std::vector<std::string> AgentSessionManager::ListSessions() const {
    std::vector<std::string> session_ids;
    for (const auto& [id, session] : sessions_) {
        if (session->IsActive()) {
            session_ids.push_back(id);
        }
    }
    return session_ids;
}

std::string AgentSessionManager::GetLastSessionId() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string last_id;
    for (const auto& [id, session] : sessions_) {
        if (session->IsActive()) {
            last_id = id;
        }
    }
    return last_id;
}

std::string AgentSessionManager::GetOrCreateSession(const std::string& role_id,
                                                    const std::string& message_hint) {
    // 尝试找到活跃的会话
    auto sessions = GetSessionsByRole(role_id);

    // 简单策略：复用最近的活跃会话
    // TODO: 可以用语义相似度判断是否相关
    if (!sessions.empty()) {
        return sessions.back()->GetSessionId();
    }

    // 没有活跃会话，创建新的
    return CreateSession(role_id, message_hint);
}

void AgentSessionManager::BroadcastToSessions(const std::vector<std::string>& session_ids,
                                              const std::string& message) {
    // 异步发送所有消息，不等待结果
    // 每个 session 的结果通过 output_callback 通知
    for (const auto& session_id : session_ids) {
        SendToSessionAsync(session_id, message);
    }
}

void AgentSessionManager::BroadcastToRole(
    const std::string& role_id,
    const std::string& message) {

    auto sessions = GetActiveSessions(30);  // 最近 30 分钟
    std::vector<std::string> session_ids;

    for (auto* session : sessions) {
        if (session->GetRole()->id == role_id) {
            session_ids.push_back(session->GetSessionId());
        }
    }

    BroadcastToSessions(session_ids, message);
}

void AgentSessionManager::SwitchRoleForSession(const std::string& session_id,
                                                const std::string& new_role_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        throw std::runtime_error("Session not found: " + session_id);
    }

    auto role_it = roles_.find(new_role_id);
    if (role_it == roles_.end()) {
        throw std::runtime_error("Role not found: " + new_role_id);
    }

    AgentSession& session = *it->second;

    {   // session_mutex: 与 Loop 路径不并发读写 role/provider/base_url/system_prompt
        auto session_lock = session.ScopedLock();
        session.SetRole(&role_it->second);
        session.SetProvider(ProviderRouter::GetInstance().GetProviderByName(session.GetRole()->provider_prot));

        auto& config = ProsophorConfig::GetInstance();
        auto prov_it = config.providers.find(session.GetRole()->provider_prot);
        if (prov_it != config.providers.end()) {
            session.SetBaseUrl(prov_it->second.base_url);
        }

        session.SetSystemPrompt(BuildSystemPrompt(session));
    }

    LOG_INFO("Switched session {} to role: {}", session_id, new_role_id);
    LOG_INFO("  Role Memory (from new role): {}", session.GetRole()->memory_dir);
    LOG_INFO("  Session History (unchanged): {}", session.GetSessionHistoryDir());
}

std::vector<SystemSchema> AgentSessionManager::BuildSystemPrompt(const AgentSession& session) {
    std::ostringstream prompt;

    // 1. Role Memory (长期记忆 - 习惯/偏好) - 从 AgentRole 封装方法加载
    if (session.GetRole()) {
        std::string memory_content = session.GetRole()->LoadMemoryContent();
        if (!memory_content.empty()) {
            prompt << memory_content;
        }
    }

    // 2. Session History (项目上下文 - 决策/待办)
    if (!session.GetSessionHistoryDir().empty() &&
        std::filesystem::exists(session.GetSessionHistoryDir())) {
        prompt << "## 项目上下文\n\n";

        // 加载 Session History 文件
        for (const auto& entry : std::filesystem::directory_iterator(session.GetSessionHistoryDir())) {
            if (entry.is_regular_file() && entry.path().extension() == ".md") {
                auto content = ReadFile(entry.path().string());
                if (content.has_value()) {
                    prompt << "### " << entry.path().stem().string() << "\n";
                    prompt << content.value() << "\n\n";
                }
            }
        }
    }

    // 3. Role 基础 Prompt（System Prompt + Personality）
    if (session.GetRole()) {
        std::string role_prompt = session.GetRole()->BuildPrompt();
        if (!role_prompt.empty()) {
            prompt << role_prompt << "\n";
        }
    }

    return {{"text", prompt.str(), false}};
}

}  // namespace prosophor
