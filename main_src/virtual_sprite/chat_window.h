// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <functional>
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>

#include "virtual_sprite/settings_window.h"
#include "virtual_sprite/update_handler.h"
#include "components/sidebar.h"
#include "media_engine/media/colors.h"

namespace media_engine { class Window; class InputPanel; class Texture; }
namespace prosophor { class ChatPanel; }

namespace prosophor {

class ChatWindow {
public:
    ChatWindow();
    ~ChatWindow();

    bool Create(int width, int height);
    media_engine::Window* GetWindow() const;
    void Render();
    void SetVisible(bool visible);
    bool IsVisible() const { return visible_; }

    using SubmitCallback = std::function<void(const std::string&)>;
    void SetOnSubmit(SubmitCallback cb);

    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }
    void OpenSettings() { settings_.Open(); }

private:
    // Shell
    void RenderChatUI();

    // View switching
    void RenderCurrentView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderChatView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderStatusView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderSessionsView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderActiveTriggersView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderLogsView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderUsageView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderConfigView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderRolesView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderMemoryView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderProvidersView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderAboutView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderLocalModelsView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderTtsView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderSecurityView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderSkillsView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderKnowledgeView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderMcpView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderPetStoreView(int cont_x, int cont_y, int cont_w, int cont_h);
    void RenderComputerOrganizeView(int cont_x, int cont_y, int cont_w, int cont_h);

    // Sub-components
    void UpdateLayout(int main_w, int main_h);
    void RenderChatContent();
    void RenderRightPanel(int panel_x, int panel_w, int panel_y, int panel_h);
    void RenderTokenSpeed(int main_x, int main_y, int main_w, int main_h);
    void RenderAboutWindow();

    // Tray
    void CreateTrayWindow();
    void ShowTray(bool show);
    void RenderTray();

    // Panel data (exposed for panel/*.cc files)
    struct PanelData {
        media_engine::Window* window = nullptr;
        std::unique_ptr<ChatPanel> chat_panel;
        std::unique_ptr<media_engine::InputPanel> input_panel;
        std::unordered_map<std::string, std::unique_ptr<media_engine::Texture>> thumbnails;
        std::atomic<bool> lm_starting{false};
    };
    std::unique_ptr<PanelData> d_;

    media_engine::Window* tray_window_ = nullptr;
    std::unique_ptr<media_engine::Texture> tray_texture_;

    SettingsWindow settings_;
    UpdateHandler update_handler_;
    Sidebar sidebar_;

    bool visible_         = true;
    bool tray_showing_    = false;
    bool about_open_      = false;
    bool right_panel_open_= true;
    int  width_           = 800;
    int  height_          = 600;
    int  prev_layout_w_   = 0;
    int  prev_layout_h_   = 0;

    // Log filter state
    int  log_filter_      = 0; // 0=All, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR
    size_t log_clear_anchor_ = 0; // ring buffer entries to skip after Clear

    SubmitCallback submit_cb_;
};

}  // namespace prosophor
