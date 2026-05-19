// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "managers/agent_role_loader.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>

#ifndef PROSOPHOR_SOURCE_DIR
#define PROSOPHOR_SOURCE_DIR "."
#endif

#include "common/log_wrapper.h"
#include "config/config.h"
#include "tools/tool_registry.h"
#include "managers/skill_loader.h"
#include "core/dialog_strategy.h"

namespace prosophor {

AgentRoleLoader& AgentRoleLoader::GetInstance() {
    static AgentRoleLoader instance;
    return instance;
}

AgentRole AgentRoleLoader::LoadRole(const std::string& role_path) {
    std::ifstream ifs(role_path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open role file: " + role_path);
    }

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse role JSON: " + role_path + " — " + e.what());
    }

    std::string file_stem = std::filesystem::path(role_path).stem().string();
    return ParseFromJson(j, file_stem);
}

std::vector<AgentRole> AgentRoleLoader::LoadAllRoles(const std::string& roles_dir) {
    std::vector<AgentRole> roles;

    if (!std::filesystem::exists(roles_dir)) {
        LOG_WARN("Roles directory does not exist: {}", roles_dir);
        return roles;
    }

    for (const auto& entry : std::filesystem::directory_iterator(roles_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            try {
                AgentRole role = LoadRole(entry.path().string());
                roles.push_back(role);
                LOG_DEBUG("Loaded role: {} ({})", role.name, role.id);
            } catch (const std::exception& e) {
                LOG_ERROR("Failed to load role {}: {}", entry.path().string(), e.what());
            }
        }
    }

    return roles;
}

