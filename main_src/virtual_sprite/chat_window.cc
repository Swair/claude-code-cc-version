// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/sprite_manager.h"
#include "scene/layout_config.h"
#include "agent_engine.h"
#include "components/chat_panel.h"
#include "media_engine/ui_component/input_panel.h"
#include "media_engine/media_engine.h"
#include "common/log_wrapper.h"
#include "config/config.h"

#include <filesystem>
#include <fstream>
#include <cstring>
#include <algorithm>

namespace prosophor {

struct ChatWindow::Impl {
    media_engine::Window* window = nullptr;
    std::unique_ptr<ChatPanel> chat_panel;
    std::unique_ptr<media_engine::InputPanel> input_panel;
};

ChatWindow::ChatWindow() = default;

ChatWindow::~ChatWindow() = default;

bool ChatWindow::Create(int width, int height) {
    width_ = width;
    height_ = height;

    auto* win = media_engine::MediaCore::Instance().CreateMediaWindow(
        "Prosophor Chat", width_, height_);

    auto chat_panel = std::make_unique<ChatPanel>(0, 0, 100, 100);
    auto input_panel = std::make_unique<media_engine::InputPanel>(0, 0, 100, 100);
    input_panel->SetSendButtonColor(media_engine::Colors::Orange);
    if (submit_cb_) {
        input_panel->SetOnSubmit(submit_cb_);
    }

    impl_ = std::make_unique<Impl>();
    impl_->window = win;
    impl_->chat_panel = std::move(chat_panel);
    impl_->input_panel = std::move(input_panel);

    // Register render handler internally
    media_engine::MediaCore::Instance().RegRenderHandler(win, [this]() {
        // Apple-style: white background, subtle borders, orange accent
        constexpr int kColorCount = 5;
        media_engine::PushStyleColor(media_engine::Color::Slot::WindowBg, media_engine::Colors::White);
        media_engine::PushStyleColor(media_engine::Color::Slot::Text, media_engine::Colors::Gray40);
        media_engine::PushStyleColor(media_engine::Color::Slot::FrameBg, media_engine::Colors::White);
        media_engine::PushStyleColor(media_engine::Color::Slot::PopupBg, media_engine::Colors::White);
        media_engine::PushStyleColor(media_engine::Color::Slot::Border, media_engine::Colors::CreamBorder);
        Render();
        media_engine::PopStyleColor(kColorCount);
    });

    CreateTrayWindow();

    LOG_INFO("[ChatWindow] Created {}x{} window", width_, height_);
    return true;
}

void ChatWindow::SetOnSubmit(SubmitCallback cb) {
    submit_cb_ = cb;
    if (impl_ && impl_->input_panel) {
        impl_->input_panel->SetOnSubmit(cb);
    }
}

void ChatWindow::SetVisible(bool visible) {
    visible_ = visible;
    if (impl_ && impl_->window) {
        if (visible) {
            impl_->window->Show();
            ShowTray(false);
        } else {
            impl_->window->Hide();
            ShowTray(true);
        }
    }
}

media_engine::Window* ChatWindow::GetWindow() const {
    return impl_ ? impl_->window : nullptr;
}

void ChatWindow::Render() {
    if (!visible_ || !impl_ || !impl_->window) {
        return;
    }
    RenderChatUI();
}

void ChatWindow::RenderChatUI() {
    int win_w = impl_->window->GetWidth();
    int win_h = impl_->window->GetHeight();

    // Full-screen ImGui window to provide context for children's ImGui draw calls
    media_engine::SetImGuiNextWindowPos(0, 0);
    media_engine::SetImGuiNextWindowSize(static_cast<float>(win_w), static_cast<float>(win_h));
    media_engine::SetImGuiNextWindowBgAlpha(0.0f);
    bool chat_root_open = true;
    media_engine::ImGuiBegin("chat_root", &chat_root_open,
        media_engine::ImGuiWindowFlags_NoDecoration |
        media_engine::ImGuiWindowFlags_NoMove |
        media_engine::ImGuiWindowFlags_NoSavedSettings |
        media_engine::ImGuiWindowFlags_NoScrollWithMouse);

    // Skip layout recalculation if window hasn't been resized
    if (win_w != prev_layout_w_ || win_h != prev_layout_h_) {
        prev_layout_w_ = win_w;
        prev_layout_h_ = win_h;

        LayoutConfig cfg;
        float input_pct = cfg.input_area_height / win_h * 100.0f;
        float close_x = static_cast<float>(win_w) - cfg.close_btn_size - 4.0f;

        impl_->chat_panel->SetRoot(win_w, win_h);
        impl_->chat_panel->SetPosition(0, 0, 100, 100 - input_pct);
        impl_->input_panel->SetRoot(win_w, win_h);
        impl_->input_panel->SetPosition(0, 100 - input_pct, 100, input_pct);

        close_btn_x_ = close_x;
    }

    float gear_x = close_btn_x_ - LayoutConfig{}.close_btn_size - 4.0f;
    if (media_engine::IconButtonRender("settings", "⚙",
                                        gear_x, 0.0f, LayoutConfig{}.close_btn_size,
                                        media_engine::Colors::CreamDark,
                                        media_engine::Colors::WhiteTranslucent)) {
        settings_open_ = true;
    }

    if (media_engine::IconButtonRender("close_chat", "x",
                                        close_btn_x_, 0.0f, LayoutConfig{}.close_btn_size,
                                        media_engine::Colors::CreamDark,
                                        media_engine::Colors::WhiteTranslucent)) {
        SetVisible(false);
    }

    // Show focused sprite's session (set by clicking a sprite)
    auto& engine = AgentEngine::GetInstance();
    std::string sid = SpriteManager::GetInstance().GetFocusedSession();
    auto snap = sid.empty() ? engine.GetFocusedSessionSnapshot()
                            : engine.GetSessionSnapshot(sid);
    impl_->chat_panel->SetSnapshot(snap.value_or(RenderSnapshot{}));
    impl_->chat_panel->Render(media_engine::RenderContext{});

    impl_->input_panel->Render(media_engine::RenderContext{});

    RenderSettingsWindow();

    media_engine::ImGuiEnd();
}

void ChatWindow::RenderSettingsWindow() {
    if (!settings_open_) return;

    media_engine::ImGuiOpenPopup("Settings");
    media_engine::SetImGuiNextWindowSize(520.0f, 380.0f, media_engine::ImGuiCond_Appearing);
    if (!media_engine::ImGuiBeginPopupModal("Settings", &settings_open_,
                                            media_engine::ImGuiWindowFlags_NoSavedSettings)) {
        return;
    }

    auto& config = ProsophorConfig::GetInstance();

    // Editable copies (re-init each open via static flag)
    static bool first_open = true;
    static std::string edit_log_level;
    static bool edit_enable_summary;
    static std::string edit_sprite_dir;
    static std::vector<std::string> edit_roles;
    static std::vector<int> role_checked;
    static std::vector<std::string> all_role_ids;

    if (first_open) {
        edit_log_level = config.log_level;
        edit_enable_summary = config.enable_summary;
        edit_sprite_dir = config.sprite_assets_dir;
        edit_roles = config.default_role;
        first_open = false;

        // Scan available roles
        all_role_ids.clear();
        std::string roles_dir = std::string(PROSOPHOR_SOURCE_DIR) + "/config/.prosophor/roles";
        if (std::filesystem::exists(roles_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(roles_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".json") {
                    all_role_ids.push_back(entry.path().stem().string());
                }
            }
        }
        role_checked.assign(all_role_ids.size(), 0);
        for (size_t i = 0; i < all_role_ids.size(); ++i) {
            if (std::find(edit_roles.begin(), edit_roles.end(), all_role_ids[i]) != edit_roles.end()) {
                role_checked[i] = 1;
            }
        }
    }
    auto reset_on_close = [&] { first_open = true; };

    if (media_engine::ImGuiBeginTabBar("SettingsTabs")) {
        // ── General tab ──
        if (media_engine::ImGuiBeginTabItem("General")) {
            media_engine::ImGuiTextUnformatted("Log Level");
            media_engine::ImGuiSameLine();
            const char* levels[] = {"trace", "debug", "info", "warn", "error"};
            int ll_idx = 0;
            for (int i = 0; i < 5; ++i) {
                if (edit_log_level == levels[i]) { ll_idx = i; break; }
            }
            media_engine::ImGuiCombo("##log_level", &ll_idx, levels, 5);
            edit_log_level = levels[ll_idx];

            media_engine::Dummy(0, 8);
            media_engine::ImGuiTextUnformatted("Enable Summary");
            media_engine::ImGuiSameLine();
            media_engine::ImGuiCheckbox("##enable_summary", &edit_enable_summary);

            media_engine::Dummy(0, 8);
            media_engine::ImGuiTextUnformatted("Sprite Assets Dir");
            char dir_buf[512];
            std::strncpy(dir_buf, edit_sprite_dir.c_str(), sizeof(dir_buf) - 1);
            dir_buf[sizeof(dir_buf) - 1] = '\0';
            if (media_engine::ImGuiInputText("##sprite_dir", dir_buf, sizeof(dir_buf))) {
                edit_sprite_dir = dir_buf;
            }

            media_engine::ImGuiEndTabItem();
        }

        // ── Roles tab ──
        if (media_engine::ImGuiBeginTabItem("Roles")) {
            media_engine::ImGuiTextUnformatted("Select default roles (one sprite per role):");
            media_engine::ImGuiSeparator();
            for (size_t i = 0; i < all_role_ids.size(); ++i) {
                bool checked = (role_checked[i] != 0);
                media_engine::ImGuiCheckbox(all_role_ids[i].c_str(), &checked);
                role_checked[i] = checked ? 1 : 0;
            }
            media_engine::ImGuiEndTabItem();
        }

        // ── Providers tab ──
        if (media_engine::ImGuiBeginTabItem("Providers")) {
            media_engine::ImGuiTextUnformatted("Configured providers (read-only):");
            media_engine::ImGuiSeparator();
            for (const auto& [name, prov] : config.providers) {
                if (media_engine::ImGuiTreeNode(name.c_str())) {
                    media_engine::ImGuiText("  API Key: %s...", prov.api_key.empty() ? "" : prov.api_key.substr(0, 12).c_str());
                    media_engine::ImGuiText("  Base URL: %s", prov.base_url.c_str());
                    media_engine::ImGuiText("  Timeout: %ds", prov.timeout);
                    media_engine::ImGuiTextUnformatted("  Agents:");
                    for (const auto& [agent_name, agent] : prov.agents) {
                        media_engine::ImGuiBulletText("%s (%s, T=%.1f, max=%d)",
                                          agent_name.c_str(), agent.model.c_str(),
                                          agent.temperature, agent.max_tokens);
                    }
                    media_engine::ImGuiTreePop();
                }
            }
            media_engine::ImGuiEndTabItem();
        }

        media_engine::ImGuiEndTabBar();
    }

    // ── Save / Cancel ──
    media_engine::ImGuiSeparator();
    media_engine::Dummy(0, 4);
    float btn_w = 100.0f;
    media_engine::ImGuiSetCursorPosX(media_engine::ImGuiGetWindowWidth() - btn_w * 2.0f - 12.0f);
    if (media_engine::ImGuiButton("Cancel", btn_w, 0)) {
        settings_open_ = false;
        reset_on_close();
    }
    media_engine::ImGuiSameLine();
    if (media_engine::ImGuiButton("Save", btn_w, 0)) {
        // Write back editable copies
        config.log_level = edit_log_level;
        config.enable_summary = edit_enable_summary;
        config.sprite_assets_dir = edit_sprite_dir;

        edit_roles.clear();
        for (size_t i = 0; i < all_role_ids.size(); ++i) {
            if (role_checked[i]) {
                edit_roles.push_back(all_role_ids[i]);
            }
        }
        config.default_role = edit_roles;

        config.SaveToFile();
        settings_open_ = false;
        reset_on_close();
    }

    media_engine::ImGuiEndPopup();
}

void ChatWindow::CreateTrayWindow() {
    media_engine::WindowConfig tcfg;
    tcfg.borderless = true;
    tcfg.transparent_window = true;
    tcfg.transparent_bg = true;
    tcfg.skip_taskbar = true;
    tcfg.always_on_top = true;
    tcfg.resizable = false;

    LayoutConfig tcfg_layout;
    tray_window_ = media_engine::MediaCore::Instance().CreateMediaWindow(
        "prosophor_tray", tcfg_layout.tray_icon_size, tcfg_layout.tray_icon_size, tcfg);
    if (!tray_window_) {
        LOG_WARN("[ChatWindow] Failed to create tray window");
        return;
    }

    int dw, dh;
    media_engine::MediaCore::GetPrimaryDisplaySize(&dw, &dh);
    tray_window_->SetPosition(dw - tcfg_layout.tray_margin, dh - tcfg_layout.tray_margin);
    tray_window_->Hide();

    media_engine::MediaCore::Instance().RegRenderHandler(tray_window_, [this]() {
        RenderTray();
    });

    media_engine::MediaCore::Instance().RegMouseHandler(tray_window_,
        [this](const media_engine::MouseEvent& me) {
            if (me.type == media_engine::MouseEventType::DOWN &&
                me.button == media_engine::MouseButton::RIGHT) {
                media_engine::MediaCore::Instance().Quit();
            } else if (me.type == media_engine::MouseEventType::DOWN &&
                       me.button == media_engine::MouseButton::LEFT) {
                ShowTray(false);
                SetVisible(true);
            }
        });
}

void ChatWindow::ShowTray(bool show) {
    tray_showing_ = show;
    if (tray_window_) {
        if (show) tray_window_->Show();
        else tray_window_->Hide();
    }
}

void ChatWindow::RenderTray() {
    float s = static_cast<float>(LayoutConfig{}.tray_icon_size), pad = 3.0f;
    media_engine::DrawPanel(pad, pad, s - pad * 2.0f, s - pad * 2.0f, 20.0f,
                             media_engine::Colors::Orange, media_engine::Colors::CreamBorder, 1.5f);
    int tx, ty;
    tray_window_->GetPosition(&tx, &ty);
    media_engine::ImGuiSetCursorScreenPos(
        static_cast<float>(tx) + 16.0f, static_cast<float>(ty) + 10.0f);
    media_engine::ImGuiTextColored(media_engine::Colors::White, "P");
}

}  // namespace prosophor
