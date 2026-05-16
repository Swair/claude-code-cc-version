// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "agent_engine.h"

#include <iostream>
#include <filesystem>

#include "common/constants.h"
#include "common/log_wrapper.h"
#include "common/string_utils.h"
#include "common/file_utils.h"
#include "managers/memory_manager.h"
#include "managers/agent_session_manager.h"
#include "managers/agent_role_loader.h"
#include "managers/local_model_manager.h"
#include "managers/active_trigger_manager.h"
#include "command_registry.h"
#include "tools/tool_registry.h"
#include "providers/provider_router.h"
#include "services/lsp_manager.h"

namespace prosophor {

AgentEngine& AgentEngine::GetInstance() {
    static AgentEngine instance;
    return instance;
}

AgentEngine::AgentEngine()
    : workspace_path_(std::filesystem::current_path().string()) {
    InitializeComponents();
}

AgentEngine::~AgentEngine() {
    if (memory_manager_) {
        memory_manager_->StopFileWatcher();
    }
    if (LocalModelManager::GetInstance().IsRunning()) {
        LocalModelManager::GetInstance().Stop();
    }
}

void AgentEngine::InitializeComponents() {
    LOG_DEBUG("Initializing AgentEngine components...");

    config_ = prosophor::ProsophorConfig::GetInstance();

    EnsureDirectory(workspace_path_);

    memory_manager_ = std::make_shared<MemoryManager>(workspace_path_);
    memory_manager_->LoadWorkspaceFiles();
    memory_manager_->StartFileWatcher();

    tool_registry_ = &ToolRegistry::GetInstance();
    tool_registry_->SetWorkspace(workspace_path_);

    session_manager_ = &AgentSessionManager::GetInstance();

    ToolExecutorCallback tool_executor =
        [this](const std::string& tool_name, const nlohmann::json& args) -> std::string {
            return tool_registry_->ExecuteTool(tool_name, args);
        };

    session_manager_->Initialize(tool_executor);

    provider_router_ = &ProviderRouter::GetInstance();
    provider_router_->Initialize(config_);

    auto& lsp_manager = prosophor::LspManager::GetInstance();
    lsp_manager.Initialize();
    LOG_DEBUG("LSP integration initialized with {} servers",
              lsp_manager.GetRegisteredServers().size());

    command_registry_ = &CommandRegistry::GetInstance();
    command_registry_->Initialize();

    // ── 主动触发管理器 ──────────────────────────────────────────
    auto& active_trigger = ActiveTriggerManager::GetInstance();
    active_trigger.Initialize("~/.prosophor/active");

    active_trigger.SetLlmExecuteCallback(
        [this](const std::string& /*session_id*/, const std::string& trigger_reason,
               const std::string& prompt_md) -> std::string {
            auto& config = ProsophorConfig::GetInstance();
            auto provider = provider_router_->GetDefaultProvider();
            if (!provider) {
                LOG_ERROR("No default provider available for active trigger");
                return "";
            }

            std::string default_provider_name = provider_router_->GetProviderName("");
            auto prov_it = config.providers.find(default_provider_name);
            if (prov_it == config.providers.end()) {
                LOG_ERROR("Default provider '{}' not found in config", default_provider_name);
                return "";
            }

            const auto& agent_config = prov_it->second.GetDefaultAgent();
            ChatRequest req;
            req.model = agent_config.model;
            req.temperature = agent_config.temperature;
            req.max_tokens = agent_config.max_tokens;
            req.base_url = prov_it->second.base_url;
            req.api_key = prov_it->second.api_key;
            req.timeout = prov_it->second.timeout;

            std::string user_message = "触发事件：" + trigger_reason + "\n\n" + prompt_md;
            req.AddUserMessage(user_message);

            return provider->Chat(req).content_text;
        });

    active_trigger.SetUserInteractionCallback([this]() -> bool {
        return !session_manager_->GetActiveSessions(5).empty();
    });

    active_trigger.SetSessionManager(session_manager_);
    active_trigger.Start();

    LOG_DEBUG("ActiveTriggerManager initialized and started");
    LOG_DEBUG("AgentEngine initialized");

    if (!config_.local_models.empty() && config_.local_models[0].auto_start) {
        auto& lm = config_.local_models[0];
        if (!LocalModelManager::GetInstance().Start(lm)) {
            LOG_WARN("Failed to auto-start local model server.");
        } else {
            LOG_INFO("Local model server started on port {}", lm.port);
        }
    }
}

void AgentEngine::SetOutputCallback(OutputCallback cb) {
    session_manager_->SetOutputCallback(std::move(cb));
}

void AgentEngine::SetPermissionCallback(PermissionCallback cb) {
    tool_registry_->SetPermissionConfirmCallback(std::move(cb));
}

void AgentEngine::SendUserMessage(const std::string& session_id, const std::string& text) {
    if (!text.empty() && text[0] == '/') {
        HandleCommand(text, session_id);
        return;
    }
    try {
        session_manager_->SendToSessionAsync(session_id, text);
    } catch (const std::exception& e) {
        LOG_ERROR("SendUserMessage error (session={}): {}", session_id, e.what());
    }
}

bool AgentEngine::HandleCommand(const std::string& line, const std::string& session_id) {
    if (line.empty() || line[0] != '/') {
        return false;
    }

    std::vector<std::string> args = CommandRegistry::ParseCommandLine(line);
    if (args.empty()) {
        return false;
    }

    std::string cmd_name = args[0].substr(1);
    std::vector<std::string> cmd_args(args.begin() + 1, args.end());

    if (cmd_name == "role" && !cmd_args.empty()) {
        SwitchRole(session_id, cmd_args[0]);
        return true;
    }

    CommandContext ctx;
    ctx.workspace   = workspace_path_;
    ctx.session_id  = session_id;
    ctx.user_data   = this;
    ctx.agent_session = session_manager_->GetSession(session_id);

    std::cout << ColorCode::kGreen << "/" << cmd_name;
    for (const auto& a : cmd_args) std::cout << " " << a;
    std::cout << ColorCode::kReset << std::endl;

    auto result = CommandRegistry::GetInstance().ExecuteCommand(cmd_name, cmd_args, ctx);
    if (!result.output.empty()) {
        std::cout << ColorCode::kGray << result.output << ColorCode::kReset << std::endl;
    } else if (!result.success) {
        std::cout << ColorCode::kRed << result.error << ColorCode::kReset << "\n";
    }
    return true;
}

std::string AgentEngine::CreateSession(const std::string& role_id,
                                        const std::string& task_desc) {
    return session_manager_->CreateSession(role_id, task_desc);
}

std::optional<RenderSnapshot> AgentEngine::GetFocusedSessionSnapshot() {
    return GetSessionSnapshot(session_manager_->GetLastSessionId());
}

std::optional<RenderSnapshot> AgentEngine::GetSessionSnapshot(const std::string& session_id) {
    auto* session = session_manager_->GetSession(session_id);
    if (!session) return std::nullopt;
    return session->GetSnapshot();
}

void AgentEngine::StopSession(const std::string& session_id) {
    auto* session = session_manager_->GetSession(session_id);
    if (session) {
        session->RequestStop();
    }
}

void AgentEngine::ChangeWorkspace(const std::string& new_path) {
    if (!std::filesystem::exists(new_path)) {
        LOG_ERROR("Workspace path does not exist: {}", new_path);
        return;
    }

    if (memory_manager_) {
        memory_manager_->StopFileWatcher();
    }

    workspace_path_ = new_path;
    EnsureDirectory(workspace_path_);

    // Re-create memory manager with new path
    memory_manager_ = std::make_shared<MemoryManager>(workspace_path_);
    memory_manager_->LoadWorkspaceFiles();
    memory_manager_->StartFileWatcher();

    tool_registry_->SetWorkspace(workspace_path_);

    LOG_INFO("Workspace changed to: {}", workspace_path_);
}

void AgentEngine::SwitchRole(const std::string& session_id, const std::string& new_role_id) {
    session_manager_->SwitchRoleForSession(session_id, new_role_id);
}

std::vector<std::string> AgentEngine::ListRoles() const {
    return session_manager_->ListRoles();
}

std::vector<std::string> AgentEngine::ListSessions() const {
    return session_manager_->ListSessions();
}

}  // namespace prosophor
