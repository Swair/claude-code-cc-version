// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "common/noncopyable.h"
#include "config/config.h"
#include "core/messages_schema.h"
#include "core/agent_types.h"
#include "components/ui_types.h"

namespace prosophor {

// Forward declarations
class MemoryManager;
class ToolRegistry;
class AgentSessionManager;
class LlmProviderRouter;
class CommandRegistry;

/// AgentEngine: core business logic shared by all frontends (Terminal, SDL, etc.)
/// Manages sessions, tools, providers, and commands.
/// Frontends register callbacks to receive output and handle permissions
/// and hold session_ids to manage multi-character conversations.
class AgentEngine : public Noncopyable {
 public:
    static AgentEngine& GetInstance();

    using OutputCallback = std::function<void(
        const std::string& session_id,
        const std::string& role_id,
        AgentRuntimeState state,
        const std::string& state_msg,
        const std::optional<MessageSchema>& reply)>;

    using PermissionCallback = std::function<bool(
        const std::string& tool_name,
        const nlohmann::json& input,
        const std::string& reason)>;

    /// Change workspace path at runtime
    void ChangeWorkspace(const std::string& new_path);

    void SetOutputCallback(OutputCallback cb);
    void SetPermissionCallback(PermissionCallback cb);

    // ── Session API (session_id owned by caller) ────────────────────────────
    /// Create a new session for the given role; returns its session_id.
    std::string CreateSession(const std::string& role_id, const std::string& task_desc = "");

    /// Send a user message to a specific session (handles /slash commands internally).
    void SendUserMessage(const std::string& session_id, const std::string& text);

    /// Stop a specific session.
    void StopSession(const std::string& session_id);

    /// Switch an existing session to a different role (preserves history).
    void SwitchRole(const std::string& session_id, const std::string& new_role_id);

    /// Get a snapshot of the focused (last active) session for UI rendering.
    std::optional<RenderSnapshot> GetFocusedSessionSnapshot();

    /// Get a snapshot of a specific session (for per-sprite rendering).
    std::optional<RenderSnapshot> GetSessionSnapshot(const std::string& session_id);


    // ── Commands ────────────────────────────────────────────────────────────
    /// Execute a slash command in the context of the given session.
    bool HandleCommand(const std::string& line, const std::string& session_id);

    // ── Info ────────────────────────────────────────────────────────────────
    std::vector<std::string> ListRoles() const;
    std::vector<std::string> ListSessions() const;

    const ProsophorConfig& GetConfig() const { return config_; }
    MemoryManager& GetMemoryManager() const { return *memory_manager_; }

 private:
    AgentEngine();
    ~AgentEngine();

    void InitializeComponents();

    ProsophorConfig config_;
    ModelConfig model_config_;
    std::string workspace_path_;

    std::shared_ptr<MemoryManager> memory_manager_;
    ToolRegistry*        tool_registry_   = nullptr;
    AgentSessionManager* session_manager_ = nullptr;
    LlmProviderRouter*  provider_router_ = nullptr;
    CommandRegistry*     command_registry_ = nullptr;

};

}  // namespace prosophor
