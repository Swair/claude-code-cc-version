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

bool RoleConfigManager::SaveField(const std::string& role_id,
                                  const std::string& field_path,
                                  const std::string& value) {
    auto rp = ProsophorConfig::BaseDir() / "roles" / (role_id + ".json");
    if (!DirExists(rp.parent_path().string())) return false;
    auto content = ReadFile(rp.string());
    if (!content) return false;
    nlohmann::ordered_json rj;
    try { rj = nlohmann::ordered_json::parse(*content); }
    catch (...) { return false; }

    // Navigate dot-separated path ("llm.protocal")
    auto* j = &rj;
    size_t dot = 0, start = 0;
    while ((dot = field_path.find('.', start)) != std::string::npos) {
        std::string key = field_path.substr(start, dot - start);
        if (!j->contains(key) || !(*j)[key].is_object()) (*j)[key] = nlohmann::ordered_json::object();
        j = &(*j)[key];
        start = dot + 1;
    }
    std::string leaf = field_path.substr(start);
    if ((*j)[leaf] != value) {
        (*j)[leaf] = value;
        WriteOrderedJson(rp.string(), rj, 2);
        return true;
    }
    return false;
}

bool RoleConfigManager::SaveFieldBool(const std::string& role_id,
                                      const std::string& field_path,
                                      bool value) {
    auto rp = ProsophorConfig::BaseDir() / "roles" / (role_id + ".json");
    if (!DirExists(rp.parent_path().string())) return false;
    auto content = ReadFile(rp.string());
    if (!content) return false;
    nlohmann::ordered_json rj;
    try { rj = nlohmann::ordered_json::parse(*content); }
    catch (...) { return false; }

    auto* j = &rj;
    size_t dot = 0, start = 0;
    while ((dot = field_path.find('.', start)) != std::string::npos) {
        std::string key = field_path.substr(start, dot - start);
        if (!j->contains(key) || !(*j)[key].is_object()) (*j)[key] = nlohmann::ordered_json::object();
        j = &(*j)[key];
        start = dot + 1;
    }
    std::string leaf = field_path.substr(start);
    bool cur = (*j).value(leaf, false);
    if (cur != value) {
        (*j)[leaf] = value;
        WriteOrderedJson(rp.string(), rj, 2);
        return true;
    }
    return false;
}

void RoleConfigManager::HotReload(const std::string& role_id) {
    auto& mgr = AgentSessionManager::GetInstance();
    auto sessions = mgr.GetSessionsByRole(role_id);
    for (auto* session : sessions) {
        if (!session) continue;
        auto* role = session->GetRole();
        if (!role) continue;
        // Reload role JSON from disk
        auto rp = ProsophorConfig::BaseDir() / "roles" / (role_id + ".json");
        auto content = ReadFile(rp.string());
        if (!content) continue;
        try {
            nlohmann::json rj = nlohmann::json::parse(*content);
            role->description = rj.value("description", role->description);
            role->personality_prompt = rj.value("personality_prompt", role->personality_prompt);
            role->name = rj.value("display_name", role->name);
            if (rj.contains("tts") && rj["tts"].is_object()) {
                role->tts_voice = rj["tts"].value("voice", role->tts_voice);
                role->tts_backend = rj["tts"].value("backend", role->tts_backend);
            }
            if (rj.contains("llm") && rj["llm"].is_object()) {
                role->auto_confirm_tools = rj["llm"].value("auto_confirm_tools", role->auto_confirm_tools);
            }
        } catch (...) {}
    }
    LOG_DEBUG("RoleConfigManager: hot-reloaded {} sessions for role {}", sessions.size(), role_id);
}

} // namespace prosophor
