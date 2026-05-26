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
#include "common/file_utils.h"
#include "config/config.h"
#include "config/role_config_manager.h"
#include "providers/tts/tts_speaker.h"

#include <filesystem>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <future>
#include <thread>
#include <nlohmann/json.hpp>

#include "platform/platform.h"

namespace prosophor {

struct ChatWindow::Impl {
    media_engine::Window* window = nullptr;
    std::unique_ptr<ChatPanel> chat_panel;
    std::unique_ptr<media_engine::InputPanel> input_panel;
    std::unordered_map<std::string, std::unique_ptr<media_engine::Texture>> thumbnails;
    std::atomic<bool> lm_starting{false};

};

// Settings edit state (shared between settings window and StartLLM button)
static std::string edit_lm_model_path;
static int edit_lm_port = 8080;
static bool edit_lm_auto_start;
static int edit_lm_start_timeout = 150000;

ChatWindow::ChatWindow() = default;

ChatWindow::~ChatWindow() = default;

bool ChatWindow::Create(int width, int height) {
    width_ = width;
    height_ = height;

    media_engine::WindowConfig cfg;
    cfg.resizable = true;
    auto* win = media_engine::MediaCore::Instance().CreateMediaWindow(
        (I18n::Instance().Get("window_title") + " v" PROSOPHOR_VERSION).c_str(), width_, height_, cfg);

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
        []{ media_engine::ImGuiWindow::End(); },
        true);  // Begin: always call End
    if (!_root) return;

    // 乳白色背景
    media_engine::DrawList::RoundRect(0, 0, static_cast<float>(win_w),
        static_cast<float>(win_h), 0, media_engine::Colors::MilkyWhite);

    RenderMenuBar(static_cast<float>(win_w));
    UpdateLayout(win_w, win_h);
    RenderChatContent();
    RenderRightPanel(win_w, win_h);
    RenderTokenSpeed(win_w, win_h);

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
    if (!_panel) return;

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
                impl_->window->SetTitle((L.Get("window_title") + " v" PROSOPHOR_VERSION).c_str());
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
    media_engine::Popup::Open("about_modal");
    media_engine::ImGuiWindow::SetNextSize(400.0f, 300.0f, media_engine::ImGuiCond_Appearing);
    const auto about_title = L.Get("about_title") + "###about_modal";
    auto _about = media_engine::ScopedModal(
        about_title.c_str(), &about_open_,
        media_engine::ImGuiWindowFlags_NoSavedSettings);
    if (!_about) return;

    std::string title = L.Get("app_name") + " v" PROSOPHOR_VERSION;
    media_engine::Text::Colored(media_engine::Colors::OrangeDeep, title.c_str());
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
    auto& config = ProsophorConfig::GetInstance();

    // Light settings panel
    auto _colors = media_engine::ScopedColors(media_engine::Color::Slot::TitleBg, media_engine::Colors::BluePale)
                   .Then(media_engine::Color::Slot::TitleBgActive, media_engine::Colors::BlueLightSoft)
                   .Then(media_engine::Color::Slot::WindowBg, media_engine::Colors::White)
                   .Then(media_engine::Color::Slot::ChildBg, media_engine::Colors::White)
                   .Then(media_engine::Color::Slot::Text, media_engine::Colors::Gray40)
                   .Then(media_engine::Color::Slot::FrameBg, media_engine::Colors::White)
                   .Then(media_engine::Color::Slot::PopupBg, media_engine::Colors::White)
                   .Then(media_engine::Color::Slot::Border, media_engine::Colors::CreamBorder);

    media_engine::Popup::Open("settings_modal");
    media_engine::ImGuiWindow::SetNextSize(560.0f, 460.0f, media_engine::ImGuiCond_Appearing);
    const auto settings_title = L.Get("settings_title") + "###settings_modal";
    auto _set = media_engine::ScopedModal(
        settings_title.c_str(), &settings_open_,
        media_engine::ImGuiWindowFlags_NoSavedSettings);
    if (!_set) return;

