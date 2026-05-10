// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "core/agent_core.h"

#include <chrono>
#include <sstream>
#include <thread>
#include <atomic>

#include <nlohmann/json.hpp>
#include "common/log_wrapper.h"

#include "common/constants.h"
#include "managers/skill_loader.h"
#include "core/compact_service.h"
#include "managers/token_tracker.h"
#include "core/reference_parser.h"
#include "tools/tool_registry.h"

namespace prosophor {

// Truncates a tool result if it exceeds the limit
static std::string TruncateToolResult(const std::string& result,
                                      int max_chars, int keep_lines) {
    if (static_cast<int>(result.size()) <= max_chars) return result;

    std::vector<std::string> lines;
    std::istringstream stream(result);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }

    if (static_cast<int>(lines.size()) <= keep_lines * 2) {
        return result;
    }

    std::string truncated;
    for (int i = 0; i < keep_lines; ++i) {
        truncated += lines[i] + "\n";
    }

    int omitted = static_cast<int>(lines.size()) - keep_lines * 2;
    truncated += "\n... [" + std::to_string(omitted) + " lines omitted] ...\n\n";
    for (int i = static_cast<int>(lines.size()) - keep_lines; i < static_cast<int>(lines.size()); ++i) {
        truncated += lines[i] + "\n";
    }
    return truncated;
}

/// Execute tool calls and build messages - shared by CloseLoop and CloseLoopStream
bool AgentCore::ExecuteToolCalls(const std::vector<ToolUseSchema>& tool_calls,
                                  AgentSession& session,
                                  MessageSchema& assistant_msg,
                                  std::string& accumulated_text,
                                  int& iterations) {
    if (tool_calls.empty()) {
        return false;
    }

    LOG_DEBUG("LLM requested {} tool calls", tool_calls.size());

    // Build results message
    MessageSchema results_msg;
    results_msg.role = "user";
    bool has_tool_error = false;

    for (const auto& tc : tool_calls) {
        // Add tool call to assistant message
        assistant_msg.AddToolUseContent(tc.id, tc.name, tc.arguments);
        session.SetOutput(AgentRuntimeState::EXECUTING_TOOL, std::string("Tool using: ") + tc.name + ", args: " + tc.arguments.dump());
        try {
            auto result = session.tool_executor_(tc.name, tc.arguments);
            // Only truncate successful results, keep error output intact
            result = TruncateToolResult(result, kToolResultMaxChars, kToolResultKeepLines);
            session.SetOutput(AgentRuntimeState::EXECUTING_TOOL, std::string("Tool using: ") + tc.name + ", result: " + result);
            results_msg.AddToolResultContent(tc.id, result);
        } catch (const std::exception& e) {
            // Tool execution failed - DON'T truncate error message
            // Full error context is critical for LLM to diagnose and fix the issue
            has_tool_error = true;
            std::string error_result = e.what();
            session.SetOutput(AgentRuntimeState::STATE_ERROR, std::string("Tool using: ") + tc.name + ", error_result: " + error_result);
            results_msg.AddToolResultContent(tc.id, error_result);
        }
    }

    // Set EXECUTING_TOOL state and add messages to history
    session.SetOutput(AgentRuntimeState::TOOL_USE, "", assistant_msg);
    session.SetOutput(AgentRuntimeState::TOOL_USE, "", results_msg);

    iterations++;

    // If tool had errors, LLM will see the error and can decide what to do next
    if (has_tool_error) {
        session.SetOutput(AgentRuntimeState::STATE_ERROR, "Tool execution had errors, continuing to let LLM handle");
    }

    // Clear accumulated text after tool execution
    accumulated_text.clear();

    return true;
}

std::string AgentCore::ProcessFileRefs(const std::string& message, const AgentSession& session) {
    std::string processed_message = message;

    if (message.empty() || !ReferenceParser::HasFileRefs(message)) {
        return processed_message;
    }

    auto file_refs = ReferenceParser::ParseFileRefs(message, session.GetWorkingDirectory());

    // Load file contents
    for (auto& ref : file_refs) {
        if (ref.exists) {
            LOG_INFO("Loaded file reference: {}", ref.path);
        } else {
            LOG_WARN("File reference not found: {}", ref.path);
        }
    }

    // Replace @file with actual content
    processed_message = ReferenceParser::ReplaceFileRefs(message, file_refs);

    return processed_message;
}

