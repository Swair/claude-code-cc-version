// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>

#include "media_engine/media/colors.h"
#include "core/agent_types.h"
#include "core/messages_schema.h"

namespace prosophor {

/// 聊天消息结构
struct ChatMessage {
    std::string role;
    std::string name;
    std::string content;
    double timestamp;
};

/// 状态视觉属性
struct StateVisualProps {
    media_engine::Color color;
    std::string name;
};

/// 从 Color + 名称构造视觉属性
inline StateVisualProps MakeVisualProps(media_engine::Color c, std::string name) {
    return {c, std::move(name)};
}

}  // namespace prosophor
