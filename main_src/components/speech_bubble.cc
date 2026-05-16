// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "components/speech_bubble.h"

namespace prosophor {

SpeechBubble::SpeechBubble()
    : input_panel_(std::make_unique<media_engine::InputPanel>(0, 0, 0, 0))
    , chat_panel_(std::make_unique<ChatPanel>(0, 0, 0, 0)) {
    input_panel_->SetSendButtonColor(media_engine::Colors::Orange);
    input_panel_->SetOnSubmit([this](const std::string& msg) {
        if (on_submit_) { on_submit_(msg); }
    });
    chat_panel_->SetRoleFilter("assistant");
    chat_panel_->SetHideRoleLabels(true);
}

SpeechBubble::~SpeechBubble() = default;

void SpeechBubble::SetVisible(bool visible) {
    visible_ = visible;
    if (visible) input_panel_->SetText("");
}

void SpeechBubble::Toggle() {
    visible_ = !visible_;
    if (visible_) input_panel_->SetText("");
}

bool SpeechBubble::HitTest(int x, int y) const {
    if (!visible_) return false;
    return x >= rect_.x && x <= rect_.x + rect_.w &&
           y >= rect_.y && y <= rect_.y + rect_.h;
}

void SpeechBubble::Render(const RenderSnapshot& snapshot, int override_w, int override_h) {
    if (!visible_) return;

    int win_w = override_w;
    int win_h = override_h;

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

    // Store rect for hit-testing
    rect_.x = static_cast<int>(bx);
    rect_.y = static_cast<int>(by);
    rect_.w = static_cast<int>(bubble_width);
    rect_.h = static_cast<int>(bubble_total_h);

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

    float cx = padding_;
    float cw = bubble_width - padding_ * 2.0f;

    DrawTitleBar(bx, by, bubble_width);

    // Messages area (delegate to ChatPanel)
    float msgs_y = padding_ + title_height_ + 4.0f;
    float msgs_h = bubble_body_h - padding_ - title_height_ - input_height_ - 12.0f;
    chat_panel_->RenderContentInRect(bx + cx, by + msgs_y, cw, msgs_h, snapshot);

    DrawInputArea(cx, cw, bubble_width, bubble_body_h);
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

// ── 输入区：委托给 InputPanel ──
void SpeechBubble::DrawInputArea(float cx, float cw,
                                  float bubble_width, float bubble_body_h) {
    float input_y = bubble_body_h - padding_ - input_height_;
    media_engine::DrawPanel(cx, input_y, cw, input_height_, 8, bg_color_, border_color_, 1.0f);

    // Root = bubble dimensions → coordinates match speech_bubble ImGui window (relative coords)
    input_panel_->SetRoot(bubble_width, bubble_body_h);
    input_panel_->SetPosition(10.0f, 80.0f, 80.0f, 20.0f);
    input_panel_->Render(media_engine::RenderContext{});
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
