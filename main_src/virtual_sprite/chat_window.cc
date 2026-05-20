// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/sprite.h"
#include "virtual_sprite/sprite_manager.h"
#include "virtual_sprite/layout_config.h"
#include "virtual_sprite/ui_renderer.h"
#include "agent_engine.h"
#include "components/chat_panel.h"
#include "media_engine/ui_component/input_panel.h"
#include "media_engine/media_engine.h"
#include "common/log_wrapper.h"
#include "common/i18n.h"
#include "config/config.h"

#include <filesystem>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <unordered_map>

namespace prosophor {

struct ChatWindow::Impl {
    media_engine::Window* window = nullptr;
    std::unique_ptr<ChatPanel> chat_panel;
    std::unique_ptr<media_engine::InputPanel> input_panel;
    std::unordered_map<std::string, std::unique_ptr<media_engine::Texture>> thumbnails;

};

ChatWindow::ChatWindow() = default;

ChatWindow::~ChatWindow() = default;

bool ChatWindow::Create(int width, int height) {
    width_ = width;
    height_ = height;

    media_engine::WindowConfig cfg;
    cfg.resizable = true;
    auto* win = media_engine::MediaCore::Instance().CreateMediaWindow(
        I18n::Instance().Get("window_title").c_str(), width_, height_, cfg);

    // Minimum size to keep layout usable
    win->SetMinSize(800, 500);

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
    media_engine::MediaCore::Instance().RegRenderHandler(win, [win, this]() {
        // Apple-style: white background, subtle borders, orange accent
        auto _app = media_engine::ScopedColors(media_engine::Color::Slot::WindowBg, media_engine::Colors::MilkyWhite)
                    .Then(media_engine::Color::Slot::Text, media_engine::Colors::Gray40)
                    .Then(media_engine::Color::Slot::FrameBg, media_engine::Colors::White)
                    .Then(media_engine::Color::Slot::PopupBg, media_engine::Colors::White)
                    .Then(media_engine::Color::Slot::Border, media_engine::Colors::CreamBorder);
        Render();

        // Right-click context menu (shared singleton)
        UIRenderer::Instance().RenderContextMenu(win);
    });

    // Right-click on chat window → context menu
    media_engine::MediaCore::Instance().RegMouseHandler(win,
        [win](const media_engine::MouseEvent& me) {
            if (me.type == media_engine::MouseEventType::DOWN &&
                me.button == media_engine::MouseButton::RIGHT) {
                UIRenderer::Instance().RequestContextMenu(me.window ? me.window : win);
            }
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
    media_engine::ImGuiWindow::SetNextPos(0, 0);
    media_engine::ImGuiWindow::SetNextSize(static_cast<float>(win_w), static_cast<float>(win_h));
    media_engine::ImGuiWindow::SetNextBgAlpha(0.0f);
    bool chat_root_open = true;
    auto _root = media_engine::ScopedGuard(
        media_engine::ImGuiWindow::Begin("chat_root", &chat_root_open,
            media_engine::ImGuiWindowFlags_MenuBar |
            media_engine::ImGuiWindowFlags_NoDecoration |
            media_engine::ImGuiWindowFlags_NoMove |
            media_engine::ImGuiWindowFlags_NoSavedSettings |
            media_engine::ImGuiWindowFlags_NoScrollWithMouse),
        []{ media_engine::ImGuiWindow::End(); });

    // 乳白色背景
    media_engine::DrawList::RoundRect(0, 0, static_cast<float>(win_w),
        static_cast<float>(win_h), 0, media_engine::Colors::MilkyWhite);

    RenderMenuBar(static_cast<float>(win_w));
    UpdateLayout(win_w, win_h);
    RenderChatContent();
    RenderRightPanel(win_w, win_h);

    RenderSettingsWindow();
    RenderAboutWindow();
}

void ChatWindow::RenderMenuBar(float win_w_f) {
    auto& L = I18n::Instance();
    auto _bar = media_engine::ScopedMenuBar();
    if (!_bar) return;

    if (auto _file = media_engine::ScopedMenu(L.Get("menu_file").c_str())) {
        if (media_engine::Popup::MenuItem(L.Get("ctx_settings").c_str())) {
            settings_open_ = true;
        }
        media_engine::ImGuiWidget::Separator();
        if (media_engine::Popup::MenuItem(L.Get("ctx_quit").c_str())) {
            media_engine::MediaCore::Instance().Quit();
        }
    }
    if (auto _help = media_engine::ScopedMenu(L.Get("menu_help").c_str())) {
        if (media_engine::Popup::MenuItem(L.Get("menu_about").c_str())) {
            about_open_ = true;
        }
    }

    // Right-side quick buttons
    float btn_sz = LayoutConfig{}.close_btn_size;
    float gear_x = win_w_f - btn_sz * 2 - 8.0f;
    float close_x = win_w_f - btn_sz - 4.0f;

    if (media_engine::ImGuiWidget::IconButton("settings", "S",
                                        gear_x, 0.0f, btn_sz,
                                        media_engine::Colors::CreamDark,
                                        media_engine::Colors::WhiteTranslucent)) {
        settings_open_ = true;
    }

    if (media_engine::ImGuiWidget::IconButton("close_chat", "x",
                                        close_x, 0.0f, btn_sz,
                                        media_engine::Colors::CreamDark,
                                        media_engine::Colors::WhiteTranslucent)) {
        SetVisible(false);
    }
}

void ChatWindow::UpdateLayout(int win_w, int win_h) {
    if (win_w == prev_layout_w_ && win_h == prev_layout_h_) return;
    prev_layout_w_ = win_w;
    prev_layout_h_ = win_h;

    constexpr float kLeftRatio = 0.75f;
    LayoutConfig cfg;
    float menu_pct = 22.0f / win_h * 100.0f;
    float input_pct = cfg.input_area_height / win_h * 100.0f;
    float content_h_pct = 100.0f - menu_pct - input_pct;
    impl_->chat_panel->SetRoot(win_w, win_h);
    impl_->chat_panel->SetPosition(0, menu_pct, kLeftRatio * 100, content_h_pct);
    impl_->input_panel->SetRoot(win_w, win_h);
    impl_->input_panel->SetPosition(0, 100 - input_pct, kLeftRatio * 100, input_pct);
}

void ChatWindow::RenderChatContent() {
    // Show focused sprite's session (set by clicking a sprite)
    auto& engine = AgentEngine::GetInstance();
    std::string sid = SpriteManager::GetInstance().GetFocusedSession();
    auto snap = sid.empty() ? engine.GetFocusedSessionSnapshot()
                            : engine.GetSessionSnapshot(sid);
    impl_->chat_panel->SetSnapshot(snap.value_or(RenderSnapshot{}));

    std::string sprite_name = SpriteManager::GetInstance().GetFocusedSpriteName();
    if (!sprite_name.empty()) {
        impl_->chat_panel->SetAssistantDisplayName(sprite_name);
    }

    impl_->chat_panel->Render(media_engine::RenderContext{});
    impl_->input_panel->Render(media_engine::RenderContext{});
}

void ChatWindow::RenderRightPanel(int win_w, int win_h) {
    auto& L = I18n::Instance();
    constexpr float kLeftRatio = 0.75f;
    float panel_left = static_cast<float>(win_w) * kLeftRatio;
    float right_w = static_cast<float>(win_w) - panel_left;

    // Decorative divider (warm two-tone gradient look)
    media_engine::DrawList::RoundRect(panel_left, 3.0f, 2.0f,
                 static_cast<float>(win_h) - 3.0f, 0,
                 media_engine::Colors::OrangeWarm);
    media_engine::DrawList::RoundRect(panel_left + 2, 3.0f, 1.0f,
                 static_cast<float>(win_h) - 3.0f, 0,
                 media_engine::Colors::CreamBorder);

    // Right panel background
    media_engine::DrawList::RoundRect(panel_left + 3, 3.0f,
                 right_w - 3, static_cast<float>(win_h) - 6.0f,
                 0, media_engine::Colors::CreamLight);

    // Decorative corner ornament (top-right of right panel)
    media_engine::DrawList::RoundRect(panel_left + right_w - 18.0f, 6.0f,
                 14.0f, 14.0f, 7.0f,
                 media_engine::Colors::OrangeLight);
    media_engine::DrawList::RoundRect(panel_left + right_w - 14.0f, 10.0f,
                 6.0f, 6.0f, 3.0f,
                 media_engine::Colors::OrangeWarm);

    // Scrolling area for cards (thin scrollbar)
    auto _scroll = media_engine::ScopedStyleVar::ScrollbarSize(3.0f);
    media_engine::Layout::SetCursorScreenPos(panel_left + 4, 44.0f);
    auto _panel = media_engine::ScopedChild(
        "role_panel", right_w - 7, static_cast<float>(win_h) - 52.0f,
        0, media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);

    // Header with decorative accent and language switch
    {
        std::string hdr = L.Get("role_panel_header");
        media_engine::DrawList::RoundRect(panel_left + 4.0f, 48.0f, 3.0f, 12.0f, 1.5f,
                     media_engine::Colors::Orange);
        media_engine::Layout::Dummy(8, 0);
        media_engine::Layout::SameLine();
        media_engine::Text::Colored(media_engine::Colors::OrangeWarm, hdr.c_str());
        media_engine::Layout::SameLine();
        bool is_zh = L.GetLanguage() == "zh-CN";
        if (media_engine::ImGuiWidget::Button(is_zh ? "EN" : "中", 36.0f, 20.0f)) {
            L.SetLanguage(is_zh ? "en" : "zh-CN");
            if (impl_ && impl_->window) {
                impl_->window->SetTitle(L.Get("window_title").c_str());
            }
        }
    }
    // Decorative thin separator
    media_engine::DrawList::RoundRect(panel_left + 8.0f, 66.0f, right_w - 16.0f, 1.0f, 0,
                 media_engine::Colors::CreamBorder);
    media_engine::Layout::Dummy(0, 4);

    auto& sprites = SpriteManager::GetInstance().GetAll();
    std::string focused_sid = SpriteManager::GetInstance().GetFocusedSession();
    float card_w = right_w - 10 - 6;
    constexpr float kCardH = 64.0f;

    for (auto& s : sprites) {
        bool focused = (s->GetSessionId() == focused_sid);

        float cx, cy;
        media_engine::Layout::GetCursorScreenPos(&cx, &cy);

        media_engine::DrawList::RoundRect(cx, cy, card_w, kCardH, 6.0f,
            focused ? media_engine::Colors::OrangeLightest
                    : media_engine::Colors::White);

        if (focused) {
            media_engine::DrawList::RoundRectOutline(cx, cy, card_w, kCardH, 6.0f,
                media_engine::Colors::OrangeWarm, 1.5f);
        }

        // Thumbnail: first spritesheet frame
        constexpr float kThumbW = 48.0f;
        constexpr float kThumbH = 48.0f;
        float thumb_x = cx + 8.0f;
        float thumb_y = cy + (kCardH - kThumbH) / 2;
        std::string tex_path = s->GetSpritesheetPath();
        if (!tex_path.empty()) {
            auto it = impl_->thumbnails.find(tex_path);
            if (it == impl_->thumbnails.end()) {
                auto tex = std::make_unique<media_engine::Texture>(
                    *impl_->window, tex_path);
                if (tex->GetOriginWidth() > 0 && tex->GetOriginHeight() > 0) {
                    it = impl_->thumbnails.emplace(tex_path, std::move(tex)).first;
                }
            }
            if (it != impl_->thumbnails.end() && it->second) {
                it->second->DrawImGui(thumb_x, thumb_y, kThumbW, kThumbH,
                                      0.0f, 0.0f, 1.0f / 8.0f, 1.0f / 9.0f);
            }
        }

        float text_x = cx + 12.0f + kThumbW + 6.0f;
        float text_y = cy + (kCardH - 16.0f) / 2;
        media_engine::DrawList::Text(text_x, text_y,
            focused ? media_engine::Colors::OrangeDeep
                    : media_engine::Colors::Black,
            s->GetName().c_str());

        if (media_engine::ImGuiWidget::InvisibleButton(
                ("card_" + s->GetSessionId()).c_str(), card_w, kCardH)) {
            SpriteManager::GetInstance().SetFocusedSession(s->GetSessionId());
        }

        media_engine::Layout::Dummy(0, 4);
    }

}

void ChatWindow::RenderAboutWindow() {
    if (!about_open_) return;

    auto& L = I18n::Instance();
    media_engine::Popup::Open("About");
    media_engine::ImGuiWindow::SetNextSize(400.0f, 300.0f, media_engine::ImGuiCond_Appearing);
    auto _about = media_engine::ScopedModal(
        L.Get("about_title").c_str(), &about_open_,
        media_engine::ImGuiWindowFlags_NoSavedSettings);
    if (!_about) return;

    media_engine::Text::Colored(media_engine::Colors::OrangeDeep, "Prosophor v" PROSOPHOR_VERSION);
    media_engine::Layout::Dummy(0, 8);
    media_engine::Text::Wrapped("AI Desktop Companion \xe2\x80\x94 Desktop Pet + LLM Chat",
                                media_engine::ImGuiWindow::GetWidth() - 30.0f,
                                media_engine::Colors::Gray40);
    media_engine::Layout::Dummy(0, 12);
    media_engine::Text::Colored(media_engine::Colors::Gray47, L.Get("about_contact").c_str());
    media_engine::Layout::Dummy(0, 4);
    media_engine::Text::Fmt("Email: %s", "swair@outlook.com");
    media_engine::Layout::Dummy(0, 4);
    media_engine::Text::Fmt("GitHub: %s", "https://github.com/swair");
    media_engine::Layout::Dummy(0, 16);

    float btn_w = 80.0f;
    media_engine::Layout::SetCursorPosX(media_engine::ImGuiWindow::GetWidth() - btn_w - 10.0f);
    if (media_engine::ImGuiWidget::Button(L.Get("btn_close").c_str(), btn_w, 0)) {
        about_open_ = false;
    }
}

void ChatWindow::RenderSettingsWindow() {
    if (!settings_open_) return;

    auto& L = I18n::Instance();
    media_engine::Popup::Open("Settings");
    media_engine::ImGuiWindow::SetNextSize(520.0f, 380.0f, media_engine::ImGuiCond_Appearing);
    auto _set = media_engine::ScopedModal(
        L.Get("settings_title").c_str(), &settings_open_,
        media_engine::ImGuiWindowFlags_NoSavedSettings);
    if (!_set) return;

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

    if (auto _bar = media_engine::ScopedTabBar("SettingsTabs")) {
        // ── General tab ──
        if (auto _tab = media_engine::ScopedTabItem(L.Get("tab_general").c_str())) {
            media_engine::Text::Raw(L.Get("general_log_level").c_str());
            media_engine::Layout::SameLine();
            const char* levels[] = {"trace", "debug", "info", "warn", "error"};
            int ll_idx = 0;
            for (int i = 0; i < 5; ++i) {
                if (edit_log_level == levels[i]) { ll_idx = i; break; }
            }
            media_engine::ImGuiWidget::Combo("##log_level", &ll_idx, levels, 5);
            edit_log_level = levels[ll_idx];

            media_engine::Layout::Dummy(0, 8);
            media_engine::Text::Raw(L.Get("general_enable_summary").c_str());
            media_engine::Layout::SameLine();
            media_engine::ImGuiWidget::Checkbox("##enable_summary", &edit_enable_summary);

            media_engine::Layout::Dummy(0, 8);
            media_engine::Text::Raw(L.Get("general_sprite_assets_dir").c_str());
            char dir_buf[512];
            std::strncpy(dir_buf, edit_sprite_dir.c_str(), sizeof(dir_buf) - 1);
            dir_buf[sizeof(dir_buf) - 1] = '\0';
            if (media_engine::ImGuiWidget::InputText("##sprite_dir", dir_buf, sizeof(dir_buf))) {
                edit_sprite_dir = dir_buf;
            }

        }

        // ── Roles tab ──
        if (auto _tab = media_engine::ScopedTabItem(L.Get("tab_roles").c_str())) {
            media_engine::Text::Raw(L.Get("roles_select_hint").c_str());
            media_engine::ImGuiWidget::Separator();
            for (size_t i = 0; i < all_role_ids.size(); ++i) {
                bool checked = (role_checked[i] != 0);
                media_engine::ImGuiWidget::Checkbox(all_role_ids[i].c_str(), &checked);
                role_checked[i] = checked ? 1 : 0;
            }
        }

        // ── Providers tab ──
        if (auto _tab = media_engine::ScopedTabItem(L.Get("tab_providers").c_str())) {
            media_engine::Text::Raw(L.Get("providers_readonly").c_str());
            media_engine::ImGuiWidget::Separator();
            for (const auto& [name, prov] : config.providers) {
                if (media_engine::ImGuiWidget::TreeNode(name.c_str())) {
                    media_engine::Text::Fmt("  API Key: %s...", prov.api_key.empty() ? "" : prov.api_key.substr(0, 12).c_str());
                    media_engine::Text::Fmt("  Base URL: %s", prov.base_url.c_str());
                    media_engine::Text::Fmt("  Timeout: %ds", prov.timeout);
                    media_engine::Text::Raw("  Agents:");
                    for (const auto& [agent_name, agent] : prov.agents) {
                        media_engine::ImGuiWidget::BulletText("%s (%s, T=%.1f, max=%d)",
                                          agent_name.c_str(), agent.model.c_str(),
                                          agent.temperature, agent.max_tokens);
                    }
                    media_engine::ImGuiWidget::TreePop();
                }
            }
        }

    }

    // ── Save / Cancel ──
    media_engine::ImGuiWidget::Separator();
    media_engine::Layout::Dummy(0, 4);
    float btn_w = 100.0f;
    media_engine::Layout::SetCursorPosX(media_engine::ImGuiWindow::GetWidth() - btn_w * 2.0f - 12.0f);
    if (media_engine::ImGuiWidget::Button(L.Get("btn_cancel").c_str(), btn_w, 0)) {
        settings_open_ = false;
        reset_on_close();
    }
    media_engine::Layout::SameLine();
    if (media_engine::ImGuiWidget::Button(L.Get("btn_save").c_str(), btn_w, 0)) {
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
}

void ChatWindow::CreateTrayWindow() {
    media_engine::WindowConfig tcfg;
    tcfg.borderless = true;
    tcfg.transparent_window = true;
    tcfg.transparent_bg = true;
    tcfg.skip_taskbar = true;
    tcfg.always_on_top = true;
    tcfg.resizable = false;
    tcfg.use_shared_font = false;

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
    media_engine::DrawList::Panel(pad, pad, s - pad * 2.0f, s - pad * 2.0f, 20.0f,
                                   media_engine::Colors::Orange, media_engine::Colors::CreamBorder, 1.5f);
    int tx, ty;
    tray_window_->GetPosition(&tx, &ty);
    media_engine::Layout::SetCursorScreenPos(
        static_cast<float>(tx) + 16.0f, static_cast<float>(ty) + 10.0f);
    media_engine::Text::Colored(media_engine::Colors::White, "P");
}

}  // namespace prosophor
