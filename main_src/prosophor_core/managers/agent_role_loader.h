// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include "core/agent_role.h"

namespace prosophor {

/// AgentRoleLoader: 从 JSON 文件加载 AgentRole 配置
class AgentRoleLoader {
public:
    static AgentRoleLoader& GetInstance();

    /// 从单个 JSON 文件加载角色
    AgentRole LoadRole(const std::string& role_path);

    /// 从目录加载所有 .json 角色文件
    std::vector<AgentRole> LoadAllRoles(const std::string& roles_dir);

    /// 从 JSON 对象构建 AgentRole（核心解析逻辑）
    AgentRole ParseFromJson(const nlohmann::json& j, const std::string& file_stem) const;

private:
    AgentRoleLoader() = default;
};

}  // namespace prosophor
