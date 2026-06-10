// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>

namespace prosophor {

/// Persists role LLM config (provider + model) to ~/.prosophor/roles/
/// and hot-switches running sessions to use the new settings immediately.
class RoleConfigManager {
public:
    /// Update provider/model in the role's JSON file on disk.
    /// Returns true if the file was actually changed.
    static bool SaveModel(const std::string& role_id,
                          const std::string& provider,
                          const std::string& model);

    /// Hot-switch all running sessions for the given role to the new provider/model.
    static void HotSwitch(const std::string& role_id,
                          const std::string& provider,
                          const std::string& model);

    /// Update TTS voice/backend in the role's JSON file on disk.
    static bool SaveTtsVoice(const std::string& role_id,
                             const std::string& voice,
                             const std::string& backend);

    /// Hot-switch all running sessions for the given role to the new TTS voice.
    static void HotSwitchTtsVoice(const std::string& role_id,
                                  const std::string& voice,
                                  const std::string& backend);

    /// Save an arbitrary string field (e.g. "description", "personality_prompt") to the role JSON.
    /// Returns true if the file was actually changed.
    static bool SaveField(const std::string& role_id,
                          const std::string& field_path,
                          const std::string& value);

    /// Save a boolean field (e.g. "llm.auto_confirm_tools").
    static bool SaveFieldBool(const std::string& role_id,
                              const std::string& field_path,
                              bool value);

    /// Reload the role JSON from disk and update all running sessions for this role.
    static void HotReload(const std::string& role_id);
};

} // namespace prosophor
