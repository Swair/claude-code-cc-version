// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "media_engine/media_engine.h"
#include "ui_component/input_panel.h"
#include "virtual_sprite/components/ui_types.h"
#include "virtual_sprite/components/chat_panel.h"
#include <memory>
#include <string>
#include <functional>

namespace prosophor {

/// Cloud-like speech bubble overlay for compact desktop-pet mode.
/// Inherits Widget for coordinate cascade integration with the Widget render tree.
class SpeechBubble : public media_engine::Widget {
public:
    SpeechBubble();
    ~SpeechBubble();

    void SetVisible(bool visible);
    bool IsVisible() const { return media_engine::Widget::IsVisible(); }
    void Toggle();

    using MessageSubmitCallback = std::function<void(const std::string&)>;
    void SetOnSubmit(MessageSubmitCallback cb) { on_submit_ = std::move(cb); }

    /// Mic toggle passthrough to InputPanel
    void SetOnMicToggle(media_engine::InputPanel::MicToggleCallback cb) {
        if (input_panel_) input_panel_->SetOnMicToggle(std::move(cb));
    }

    /// Set input text (e.g. from ASR result)
    void SetInputText(const std::string& text) { if (input_panel_) input_panel_->SetText(text); }

    /// Callback fired when maximize/restore is toggled — lets host resize the window
    using WindowResizeCallback = std::function<void(bool maximized)>;
    void SetOnWindowResize(WindowResizeCallback cb) { on_window_resize_ = std::move(cb); }

    /// Widget render override — creates ImGui overlay window, draws bubble + children
    void Render(const media_engine::RenderContext& ctx) override;

    /// Pre-set snapshot data before the render tree cascade
    void SetSnapshot(const RenderSnapshot& snap) { snapshot_ = snap; }
    /// Pre-set the sprite window dimensions before rendering
    void SetOverrideSize(int w, int h) { override_w_ = w; override_h_ = h; }

    /// Hit test: is the given screen point inside the bubble?
    bool HitTest(int x, int y) const;

    /// Hit test: is the given screen point on an interactive element
    /// (title buttons, input panel, resize handle)?
    bool HitTestInteractive(int x, int y) const;

    /// Toggle between compact and maximized size
    void ToggleMaximize() { maximized_ = !maximized_; }
    bool IsMaximized() const { return maximized_; }

private:
    bool maximized_ = false;
    float custom_w_ = 0.0f;
    float custom_h_ = 0.0f;
    std::unique_ptr<media_engine::InputPanel> input_panel_;
    std::unique_ptr<ChatPanel> chat_panel_;
    MessageSubmitCallback on_submit_;
    WindowResizeCallback on_window_resize_;
    RenderSnapshot snapshot_;
    int override_w_ = 0;
    int override_h_ = 0;

    // -- 命中检测（缓存 sprite-window 坐标，每帧 Render 时更新）--
    int rect_x_ = 0, rect_y_ = 0;
    void DrawBubbleBody(float bubble_width, float bubble_body_h);
    void DrawTail(float bubble_width, float bubble_body_h);
    void DrawTitleBar(float bx, float by, float bubble_width);
    /// Filter an assistant message to only text content (exclude thinking blocks)
    static MessageSchema FilterAssistantText(const MessageSchema& msg);
    // -- 外观 --
    media_engine::Color bg_color_{media_engine::Colors::Cream70};
    media_engine::Color border_color_{media_engine::Colors::Transparent};
    media_engine::Color title_text_color_{media_engine::Colors::Black};
    media_engine::Color button_color_{media_engine::Colors::CreamDark};
    std::string title_text_{"Prosophor"};

    // -- 布局（值由 Sprite::Create() 从 LayoutConfig 注入） --
    float bubble_radius_ = 0;
    float padding_ = 0;
    float title_height_ = 0;
    float input_height_ = 0;
    float btn_size_ = 0;
    float min_width_ = 0;
    float min_body_height_ = 0;
    float tail_height_ = 0;

public:
    // -- 外观 setter --
    void SetBubbleBackgroundColor(const media_engine::Color& c) { bg_color_ = c; }
    void SetBubbleBorderColor(const media_engine::Color& c)     { border_color_ = c; }
    void SetTitleTextColor(const media_engine::Color& c)        { title_text_color_ = c; }
    void SetTitle(const std::string& title)                     { title_text_ = title; }
    void SetAssistantRoleName(const std::string& name)       { if (chat_panel_) chat_panel_->SetAssistantRoleName(name); }
    void SetButtonColor(const media_engine::Color& c)           { button_color_ = c; }
    void SetSendButtonColor(const media_engine::Color& c)       { if (input_panel_) input_panel_->SetSendButtonColor(c); }

    // -- 布局 setter --
    void SetBubbleRadius(float r)          { bubble_radius_ = r; }
    void SetPadding(float p)               { padding_ = p; }
    void SetTitleHeight(float h)           { title_height_ = h; }
    void SetInputHeight(float h)           { input_height_ = h; }
    void SetTailHeight(float h)            { tail_height_ = h; }
    void SetButtonSize(float s)            { btn_size_ = s; }
    void SetMinBubbleSize(float w, float h) { min_width_ = w; min_body_height_ = h; }
    void SetInputCornerRadius(float r);
};

} // namespace prosophor
