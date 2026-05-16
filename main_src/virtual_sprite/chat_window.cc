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
        media_engine::PushStyleColor(media_engine::Color::Slot::Text, media_engine::Colors::Gray20);
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
}

// ── Tray window (右下角图标) ─────────────────────────────────

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