    // Editable copies (re-init each open via static flag)
    static bool first_open = true;
    static std::string edit_log_level;
    static bool edit_enable_summary;
    static std::string edit_sprite_dir;
    static std::vector<std::string> edit_roles;
    static std::vector<int> role_checked;
    static std::vector<std::string> all_role_ids;

    // Model selection per role
    static std::vector<std::string> available_models;
    static std::vector<int> role_model_idx;

    // Security
    static std::string edit_perm_level;
    static bool edit_allow_local_exec;

    // TTS
    static bool edit_tts_enabled;
    static std::string edit_tts_backend;
    static std::string edit_edge_voice;
    static bool edit_edge_auto_start;

    if (first_open) {
        edit_log_level = config.log_level;
        edit_enable_summary = config.enable_summary;
        edit_sprite_dir = config.sprite_assets_dir;
        edit_roles = config.default_role;

        edit_perm_level = config.security.permission_level;
        edit_allow_local_exec = config.security.allow_local_execute;

        edit_tts_enabled = config.tts.enabled;
        edit_tts_backend = config.tts.backend;
        edit_edge_voice = "zh-CN-XiaoxiaoNeural";
        edit_edge_auto_start = config.tts.gs_auto_start;

        if (!config.llamacpp_models.empty()) {
            edit_lm_port = config.llamacpp_models[0].port;
            edit_lm_auto_start = config.llamacpp_models[0].auto_start;
            edit_lm_start_timeout = config.llamacpp_models[0].start_timeout_ms;
            edit_lm_model_path = config.llamacpp_models[0].model_path;
        }

        // Scan available roles
        all_role_ids.clear();
        std::string roles_dir = (ProsophorConfig::BaseDir() / "roles").string();
        if (DirExists(roles_dir)) {
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

        // Collect available models from all providers (display: "[protocal] model (provider)")
        available_models.clear();
        for (const auto& [pname, prov] : config.providers) {
            for (const auto& [model_name, model_config] : prov.model_configs) {
                std::string display = "[" + pname + "] " + model_config.model;
                if (std::find(available_models.begin(), available_models.end(), display)
                    == available_models.end()) {
                    available_models.push_back(display);
                }
            }
        }
        // Read each role's current model
        role_model_idx.assign(all_role_ids.size(), 0);
        for (size_t i = 0; i < all_role_ids.size(); ++i) {
            std::string rp = (ProsophorConfig::BaseDir() / "roles" / (all_role_ids[i] + ".json")).string();
            std::ifstream f(rp);
            if (f.is_open()) {
                try {
                    auto rj = nlohmann::json::parse(f);
                    std::string rm = rj["llm"]["model"];
                    // Match against available_models[ai] = "[protocal] model"
                    std::string rp_proto = rj["llm"]["protocal"];
                    for (size_t ai = 0; ai < available_models.size(); ++ai) {
                        auto bracket_end = available_models[ai].find("] ");
                        if (bracket_end != std::string::npos && available_models[ai].substr(bracket_end + 2) == rm) {
                            role_model_idx[i] = static_cast<int>(ai);
                            break;
                        }
                    }
                    // Fallback: if model not found, pick first model under the same protocal
                    if (role_model_idx[i] == 0 && !available_models.empty()) {
                        for (size_t ai = 0; ai < available_models.size(); ++ai) {
                            auto bracket_end = available_models[ai].find("] ");
                            if (bracket_end != std::string::npos && available_models[ai].substr(1, bracket_end - 1) == rp_proto) {
                                role_model_idx[i] = static_cast<int>(ai);
                                break;
                            }
                        }
                    }
                } catch (...) {}
            }
        }
        first_open = false;
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
            media_engine::Layout::SameLine();
            if (media_engine::ImGuiWidget::Button("...", 30, 0)) {
                std::string sel = Platform::BrowseForDirectory();
                if (!sel.empty()) {
                    edit_sprite_dir = sel;
                }
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

                // Model selector combo
                media_engine::Layout::SameLine();
                std::string combo_id = "##model_" + all_role_ids[i];
                std::vector<const char*> cstrs;
                for (const auto& m : available_models) cstrs.push_back(m.c_str());
                if (media_engine::ImGuiWidget::Combo(
                        combo_id.c_str(), &role_model_idx[i], cstrs.data(),
                        static_cast<int>(cstrs.size()))) {
                    // selection changed
                }
            }
        }

        // ── Providers tab (editable) ──
        if (auto _tab = media_engine::ScopedTabItem(L.Get("tab_providers").c_str())) {
            for (auto& [pname, prov] : config.providers) {
                if (media_engine::ImGuiWidget::TreeNode(pname.c_str())) {
                    if (prov.entries.empty()) {
                        media_engine::Text::Raw("(legacy format, no editable entries)");
                    } else {
                        for (size_t ei = 0; ei < prov.entries.size(); ++ei) {
                            auto& entry = prov.entries[ei];
                            std::string label = "Entry " + std::to_string(ei + 1);
                            if (media_engine::ImGuiWidget::TreeNode(label.c_str())) {
                                // api_key
                                char buf[1024];
                                std::strncpy(buf, entry.api_key.c_str(), sizeof(buf) - 1);
                                buf[sizeof(buf) - 1] = '\0';
                                media_engine::Text::Raw("API Key");
                                std::string kid = "##api_" + pname + "_" + std::to_string(ei);
                                if (media_engine::ImGuiWidget::InputText(kid.c_str(), buf, sizeof(buf))) {
                                    entry.api_key = buf;
                                }

                                // base_url
                                std::strncpy(buf, entry.base_url.c_str(), sizeof(buf) - 1);
                                buf[sizeof(buf) - 1] = '\0';
                                media_engine::Text::Raw("Base URL");
                                std::string uid = "##url_" + pname + "_" + std::to_string(ei);
                                if (media_engine::ImGuiWidget::InputText(uid.c_str(), buf, sizeof(buf))) {
                                    entry.base_url = buf;
                                }

                                // timeout
                                media_engine::Text::Raw("Timeout (s)");
                                media_engine::Layout::SameLine();
                                std::string tid = "##to_" + pname + "_" + std::to_string(ei);
                                media_engine::ImGuiWidget::InputInt(tid.c_str(), &entry.timeout);

                                // agents
                                if (!entry.models.empty()) {
                                    media_engine::ImGuiWidget::Separator();
                                    media_engine::Text::Raw("Models");
                                    // Build a list from the map for indexed access
                                    std::vector<std::string> model_keys;
                                    for (auto& [mk, mv] : entry.models) model_keys.push_back(mk);
                                    for (size_t ai = 0; ai < model_keys.size(); ++ai) {
                                        auto& agent = entry.models[model_keys[ai]];
                                        std::string alabel = agent.model;
                                        if (media_engine::ImGuiWidget::TreeNode(alabel.c_str())) {
                                            std::string prefix = "##" + pname + "_" + std::to_string(ei) + "_" + std::to_string(ai);

                                            // model
                                            char mbuf[256];
                                            std::strncpy(mbuf, agent.model.c_str(), sizeof(mbuf) - 1);
                                            mbuf[sizeof(mbuf) - 1] = '\0';
                                            media_engine::Text::Raw("Model");
                                            std::string mid = prefix + "_model";
                                            if (media_engine::ImGuiWidget::InputText(mid.c_str(), mbuf, sizeof(mbuf))) {
                                                agent.model = mbuf;
                                            }

                                            // temperature
                                            media_engine::Text::Raw("Temperature");
                                            media_engine::Layout::SameLine();
                                            std::string tepid = prefix + "_temp";
                                            media_engine::ImGuiWidget::SliderFloat(tepid.c_str(), &agent.temperature, 0.0f, 2.0f, "%.1f");

                                            // max_tokens
                                            media_engine::Text::Raw("Max Tokens");
                                            media_engine::Layout::SameLine();
                                            std::string mtid = prefix + "_maxtok";
                                            media_engine::ImGuiWidget::InputInt(mtid.c_str(), &agent.max_tokens);

                                            // context_window
                                            media_engine::Text::Raw("Context Window");
                                            media_engine::Layout::SameLine();
                                            std::string cwid = prefix + "_ctx";
                                            media_engine::ImGuiWidget::InputInt(cwid.c_str(), &agent.context_window);

                                            // thinking
                                            media_engine::Text::Raw("Thinking");
                                            media_engine::Layout::SameLine();
                                            std::string thid = prefix + "_think";
                                            media_engine::ImGuiWidget::Checkbox(thid.c_str(), &agent.thinking);

                                            // enable_streaming
                                            media_engine::Text::Raw("Streaming");
                                            media_engine::Layout::SameLine();
                                            std::string stid = prefix + "_stream";
                                            media_engine::ImGuiWidget::Checkbox(stid.c_str(), &agent.enable_streaming);

                                            media_engine::ImGuiWidget::TreePop();
                                        }
                                    }
                                }
                                media_engine::ImGuiWidget::TreePop();
                            }
                        }

                        // Add Entry button
                        media_engine::Layout::Dummy(0, 4);
                        if (media_engine::ImGuiWidget::Button("+ Add Entry", 0, 0)) {
                            ProviderEntryConfig new_entry;
                            new_entry.timeout = 30;
                            prov.entries.push_back(std::move(new_entry));
                        }
                    }
                    media_engine::ImGuiWidget::TreePop();
                }
            }
        }

        // ── Security tab ──
        if (auto _tab = media_engine::ScopedTabItem(L.Get("tab_security").c_str())) {
            media_engine::Text::Raw(L.Get("security_permission_level").c_str());
            media_engine::Layout::SameLine();
            const char* perm_levels[] = {"auto", "allow", "deny"};
            int pl_idx = 0;
            for (int i = 0; i < 3; ++i) {
                if (edit_perm_level == perm_levels[i]) { pl_idx = i; break; }
            }
            media_engine::ImGuiWidget::Combo("##perm_level", &pl_idx, perm_levels, 3);
            edit_perm_level = perm_levels[pl_idx];

            media_engine::Layout::Dummy(0, 8);
            media_engine::Text::Raw(L.Get("security_allow_local_exec").c_str());
            media_engine::Layout::SameLine();
            media_engine::ImGuiWidget::Checkbox("##allow_local_exec", &edit_allow_local_exec);
        }

        // ── TTS tab ──
        if (auto _tab = media_engine::ScopedTabItem(L.Get("tab_tts").c_str())) {
            media_engine::Text::Raw(L.Get("tts_enabled").c_str());
            media_engine::Layout::SameLine();
            media_engine::ImGuiWidget::Checkbox("##tts_enabled", &edit_tts_enabled);

            media_engine::Layout::Dummy(0, 8);
            media_engine::Text::Raw(L.Get("tts_backend").c_str());
            media_engine::Layout::SameLine();
            const char* backends[] = {"edge-tts"};
            media_engine::ImGuiWidget::Combo("##tts_backend", nullptr, backends, 1);
            edit_tts_backend = "edge-tts";

            auto input_field = [&](const char* label, const char* id, std::string& var) {
                media_engine::Layout::Dummy(0, 4);
                media_engine::Text::Raw(label);
                char buf[512];
                std::strncpy(buf, var.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (media_engine::ImGuiWidget::InputText(id, buf, sizeof(buf))) {
                    var = buf;
                }
            };

            if (edit_tts_backend == "edge-tts") {
                media_engine::ImGuiWidget::Separator();
                media_engine::Text::Raw("edge-tts");
                input_field("Voice", "##edge_voice", edit_edge_voice);
                media_engine::Layout::Dummy(0, 8);
                media_engine::Text::Raw("Auto Start");
                media_engine::Layout::SameLine();
                media_engine::ImGuiWidget::Checkbox("##edge_auto_start", &edit_edge_auto_start);
            }

            media_engine::Layout::Dummy(0, 8);
            if (media_engine::ImGuiWidget::Button("Test", 52.0f, 20.0f)) {
                const bool zh = L.GetLanguage() == "zh-CN";
                auto& tts_config = ProsophorConfig::GetInstance().tts;
                tts_config.backend = edit_tts_backend;
                // voice saved via edit_edge_voice
                TtsSpeaker::GetInstance().ApplyVoiceProfile(tts_config.backend, edit_edge_voice);
                LOG_INFO("[ChatWindow] TTS test trigger enabled={} backend='{}' voice='{}'", tts_config.enabled,
                         tts_config.backend, edit_edge_voice);
                TtsSpeaker::GetInstance().SpeakStream(
                    zh ? "你好！我是Prosophor。" : "Hello! I am Prosophor.");
            }
        }

        // ── Local Models tab ──
        if (auto _tab = media_engine::ScopedTabItem(L.Get("tab_local_models").c_str())) {
            if (config.llamacpp_models.empty()) {
                media_engine::Text::Raw(L.Get("providers_readonly").c_str());
            } else {
                char model_buf[512];
                std::strncpy(model_buf, edit_lm_model_path.c_str(), sizeof(model_buf) - 1);
                model_buf[sizeof(model_buf) - 1] = '\0';
                media_engine::Text::Raw(L.Get("local_model_model_path").c_str());
                if (media_engine::ImGuiWidget::InputText("##lm_model_path", model_buf, sizeof(model_buf))) {
                    edit_lm_model_path = model_buf;
                }
                media_engine::Layout::SameLine();
                if (media_engine::ImGuiWidget::Button("...##lm_browse", 36, 0)) {
                    std::string path = Platform::BrowseForFile("GGUF Model (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0");
                    if (!path.empty()) {
                        edit_lm_model_path = path;
                    }
                }

                media_engine::Layout::Dummy(0, 8);
                media_engine::Text::Raw(L.Get("local_model_auto_start").c_str());
                media_engine::Layout::SameLine();
                media_engine::ImGuiWidget::Checkbox("##lm_auto_start", &edit_lm_auto_start);

                media_engine::Layout::Dummy(0, 8);
                media_engine::Text::Raw(L.Get("local_model_port").c_str());
                media_engine::Layout::SameLine();
                media_engine::ImGuiWidget::InputInt("##lm_port", &edit_lm_port);

                media_engine::Layout::Dummy(0, 8);
                media_engine::Text::Raw(L.Get("local_model_start_timeout").c_str());
                media_engine::Layout::SameLine();
                media_engine::ImGuiWidget::InputInt("##lm_start_timeout", &edit_lm_start_timeout);
            }
        }
    }

    // ── Save / Cancel ──
    media_engine::ImGuiWidget::Separator();
    media_engine::Layout::Dummy(0, 4);
    float btn_w = 100.0f;
    float spacing = 8.0f;
    media_engine::Layout::SetCursorPosX(media_engine::ImGuiWindow::GetWidth() - btn_w * 2.0f - spacing - 12.0f);
    if (media_engine::ImGuiWidget::Button(L.Get("btn_cancel").c_str(), btn_w, 0)) {
        // Reload config to discard any in-place edits (providers are edited directly)
        config = ProsophorConfig::LoadFromFile(ProsophorConfig::DefaultConfigPath());
        settings_open_ = false;
        reset_on_close();
    }
    media_engine::Layout::SameLine();
    if (media_engine::ImGuiWidget::Button(L.Get("btn_save").c_str(), btn_w, 0)) {
        // Write back General
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

        // Persist per-role model selection
        for (size_t i = 0; i < all_role_ids.size(); ++i) {
            if (role_checked[i]) {
                auto display = available_models[role_model_idx[i]];
                auto bracket_end = display.find("] ");
                if (bracket_end != std::string::npos) {
                    std::string provider = display.substr(1, bracket_end - 1);
                    std::string model = display.substr(bracket_end + 2);
                    RoleConfigManager::SaveModel(all_role_ids[i], provider, model);
                }
            }
        }

        // Write back Security
        config.security.permission_level = edit_perm_level;
        config.security.allow_local_execute = edit_allow_local_exec;

        // Write back TTS
        config.tts.enabled = edit_tts_enabled;
        config.tts.backend = edit_tts_backend;
        // voice/auto_start applied via hot-switch below

        // Hot-switch TTS backend
        TtsSpeaker::GetInstance().ApplyVoiceProfile(config.tts.backend, edit_edge_voice);
        LOG_INFO("[ChatWindow] TTS config saved: enabled={} backend='{}' voice='{}'", config.tts.enabled,
                 config.tts.backend, edit_edge_voice);

        // Write back Local Models
        if (!config.llamacpp_models.empty()) {
            config.llamacpp_models[0].port = edit_lm_port;
            config.llamacpp_models[0].auto_start = edit_lm_auto_start;
            config.llamacpp_models[0].start_timeout_ms = edit_lm_start_timeout;
            config.llamacpp_models[0].model_path = edit_lm_model_path;
        }

        config.SaveToFile();

        // Hot-switch provider/model for all checked roles
        for (size_t i = 0; i < all_role_ids.size(); ++i) {
            if (role_checked[i]) {
                auto display = available_models[role_model_idx[i]];
                auto bracket_end = display.find("] ");
                if (bracket_end != std::string::npos) {
                    std::string provider = display.substr(1, bracket_end - 1);
                    std::string model = display.substr(bracket_end + 2);
                    RoleConfigManager::HotSwitch(all_role_ids[i], provider, model);
                }
            }
        }

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

    // Preload tray texture
    std::string icon_path = std::string(PROSOPHOR_SOURCE_DIR) + "/main_src/resources/robot_icon.png";
    tray_texture_ = std::make_unique<media_engine::Texture>(*tray_window_, icon_path);

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
    float s = static_cast<float>(LayoutConfig{}.tray_icon_size);

    // Full-size transparent canvas
    media_engine::ImGuiWindow::SetNextPos(0, 0);
    media_engine::ImGuiWindow::SetNextSize(s, s);
    bool canvas_open = true;
    int canvas_flags = 0
        | media_engine::ImGuiWindowFlags_NoDecoration
        | media_engine::ImGuiWindowFlags_NoMove
        | media_engine::ImGuiWindowFlags_NoMouseInputs
        | media_engine::ImGuiWindowFlags_NoBackground
        | media_engine::ImGuiWindowFlags_NoSavedSettings;
    if (!media_engine::ImGuiWindow::Begin("tray", &canvas_open, canvas_flags)) {
        media_engine::ImGuiWindow::End();
        return;
    }

    // Draw the robot icon texture, scaled to fit with 1px padding
    float pad = 1.0f;
    if (tray_texture_) {
        tray_texture_->DrawImGui(pad, pad, s - pad * 2.0f, s - pad * 2.0f,
                                  0.0f, 0.0f, 1.0f, 1.0f);
    }

    media_engine::ImGuiWindow::End();
}

void ChatWindow::RenderTokenSpeed(int win_w, int win_h) {
    auto snap = SpriteManager::GetInstance().GetFocusedSession().empty()
        ? AgentEngine::GetInstance().GetFocusedSessionSnapshot()
        : AgentEngine::GetInstance().GetSessionSnapshot(
            SpriteManager::GetInstance().GetFocusedSession());
    if (!snap || snap->streaming_token_speed <= 0.0f) return;

    // Screen coordinates: ImGui window position + relative offset
    float wx, wy;
    media_engine::ImGuiWindow::GetPos(&wx, &wy);
    int speed_val = static_cast<int>(snap->streaming_token_speed + 0.5f);
    std::string text = std::to_string(speed_val) + " tok/s";

    float pad = 6.0f;
    float text_w = text.size() * 7.5f;
    float text_h = 16.0f;
    float badge_w = text_w + pad * 2;
    float badge_h = text_h + pad * 2;

    float left_area_w = win_w * 0.75f;
    // Screen-space bottom-right of left chat area
    float bx = wx + left_area_w - badge_w - 8.0f;
    float by = wy + static_cast<float>(win_h) - badge_h - 8.0f;

    media_engine::DrawList::RoundRect(bx, by, badge_w, badge_h, 4.0f,
        media_engine::Colors::WhiteTranslucent);
    media_engine::DrawList::Text(bx + pad, by + pad,
        media_engine::Colors::Gray55, text.c_str());
}

}  // namespace prosophor
