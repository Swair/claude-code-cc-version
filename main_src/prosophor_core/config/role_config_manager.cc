// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "config/role_config_manager.h"
#include "config/config.h"
#include "managers/agent_session_manager.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"

namespace prosophor {

bool RoleConfigManager::SaveModel(const std::string& role_id,
                                  const std::string& provider,
                                  const std::string& model) {
    auto rp = ProsophorConfig::BaseDir() / "roles" / (role_id + ".json");
    if (!DirExists(rp.parent_path().string())) {
        LOG_ERROR("RoleConfigManager: roles dir not found: {}", rp.parent_path().string());
        return false;
    }

    // Read as raw string and parse as ordered_json to preserve key order
    auto content = ReadFile(rp.string());
    if (!content) {
        LOG_ERROR("RoleConfigManager: cannot open {} for save", rp.string());
        return false;
    }

    nlohmann::ordered_json rj;
    try {
        rj = nlohmann::ordered_json::parse(*content);
    } catch (const std::exception& e) {
        LOG_ERROR("RoleConfigManager: failed to parse {}: {}", rp.string(), e.what());
        return false;
    }

    bool changed = false;

    if (rj["llm"]["protocal"] != provider) {
        rj["llm"]["protocal"] = provider;
        changed = true;
    }
    if (rj["llm"]["model"] != model) {
        rj["llm"]["model"] = model;
        changed = true;
    }

    if (changed) {
        WriteOrderedJson(rp.string(), rj, 2);
    }
    return changed;
}

void RoleConfigManager::HotSwitch(const std::string& role_id,
                                  const std::string& provider,
                                  const std::string& model) {
    auto sessions = AgentSessionManager::GetInstance().GetSessionsByRole(role_id);
    for (auto* session : sessions) {
        if (session) {
            session->ApplyProviderOverride(provider, model);
        }
    }
    LOG_DEBUG("RoleConfigManager: hot-switched {} sessions for role {}", sessions.size(), role_id);
}

bool RoleConfigManager::SaveTtsVoice(const std::string& role_id,
                                     const std::string& voice,
                                     const std::string& backend) {
    auto rp = ProsophorConfig::BaseDir() / "roles" / (role_id + ".json");
    if (!DirExists(rp.parent_path().string())) {
        LOG_ERROR("RoleConfigManager: roles dir not found: {}", rp.parent_path().string());
        return false;
    }

    auto content = ReadFile(rp.string());
    if (!content) {
        LOG_ERROR("RoleConfigManager: cannot open {} for save", rp.string());
        return false;
    }

    nlohmann::ordered_json rj;
    try {
        rj = nlohmann::ordered_json::parse(*content);
    } catch (const std::exception& e) {
        LOG_ERROR("RoleConfigManager: failed to parse {}: {}", rp.string(), e.what());
        return false;
    }

    bool changed = false;

    if (!rj.contains("tts") || !rj["tts"].is_object()) {
        rj["tts"] = nlohmann::ordered_json::object();
        changed = true;
    }

    if (rj["tts"].value("voice", "") != voice) {
        rj["tts"]["voice"] = voice;
        changed = true;
    }
    if (rj["tts"].value("backend", "") != backend) {
        rj["tts"]["backend"] = backend;
        changed = true;
    }

    if (changed) {
        WriteOrderedJson(rp.string(), rj, 2);
    }
    return changed;
}

void RoleConfigManager::HotSwitchTtsVoice(const std::string& role_id,
                                          const std::string& voice,
                                          const std::string& backend) {
    auto& mgr = AgentSessionManager::GetInstance();
    auto sessions = mgr.GetSessionsByRole(role_id);
    for (auto* session : sessions) {
        if (!session) continue;
        auto* role = session->GetRole();
        if (!role) continue;
        if (!voice.empty()) role->tts_voice = voice;
        if (!backend.empty()) role->tts_backend = backend;
    }
    LOG_DEBUG("RoleConfigManager: hot-switched TTS for {} sessions for role {}",
              sessions.size(), role_id);
}

} // namespace prosophor
