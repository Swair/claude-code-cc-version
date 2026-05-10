// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <string>
#include <vector>

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

/// RGBA 颜色值
struct StateColor {
    uint8_t r, g, b, a;
};

/// 状态视觉属性
struct StateVisualProps {
    uint8_t r, g, b, a;
    std::string name;
};

/// 从 StateColor + 名称构造视觉属性
inline StateVisualProps MakeVisualProps(StateColor c, std::string name) {
    return {c.r, c.g, c.b, c.a, std::move(name)};
}

}  // namespace prosophor
