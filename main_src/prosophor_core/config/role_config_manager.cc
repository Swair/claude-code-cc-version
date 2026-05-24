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

    auto rj_opt = ReadJson(rp.string());
    if (!rj_opt) {
        LOG_ERROR("RoleConfigManager: cannot open {} for save", rp.string());
        return false;
    }

    auto rj = *rj_opt;
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
        WriteJson(rp.string(), rj, 2);
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

} // namespace prosophor
