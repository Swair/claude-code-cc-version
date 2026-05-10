// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/messages_schema.h"
#include "core/agent_session.h"
#include "core/compact_service.h"  // For GetCompactService()
#include "core/agent_types.h"

namespace prosophor {

/// AgentCore: orchestrates message processing, tool execution, and LLM interaction
/// Stateless utility class - all state is in AgentSession
class AgentCore {
 public:
    /// Process a message - streaming mode is determined by session.role->enable_streaming
    /// @param message User message
    /// @param session Agent session (read/write) - contains tool_executor, stop_requested, role
    static void Loop(const std::string& message, AgentSession& session);

    /// Get compact service for context compaction
    static CompactService& GetCompactService() { return CompactService::GetInstance(); }

 private:
    /// Build ChatRequest from AgentSession
    static ChatRequest BuildRequest(const AgentSession& session);

    /// Process @file references in user message
    static std::string ProcessFileRefs(const std::string& message, const AgentSession& session);

    /// Check and perform context compaction if needed
    static void MaybeCompact(AgentSession& session);

    /// Get max iterations from role or default
    static int GetMaxIterations(const AgentSession& session);

    /// Execute tool calls and append to the given assistant message
    static bool ExecuteToolCalls(const std::vector<ToolUseSchema>& tool_calls,
                                 AgentSession& session,
                                 MessageSchema& assistant_msg,
                                 std::string& accumulated_text,
                                 int& iterations);
};

}  // namespace prosophor