void AgentCore::MaybeCompact(AgentSession& session) {
    auto& compact_service = CompactService::GetInstance();

    if (!compact_service.NeedsCompaction(session.GetMessages())) {
        return;
    }

    LOG_INFO("Context compaction triggered");

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
        req.api_key = session.GetApiKey();
        req.timeout = session.GetTimeout();
        req.AddUserMessage(prompt);
        return session.GetProvider()->Chat(req).content_text;
    };

    auto compact_result = compact_service.Compact(session.GetMessages(), llm_callback);
    session.CompactHistory(compact_result.kept_messages, compact_result.summary);

    LOG_INFO("Compaction complete: removed {} messages, saved ~{} tokens",
             compact_result.messages_removed, compact_result.tokens_saved);
}

ChatRequest AgentCore::BuildRequest(const AgentSession& session) {
    ChatRequest req;

    if (!session.GetRole()) { LOG_FATAL("session.GetRole() is null"); abort(); }
    if (session.GetBaseUrl().empty()) { LOG_FATAL("session.GetBaseUrl() is empty"); abort(); }
    bool _local = session.GetBaseUrl().find("localhost") != std::string::npos
        || session.GetBaseUrl().find("127.0.0.1") != std::string::npos;
    if (!_local && session.GetApiKey().empty()) { LOG_FATAL("session.GetApiKey() is empty"); abort(); }
    if (session.GetTimeout() <= 0) {
        LOG_WARN("session.GetTimeout() is invalid ({}), using default 60s", session.GetTimeout());
        req.timeout = 60;
    } else {
        req.timeout = session.GetTimeout();
    }
    req.model = session.GetRole()->model;
    req.temperature = session.GetRole()->temperature;
    req.max_tokens = session.GetRole()->max_tokens;
    req.thinking = session.GetRole()->thinking;
    req.base_url = session.GetBaseUrl();
    req.api_key = session.GetApiKey();

    req.messages = session.GetMessages();
    req.system = session.GetSystemPrompt();
    // 根据 role.tools 是否为空来判断是否发送工具（tools_white_list 字段配置了才发送）
    req.tools = session.GetUseTools() && session.GetRole() && !session.GetRole()->tools.empty()
                ? session.GetRole()->tools : std::vector<ToolsSchema>{};
    req.tool_choice_auto = session.GetUseTools() && session.GetRole() && !session.GetRole()->tools.empty();

    LOG_DEBUG("BuildRequest: use_tools={}, role->tools.size()={}, req.tools.size()={}",
             session.GetUseTools(),
             session.GetRole() ? session.GetRole()->tools.size() : 0,
             req.tools.size());
    LOG_DEBUG("BuildRequest: model='{}', base_url='{}', timeout={}s, api_key={}",
             req.model, req.base_url, req.timeout,
             req.api_key.size() > 8 ? req.api_key.substr(0, 8) + "..." : req.api_key);
    LOG_DEBUG("BuildRequest: thinking={}, temperature={}, max_tokens={}",
             req.thinking, req.temperature, req.max_tokens);
    return req;
}

int AgentCore::GetMaxIterations(const AgentSession& session) {
    return session.GetRole() ? session.GetRole()->max_iterations : 15;
}

