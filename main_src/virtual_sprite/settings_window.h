// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <memory>
#include <vector>

namespace prosophor { class VoiceEngine; }

namespace prosophor {

/// SettingsWindow: modal settings dialog with 6 tabs (General, Roles,
/// Providers, Security, TTS, Local Models).
///
/// Extracted from ChatWindow for reusability. Owns its own edit state;
/// reads from / writes to ProsophorConfig. Requires only an optional
/// VoiceEngine pointer for the TTS "Test" button.
class SettingsWindow {
public:
    SettingsWindow();
    ~SettingsWindow();

    void Render();
    bool IsOpen() const { return open_; }
    void Open() { open_ = true; }

private:
    void InitState();
    void SaveSettings();

    void RenderGeneralTab();
    void RenderRolesTab();
    void RenderProvidersTab();
    void RenderSecurityTab();
    void RenderTtsTab();
    void RenderLocalModelsTab();

    struct SettingsState;
    std::unique_ptr<SettingsState> s_;

    bool open_ = false;

    // Local model edit state (shared with StartLLM button)
    std::string edit_lm_model_path_;
    int edit_lm_port_ = 8080;
    bool edit_lm_auto_start_ = false;
    int edit_lm_start_timeout_ = 150000;
};

}  // namespace prosophor
