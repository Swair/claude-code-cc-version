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
};

} // namespace prosophor
