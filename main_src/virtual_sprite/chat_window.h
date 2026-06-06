// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>

#include "virtual_sprite/settings_window.h"

namespace media_engine { class Window; class InputPanel; class Texture; }
namespace prosophor { class ChatPanel; }

namespace prosophor {

/// ChatWindow: separate SDL window for full chat UI (message history + input).
/// Uses ChatPanel + InputPanel + Window internally.
class ChatWindow {
public:
    ChatWindow();
    ~ChatWindow();

    /// Create SDL window + ImGui context + UI components.
    bool Create(int width, int height);

    /// Get the underlying Window (for registering render handlers with MediaCore).
    media_engine::Window* GetWindow() const;

    /// Render ImGui content (MainRun handles BeginFrame/EndFrame).
    void Render();

    void SetVisible(bool visible);
    bool IsVisible() const { return visible_; }

    using SubmitCallback = std::function<void(const std::string&)>;
    void SetOnSubmit(SubmitCallback cb);


    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

    void OpenSettings() { settings_.Open(); }

private:
    void RenderChatUI();
    void RenderMenuBar(float win_w_f);
    void UpdateLayout(int win_w, int win_h);
    void RenderChatContent();
    void RenderRightPanel(int win_w, int win_h);
    void RenderAboutWindow();
    void RenderTokenSpeed(int win_w, int win_h);
    void CreateTrayWindow();
    void ShowTray(bool show);
    void RenderTray();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    media_engine::Window* tray_window_ = nullptr;
    std::unique_ptr<media_engine::Texture> tray_texture_;

    SettingsWindow settings_;

    bool          visible_    = true;
    bool          tray_showing_ = false;
    bool          about_open_ = false;
    int           width_      = 800;
    int           height_     = 600;
    int           prev_layout_w_ = 0;
    int           prev_layout_h_ = 0;

    SubmitCallback submit_cb_;
};

}  // namespace prosophor
