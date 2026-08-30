// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/components/speech_bubble.h"

namespace prosophor {

SpeechBubble::SpeechBubble()
    : Widget(0, 0, 100, 100) {
    // bg_color_ kept at White (member initializer) for bubble body rendering;
    // Widget::bg_color_ (base class, GrayDarkest) is never used since we override Render().
    visible_ = false;  // hidden until double-click toggles

    input_panel_ = std::make_unique<media_engine::InputPanel>(0, 0, 0, 0);
    chat_panel_ = std::make_unique<ChatPanel>(0, 0, 0, 0);

    AddChild(input_panel_.get());
    AddChild(chat_panel_.get());

    input_panel_->SetSendButtonColor(media_engine::Colors::Orange);
    input_panel_->SetOnSubmit([this](const std::string& msg) {
        if (on_submit_) { on_submit_(msg); }
    });
    chat_panel_->SetRoleFilter("assistant");
    chat_panel_->SetHideRoleLabels(true);
    chat_panel_->SetUserBgColor(media_engine::Colors::Transparent);
    chat_panel_->SetAssistantBgColor(media_engine::Colors::Transparent);
}

SpeechBubble::~SpeechBubble() = default;

void SpeechBubble::SetVisible(bool visible) {
    media_engine::Widget::SetVisible(visible);
    if (visible) input_panel_->SetText("");
}

void SpeechBubble::Toggle() {
    bool new_visible = !IsVisible();
    media_engine::Widget::SetVisible(new_visible);
    if (new_visible) input_panel_->SetText("");
}

bool SpeechBubble::HitTest(int x, int y) const {
    if (!visible_) return false;
    return x >= rect_x_ && x <= rect_x_ + width_ &&
           y >= rect_y_ && y <= rect_y_ + height_;
}

bool SpeechBubble::HitTestInteractive(int x, int y) const {
    if (!visible_) return false;

    // Convert to bubble-local coords
    float lx = static_cast<float>(x - rect_x_);
    float ly = static_cast<float>(y - rect_y_);
    if (lx < 0 || ly < 0) return false;

    float body_h = height_ - (maximized_ ? 0.0f : tail_height_);

    // 1. Title bar buttons (maximize + close) at top-right
    float max_btn_x = width_ - padding_ - btn_size_ * 2.0f - 6.0f;
    if (ly <= title_height_ && lx >= max_btn_x) return true;

    // 2. Input panel area at the bottom of the body
    float input_top = body_h - input_height_;
    if (ly >= input_top && ly <= body_h) return true;

    return false;
}

void SpeechBubble::Render(const media_engine::RenderContext& ctx) {
    if (!visible_) return;

    int win_w = override_w_;
    int win_h = override_h_;

    // Compute dimensions: compact vs maximized
    float bubble_width;
    float bubble_body_h;
    bool maximized = maximized_;
    if (maximized) {
        bubble_width = static_cast<float>(win_w);
        bubble_body_h = static_cast<float>(win_h);
    } else if (custom_w_ > 0 && custom_h_ > 0) {
        bubble_width = custom_w_;
        bubble_body_h = custom_h_;
    } else {
        bubble_width = std::max(min_width_, static_cast<float>(win_w));
        bubble_body_h = std::max(min_body_height_, static_cast<float>(win_h) * 0.50f);
    }

    float bubble_total_h = bubble_body_h + (maximized ? 0.0f : tail_height_);
    float bx = maximized ? 0.0f : static_cast<float>(win_w) - bubble_width;
    float by = 0.0f;

    // Store Widget pixel rect for hit-testing (in sprite window coordinates)
    width_ = bubble_width;
    height_ = bubble_total_h;
    // x_/y_ are widget-tree resolved values; for hit-test we want sprite-window coords
    // so temporarily store bx/by as the effective origin (hitttest uses x_/y_/width_/height_)
    // but x_/y_ are managed by Widget's cascading system — don't overwrite.
    // Instead, HitTest uses stored rect_ which we compute here:
    rect_x_ = static_cast<int>(bx);
    rect_y_ = static_cast<int>(by);

    // ── ImGui overlay window ──
    media_engine::ImGuiWindow::SetNextPos(bx, by);
    media_engine::ImGuiWindow::SetNextSize(bubble_width, bubble_total_h);
    media_engine::ImGuiWindow::SetNextBgAlpha(0.0f);

    auto _border = media_engine::ScopedStyleVar::WindowBorderSize(0.0f);

    bool bubble_open = true;
    auto _bubble = media_engine::ScopedGuard(
        media_engine::ImGuiWindow::Begin("speech_bubble", &bubble_open,
            media_engine::ImGuiWindowFlags_NoDecoration |
            media_engine::ImGuiWindowFlags_NoMove |
            media_engine::ImGuiWindowFlags_NoSavedSettings),
        []{ media_engine::ImGuiWindow::End(); },
        true);  // Begin: always call End

    DrawBubbleBody(bubble_width, bubble_body_h);
    if (!maximized) { DrawTail(bubble_width, bubble_body_h); }

    DrawTitleBar(bx, by, bubble_width);

    // ── Position children in pixel coordinates directly ──
    float content_w = bubble_width - padding_ * 2.0f;
    float content_h = bubble_body_h - padding_ * 2.0f;

    float chat_h = content_h - title_height_ - input_height_;

    chat_panel_->SetPixelRect(padding_, padding_ + title_height_ + 4.0f,
                              content_w, chat_h);
    input_panel_->SetPixelRect(0, bubble_body_h - input_height_,
                               bubble_width, input_height_);

    // Render InputPanel child (DrawPanel uses ImGui-window-relative coords)
    input_panel_->Render(ctx);

    // Render ChatPanel messages via ScrollWindow (needs viewport-absolute coords)
    // Bubble mode: only show the last assistant's text-only reply (no thinking)
    RenderSnapshot bubble_snapshot = snapshot_;
    bubble_snapshot.messages.clear();
    for (auto it = snapshot_.messages.rbegin(); it != snapshot_.messages.rend(); ++it) {
        if (it->role == "assistant") {
            bubble_snapshot.messages.push_back(FilterAssistantText(*it));
            break;
        }
    }
    float win_x, win_y;
    media_engine::ImGuiWindow::GetPos(&win_x, &win_y);
    chat_panel_->RenderContentInRect(
        win_x + chat_panel_->GetX(), win_y + chat_panel_->GetY(),
        chat_panel_->GetWidth(), chat_panel_->GetHeight(),
        bubble_snapshot);

    if (!bubble_open) { visible_ = false; }
}

// ── 气泡主体 ──
void SpeechBubble::SetInputCornerRadius(float r) {
    if (input_panel_) input_panel_->SetCornerRadius(r);
}

void SpeechBubble::DrawBubbleBody(float bubble_width, float bubble_body_h) {
    float wx, wy;
    media_engine::ImGuiWindow::GetPos(&wx, &wy);
    media_engine::DrawList::Panel(wx, wy, bubble_width, bubble_body_h,
                                   bubble_radius_, bg_color_, border_color_, 1.0f);
}

// ── 三角尾巴 ──
void SpeechBubble::DrawTail(float bubble_width, float bubble_body_h) {
    float wx, wy;
    media_engine::ImGuiWindow::GetPos(&wx, &wy);
    float tail_left = wx + bubble_width - 40.0f;
    float tail_top = wy + bubble_body_h - 8.0f;
    media_engine::DrawList::FilledTriangle(tail_left - 12, tail_top, tail_left + 12, tail_top,
                      tail_left, tail_top + tail_height_, bg_color_);
    media_engine::DrawList::TriangleOutline(tail_left - 12, tail_top, tail_left + 12, tail_top,
                       tail_left, tail_top + tail_height_, border_color_, 1.0f);
}

// ── 标题栏：标题 + 最大化 + 关闭 ──
void SpeechBubble::DrawTitleBar(float bx, float by, float bubble_width) {
    constexpr float title_top_y = 0.0f;

    float maximize_btn_x = bubble_width - padding_ - btn_size_ * 2.0f - 6.0f;
    if (media_engine::ImGuiWidget::IconButton("maximize", maximized_ ? "-" : "+",
                                        maximize_btn_x, title_top_y, btn_size_, button_color_, media_engine::Colors::WhiteTranslucent)) {
        if (maximized_) { custom_w_ = 0.0f; custom_h_ = 0.0f; }
        maximized_ = !maximized_;
        if (on_window_resize_) { on_window_resize_(maximized_); }
    }
    float close_btn_x = bubble_width - padding_ - btn_size_ - 2.0f;
    if (media_engine::ImGuiWidget::IconButton("close", "x",
                                        close_btn_x, title_top_y, btn_size_, button_color_, media_engine::Colors::WhiteTranslucent)) {
        visible_ = false;
    }

    media_engine::Layout::SetCursorScreenPos(bx + padding_, by + padding_ + 2);
    media_engine::Text::Colored(title_text_color_, title_text_.c_str());
}

MessageSchema SpeechBubble::FilterAssistantText(const MessageSchema& msg) {
    MessageSchema result;
    result.role = msg.role;
    for (const auto& b : msg.content) {
        if (b.type != "thinking") {
            result.content.push_back(b);
        }
    }
    return result;
}

} // namespace prosophor
