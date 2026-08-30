// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>

#include "core/messages_schema.h"

namespace prosophor {

/// Agent 运行时状态
enum class AgentRuntimeState {
    IDLE,
    BEGINNING,
    EXECUTING_TOOL,
    TOOL_USE,
    WAITING_PERMISSION,
    STATE_ERROR,
    COMPLETE,
    STREAM_CONTENT_TYPING,    // 流式响应中
    STREAM_MODE_COMPLETE,     // 流式响应完成
    STREAM_THINKING_START,    // 流式思考开始
    STREAM_THINKING,          // 流式思考中
    STREAM_THINKING_END,      // 流式思考结束
    STREAM_CONTENT_START,     // 流式内容开始
    STREAM_CONTENT_END,       // 流式内容结束
    STREAM_TOOL_START,        // 流式工具调用开始
    STREAM_TOOL,              // 流式工具调用中（缓冲工具内容，不返回给用户）
    STREAM_TOOL_END,          // 流式工具调用结束
};

/// 会话类型：IM 场景下 direct(单聊)/group(群聊)决定记忆注入范围
enum class SessionType {
    kDirect,
    kGroup,
};

/// 序列化为 JSONL "session_type" 字段值
inline std::string SessionTypeToString(SessionType t) {
    return t == SessionType::kGroup ? "group" : "direct";
}

/// 反序列化；未知值兜底为 kDirect
inline SessionType SessionTypeFromString(const std::string& s) {
    return s == "group" ? SessionType::kGroup : SessionType::kDirect;
}

/// 渲染快照：UI 每帧从 AgentEngine 取一次，不再持有副本
struct RenderSnapshot {
    std::string session_id;
    std::string role_id;
    AgentRuntimeState state = AgentRuntimeState::IDLE;
    std::string state_message;
    std::vector<MessageSchema> messages;
    std::string streaming_text;
    std::string streaming_thinking;
    float streaming_token_speed = 0.0f;  // tokens/s during streaming
};

}  // namespace prosophor