AgentRole AgentRoleLoader::ParseFromJson(const nlohmann::json& j, const std::string& file_stem) const {
    AgentRole role;
    role.id = j.value("id", file_stem);
    role.name = j.value("display_name", j.value("name", role.id));
    role.description = j.value("description", "");
    role.sprite_id = j.value("sprite_id", "");
    role.sprite_assets_dir = j.value("sprite_assets_dir", "");

    // Read provider/model from "llm" block
    const nlohmann::json* llm = nullptr;
    if (auto it = j.find("llm"); it != j.end() && it->is_object()) llm = &*it;
    role.provider_prot = llm ? llm->value("protocal", "") : j.value("provider_prot", "");
    role.model = llm ? llm->value("model", std::string("")) : j.value("model", std::string(""));

    // Support combined "provider:model" format in model field
    if (!role.model.empty()) {
        auto colon_pos = role.model.find(':');
        if (colon_pos != std::string::npos) {
            std::string extracted_provider = role.model.substr(0, colon_pos);
            std::string extracted_model = role.model.substr(colon_pos + 1);
            if (role.provider_prot.empty()) {
                role.provider_prot = extracted_provider;
                role.model = extracted_model;
            }
        }
    }

    // Resolve provider and agent config
    auto& config = ProsophorConfig::GetInstance();
    std::string provider_to_use = role.provider_prot;

    if (provider_to_use.empty()) {
        std::string primary_role = config.default_role.empty() ? "default" : config.default_role[0];
        std::string default_role_path = "config/.prosophor/roles/" + primary_role + ".json";
        if (std::filesystem::exists(default_role_path)) {
            auto& loader = AgentRoleLoader::GetInstance();
            try {
                AgentRole default_role = loader.LoadRole(default_role_path);
                if (!default_role.provider_prot.empty()) {
                    provider_to_use = default_role.provider_prot;
                    LOG_DEBUG("Role using default provider '{}' from default_role '{}'",
                             role.id, provider_to_use, primary_role);
                }
            } catch (const std::exception& e) {
                LOG_WARN("Failed to load default role '{}', using fallback: {}", primary_role, e.what());
            }
        }

        if (provider_to_use.empty() && !config.providers.empty()) {
            provider_to_use = config.providers.begin()->first;
            LOG_DEBUG("Role using first available provider: {}", role.id, provider_to_use);
        }
    }

    // Resolve model/agent config from provider
    if (!provider_to_use.empty()) {
        auto provider_it = config.providers.find(provider_to_use);
        if (provider_it != config.providers.end()) {
            auto& agent_map = provider_it->second.agents;

            // Try "{provider}/{model}" first, then bare model name
            auto agent_it = agent_map.find(provider_to_use + "/" + role.model);
            if (agent_it == agent_map.end()) {
                agent_it = agent_map.find(role.model);
            }
            if (agent_it == agent_map.end() && !agent_map.empty()) {
                // Fallback: use first available agent
                agent_it = agent_map.begin();
            }

            if (agent_it != agent_map.end()) {
                role.temperature = agent_it->second.temperature;
                role.max_tokens = agent_it->second.max_tokens;
                role.model = agent_it->second.model;
                role.enable_streaming = agent_it->second.enable_streaming;
                role.thinking = agent_it->second.thinking;
                LOG_DEBUG("Role '{}' using agent model='{}' from provider '{}': temperature={}, max_tokens={}, enable_streaming={}",
                         role.id, agent_it->second.model, provider_to_use,
                         role.temperature, role.max_tokens, role.enable_streaming);
            } else {
                LOG_WARN("Role {}: no agents configured in provider '{}', using hardcoded defaults", role.id, provider_to_use);
            }
        }
    }

    // 性格配置
    role.personality = j.value("personality", "default");
    role.personality_prompt = j.value("personality_prompt", "");
    role.role_system_prompt = j.value("system_prompt", "");

    // sprite_id → 查找 sprite JSON，同名覆盖 role 字段
    if (!role.sprite_id.empty()) {
        nlohmann::json sj;  // sprite JSON overlay

        // 1. config sprite_assets_dir/{sprite_id}/meta.json
        if (!config.sprite_assets_dir.empty()) {
            std::string meta_path = config.sprite_assets_dir + "/" + role.sprite_id + "/meta.json";
            std::ifstream mf(meta_path);
            if (mf) { try { mf >> sj; } catch (...) {} }
        }

        // 2. Fallback: petdex-sprites recursive search
        if (sj.empty()) {
            static const char* kPetdexDir = PROSOPHOR_SOURCE_DIR "/assets/petdex-sprites/by-collection";
            if (!std::filesystem::exists(kPetdexDir)) {
                LOG_WARN("Petdex directory not found: {}", kPetdexDir);
            } else {
                for (const auto& entry : std::filesystem::recursive_directory_iterator(kPetdexDir)) {
                    if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
                    try {
                        std::ifstream pf(entry.path());
                        nlohmann::json pj;
                        pf >> pj;
                        if (pj.value("id", "") == role.sprite_id) { sj = std::move(pj); break; }
                    } catch (const std::exception& e) {
                        LOG_WARN("Failed to read petdex entry {}: {}", entry.path().string(), e.what());
                    }
                }
            }
        }

        // 3. Override role fields with sprite fields (同名覆盖)
        if (!sj.empty()) {
            if (auto it = sj.find("display_name"); it != sj.end() && it->is_string())
                role.name = it->get<std::string>();
            if (auto it = sj.find("description"); it != sj.end() && it->is_string())
                role.description = it->get<std::string>();
            if (auto it = sj.find("personality"); it != sj.end() && it->is_string())
                role.personality = it->get<std::string>();
            if (auto it = sj.find("personality_prompt"); it != sj.end() && it->is_string())
                role.personality_prompt = it->get<std::string>();
            if (auto it = sj.find("system_prompt"); it != sj.end() && it->is_string())
                role.role_system_prompt = it->get<std::string>();
            LOG_DEBUG("Sprite overlay applied for sprite_id='{}': name='{}', desc='{}', personality='{}'",
                      role.sprite_id, role.name, role.description, role.personality);
        }
    }

    // Read LLM-specific fields (skills, tools, etc.) from "llm" block
    const nlohmann::json* llm_config = llm ? llm : &j;

    // 技能配置 - 支持通配符 "*"
    if (llm_config->contains("skills_white_list")) {
        auto skills_config = (*llm_config)["skills_white_list"];
        if (skills_config.is_array() && skills_config.size() == 1 &&
            skills_config[0] == "*") {
            auto& skill_loader = SkillLoader::GetInstance();
            role.skills = skill_loader.GetAllSkillIds();
            LOG_DEBUG("Role uses all skills: {}", role.id, role.skills.size());
        } else if (skills_config.is_array()) {
            role.skills = skills_config.get<std::vector<std::string>>();
        } else if (skills_config.is_string()) {
            role.skills.push_back(skills_config.get<std::string>());
        }
    }

    // 工具配置 - 支持通配符 "*"
    bool tools_explicitly_configured = llm_config->contains("tools_white_list");
    if (tools_explicitly_configured) {
        auto tools_config = (*llm_config)["tools_white_list"];
        if (tools_config.is_array() && tools_config.size() == 1 &&
            tools_config[0] == "*") {
            auto& tool_registry = ToolRegistry::GetInstance();
            role.tools = tool_registry.GetToolSchemas();
            role.auto_confirm_tools = true;
            LOG_DEBUG("Role uses all tools with auto_confirm=true: {}", role.id, role.tools.size());
        } else if (tools_config.is_array()) {
            auto& tool_registry = ToolRegistry::GetInstance();
            auto all_tools = tool_registry.GetToolSchemas();
            std::vector<std::string> tool_names = tools_config.get<std::vector<std::string>>();
            for (const auto& tool : all_tools) {
                if (std::find(tool_names.begin(), tool_names.end(), tool.name) != tool_names.end()) {
                    role.tools.push_back(tool);
                }
            }
        } else if (tools_config.is_string()) {
            auto& tool_registry = ToolRegistry::GetInstance();
            auto all_tools = tool_registry.GetToolSchemas();
            std::string tool_name = tools_config.get<std::string>();
            for (const auto& tool : all_tools) {
                if (tool.name == tool_name) {
                    role.tools.push_back(tool);
                    break;
                }
            }
        }
    }

    role.max_iterations = llm_config->value("max_iterations", 15);
    role.auto_confirm_tools = llm_config->value("auto_confirm_tools", false);
    role.enable_streaming = llm_config->value("enable_streaming", true);
    role.enable_summary = llm_config->value("enable_summary", true);

    role.memory_dir = llm_config->value("memory_dir", std::string(""));
    if (role.memory_dir.empty()) {
        role.memory_dir = ProsophorConfig::ExpandHome(
            "~/.prosophor/memories/" + role.id);
    }

    // 默认加载所有工具（如果没有显式指定 tools_white_list）
    if (!tools_explicitly_configured && role.tools.empty()) {
        auto& tool_registry = ToolRegistry::GetInstance();
        role.tools = tool_registry.GetToolSchemas();
    }

    // 初始化默认对话策略
    role.dialog_strategy = DialogStrategy::CreateDefault();

    return role;
}

}  // namespace prosophor
