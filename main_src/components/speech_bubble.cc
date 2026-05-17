// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "components/speech_bubble.h"

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
        bubble_width = std::max(min_width_, static_cast<float>(win_w) * 0.50f);
        bubble_body_h = std::max(min_body_height_, static_cast<float>(win_h) * 0.50f);
    }

    float bubble_total_h = bubble_body_h + (maximized ? 0.0f : tail_height_);
    float bx = maximized ? 0.0f : static_cast<float>(win_w) - bubble_width - 6.0f;
    float by = 0.0f;
    if (!maximized && bx < 4) bx = 4;

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
    media_engine::SetImGuiNextWindowPos(bx, by);
    media_engine::SetImGuiNextWindowSize(bubble_width, bubble_total_h);
    media_engine::SetImGuiNextWindowBgAlpha(0.0f);

    bool bubble_open = true;
    media_engine::ImGuiBegin("speech_bubble", &bubble_open,
        media_engine::ImGuiWindowFlags_NoDecoration |
        media_engine::ImGuiWindowFlags_NoMove |
        media_engine::ImGuiWindowFlags_NoSavedSettings |
        media_engine::ImGuiWindowFlags_NoScrollWithMouse);

    DrawBubbleBody(bubble_width, bubble_body_h);
    if (!maximized) { DrawTail(bubble_width, bubble_body_h); }

    DrawTitleBar(bx, by, bubble_width);

    // ── Position children in pixel coordinates directly ──
    float content_w = bubble_width - padding_ * 2.0f;
    float content_h = bubble_body_h - padding_ * 2.0f;

    float chat_h = content_h - title_height_ - input_height_;

    chat_panel_->SetPixelRect(padding_, padding_ + title_height_ + 4.0f,
                              content_w, chat_h);
    input_panel_->SetPixelRect(padding_, bubble_body_h - padding_ - input_height_,
                               content_w, input_height_);

    // Render InputPanel child (DrawPanel uses ImGui-window-relative coords)
    input_panel_->Render(ctx);

    // Render ChatPanel messages via ScrollWindow (needs viewport-absolute coords)
    float win_x, win_y;
    media_engine::ImGuiGetWindowPos(&win_x, &win_y);
    chat_panel_->RenderContentInRect(
        win_x + chat_panel_->GetX(), win_y + chat_panel_->GetY(),
        chat_panel_->GetWidth(), chat_panel_->GetHeight(),
        snapshot_);

    DrawResizeHandle(bubble_width, bubble_body_h);

    if (!bubble_open) { visible_ = false; }
    media_engine::ImGuiEnd();
}

// ── 气泡主体 ──
void SpeechBubble::DrawBubbleBody(float bubble_width, float bubble_body_h) {
    media_engine::DrawPanel(0, 0, bubble_width, bubble_body_h,
                             bubble_radius_, bg_color_, border_color_, 1.0f);
}

// ── 三角尾巴 ──
void SpeechBubble::DrawTail(float bubble_width, float bubble_body_h) {
    float tail_left = bubble_width - 40.0f;
    float tail_top = bubble_body_h - 8.0f;
    media_engine::DrawFilledTriangle(
        tail_left - 12, tail_top, tail_left + 12, tail_top, tail_left, tail_top + tail_height_,
        bg_color_);
    media_engine::DrawTriangleOutline(
        tail_left - 12, tail_top, tail_left + 12, tail_top, tail_left, tail_top + tail_height_,
        border_color_, 1.0f);
}

// ── 标题栏：标题 + 最大化 + 关闭 ──
void SpeechBubble::DrawTitleBar(float bx, float by, float bubble_width) {
    constexpr float title_top_y = 0.0f;

    float maximize_btn_x = bubble_width - padding_ - btn_size_ * 2.0f - 6.0f;
    if (media_engine::IconButtonRender("maximize", maximized_ ? "-" : "+",
                                        maximize_btn_x, title_top_y, btn_size_, button_color_, media_engine::Colors::WhiteTranslucent)) {
        if (maximized_) { custom_w_ = 0.0f; custom_h_ = 0.0f; }
        maximized_ = !maximized_;
        if (on_window_resize_) { on_window_resize_(maximized_); }
    }
    float close_btn_x = bubble_width - padding_ - btn_size_ - 2.0f;
    if (media_engine::IconButtonRender("close", "x",
                                        close_btn_x, title_top_y, btn_size_, button_color_, media_engine::Colors::WhiteTranslucent)) {
        visible_ = false;
    }

    media_engine::ImGuiSetCursorScreenPos(bx + padding_, by + padding_ + 2);
    media_engine::ImGuiTextColored(title_text_color_, "Prosophor");
}

// ── 缩放手柄 ──
void SpeechBubble::DrawResizeHandle(float bubble_width, float bubble_body_h) {
    float handle_size = 16.0f;
    float handle_x = bubble_width - handle_size - 2.0f;
    float handle_y = bubble_body_h - handle_size - 2.0f;
    media_engine::ImGuiSetCursorPos(handle_x, handle_y);
    media_engine::ImGuiInvisibleButton("##resize", handle_size, handle_size);

    if (media_engine::IsItemHovered() || media_engine::IsItemActive()) {
        media_engine::SetMouseCursor(media_engine::ImGuiMouseCursor_ResizeNWSE);
    }

    if (media_engine::IsItemActive()) {
        auto delta = media_engine::GetMouseDragDelta(3.0f);
        if (delta.x != 0.0f || delta.y != 0.0f) {
            custom_w_ = std::max(min_width_ * 0.8f, bubble_width + delta.x);
            custom_h_ = std::max(min_body_height_ * 0.8f, bubble_body_h + delta.y);
            media_engine::ResetMouseDragDelta();
            maximized_ = false;
        }
    }

    media_engine::DrawResizeGrip(handle_x, handle_y, handle_size,
                                  media_engine::Colors::Gray55a,
                                  media_engine::Colors::Gray63a);
}

} // namespace prosophor