void AgentCore::Loop(const std::string& message, AgentSession& session) {
    // Determine streaming mode from role configuration
    bool streaming = session.GetRole() && session.GetRole()->enable_streaming;
    LOG_DEBUG("Processing message (streaming={})", streaming);

    // Set initial THINKING state
    session.SetOutput(AgentRuntimeState::BEGINNING, "Processing...");

    // Process message - resolve @file references
    std::string processed_message = ProcessFileRefs(message, session);

    // Add user message (with resolved references)
	if (!processed_message.empty()) {
	    session.AddUserMessage(processed_message);
	}

    // Check if compaction is needed
    MaybeCompact(session);

    int iterations = 0;
    int max_iterations = GetMaxIterations(session);

    while (iterations < max_iterations) {
        // 打断点：只在完整对话轮次之间检查，确保 messages 始终成对
        if (session.IsStopRequested()) {
            if (iterations == 0) {
                session.CleanupInterruptedLoop();  // 尚未产生任何 assistant 回复，移除孤立的 user 消息
            }
            return;
        }
        ChatRequest request = BuildRequest(session);
        request.stream = streaming;

        // Call LLM - streaming or non-streaming
        ChatResponse response;
        if (streaming) {
            response = session.GetProvider()->ChatStream(
                request, [&session](StreamEvent event, std::string content) {
                    switch (event) {
                        case StreamEvent::kThinkingStart:
                            session.SetOutput(AgentRuntimeState::STREAM_THINKING_START, "", std::nullopt);
                            break;
                        case StreamEvent::kThinkingDelta: {
                            MessageSchema thinking_msg;
                            thinking_msg.role = "assistant";
                            thinking_msg.AddThinkingContent(content);
                            session.SetOutput(AgentRuntimeState::STREAM_THINKING, "", thinking_msg);
                            break;
                        }
                        case StreamEvent::kThinkingEnd:
                            session.SetOutput(AgentRuntimeState::STREAM_THINKING_END, "", std::nullopt);
                            break;
                        case StreamEvent::kContentStart:
                            session.SetOutput(AgentRuntimeState::STREAM_CONTENT_START, "", std::nullopt);
                            break;
                        case StreamEvent::kContentDelta: {
                            MessageSchema chunk_msg;
                            chunk_msg.role = "assistant";
                            chunk_msg.AddTextContent(content);
                            session.SetOutput(AgentRuntimeState::STREAM_CONTENT_TYPING, "", chunk_msg);
                            break;
                        }
                        case StreamEvent::kContentEnd:
                            session.SetOutput(AgentRuntimeState::STREAM_CONTENT_END, "", std::nullopt);
                            break;
                        default:
                            break;
                    }
                });
        } else {
            response = session.GetProvider()->Chat(request);
        }

        // Record token usage for both streaming and non-streaming paths
        if (response.usage.total_tokens > 0) {
            RecordTokenUsage(request.model, response.usage);
        }

        // Check for API error
        if (!response.error_msg.empty()) {
            LOG_ERROR("API error: {}", response.error_msg);
            MessageSchema error_msg;
            error_msg.role = "assistant";
            error_msg.AddTextContent("[API Error] " + response.error_msg);
            session.SetOutput(AgentRuntimeState::STATE_ERROR, response.error_msg, error_msg);
            return;
        }

        // Build assistant message with thinking (if any)
        // In thinking mode, thinking blocks must be preserved even if empty
        MessageSchema assistant_msg;
        assistant_msg.role = "assistant";
        if (response.has_thinking) {
            assistant_msg.AddThinkingContent(response.content_thinking,
                                             response.thinking_signature);
        }

        // Execute tool calls if present
        if (ExecuteToolCalls(response.tool_calls, session, assistant_msg, response.content_text, iterations)) {
            continue;
        }

        // No tool calls - check for text response
        if (!response.content_text.empty()) {
            assistant_msg.AddTextContent(response.content_text);

            if (streaming) {
                session.SetOutput(AgentRuntimeState::STREAM_MODE_COMPLETE, "Done.", assistant_msg);
            } else {
                session.SetOutput(AgentRuntimeState::COMPLETE, "Done.", assistant_msg);
            }
            return;
        }

        session.SetOutput(AgentRuntimeState::STATE_ERROR, "Unexpected LLM response format");
        break;
    }

    // Max iterations (unexpected — LLM didn't complete in time)
    session.SetOutput(AgentRuntimeState::STATE_ERROR, "[Max iterations reached]");
}

}  // namespace prosophor
