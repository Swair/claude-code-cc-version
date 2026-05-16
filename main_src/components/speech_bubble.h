// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "media_engine/media_engine.h"
#include "ui_component/input_panel.h"
#include "ui_types.h"
#include "chat_panel.h"
#include <memory>
#include <string>
#include <functional>

namespace prosophor {

/// Cloud-like speech bubble overlay for compact desktop-pet mode.
/// Appears on sprite click, shows messages + input, looks like a manga speech bubble.
class SpeechBubble {
public:
    SpeechBubble();
    ~SpeechBubble();

    void SetVisible(bool visible);
    bool IsVisible() const { return visible_; }
    void Toggle();

    using MessageSubmitCallback = std::function<void(const std::string&)>;
    void SetOnSubmit(MessageSubmitCallback cb) { on_submit_ = std::move(cb); }

    /// Callback fired when maximize/restore is toggled — lets host resize the window
    using WindowResizeCallback = std::function<void(bool maximized)>;
    void SetOnWindowResize(WindowResizeCallback cb) { on_window_resize_ = std::move(cb); }

    /// Render the bubble (ImGui overlay). Call during RenderImGui phase.
    /// @param snapshot  session state to render
    /// @param override_w  override display width (0 = use MediaCore window width)
    /// @param override_h  override display height (0 = use MediaCore window height)
    void Render(const RenderSnapshot& snapshot, int override_w, int override_h);

    /// Hit test: is the given screen point inside the bubble?
    bool HitTest(int x, int y) const;

    /// Toggle between compact and maximized size
    void ToggleMaximize() { maximized_ = !maximized_; }
    bool IsMaximized() const { return maximized_; }

private:
    bool visible_ = false;
    bool maximized_ = false;
    bool resizing_ = false;
    float custom_w_ = 0.0f;
    float custom_h_ = 0.0f;
    std::unique_ptr<media_engine::InputPanel> input_panel_;
    std::unique_ptr<ChatPanel> chat_panel_;
    MessageSubmitCallback on_submit_;
    WindowResizeCallback on_window_resize_;

    // -- 渲染分段 --
    void DrawBubbleBody(float bubble_width, float bubble_body_h);
    void DrawTail(float bubble_width, float bubble_body_h);
    void DrawTitleBar(float bx, float by, float bubble_width);
    void DrawInputArea(float cx, float cw, float bubble_width, float bubble_body_h);
    void DrawResizeHandle(float bubble_width, float bubble_body_h);

    // -- 外观 --
    media_engine::Color bg_color_{media_engine::Colors::White};
    media_engine::Color border_color_{media_engine::Colors::CreamBorder};
    media_engine::Color title_text_color_{media_engine::Colors::Gray55};
    media_engine::Color button_color_{media_engine::Colors::CreamDark};

    // -- 布局 --
    float bubble_radius_ = 14.0f;
    float padding_ = 12.0f;
    float title_height_ = 22.0f;
    float input_height_ = 34.0f;
    float btn_size_ = 22.0f;
    float min_width_ = 260.0f;
    float min_body_height_ = 200.0f;
    float tail_height_ = 20.0f;

public:
    // -- 外观 setter --
    void SetBubbleBackgroundColor(const media_engine::Color& c) { bg_color_ = c; }
    void SetBubbleBorderColor(const media_engine::Color& c)     { border_color_ = c; }
    void SetTitleTextColor(const media_engine::Color& c)        { title_text_color_ = c; }
    void SetButtonColor(const media_engine::Color& c)           { button_color_ = c; }

    // -- 布局 setter --
    void SetBubbleRadius(float r)          { bubble_radius_ = r; }
    void SetPadding(float p)               { padding_ = p; }
    void SetTitleHeight(float h)           { title_height_ = h; }
    void SetInputHeight(float h)           { input_height_ = h; }
    void SetButtonSize(float s)            { btn_size_ = s; }
    void SetMinBubbleSize(float w, float h) { min_width_ = w; min_body_height_ = h; }

    // Last rendered bubble rect in screen coordinates (updated each Render call)
    struct { int x = 0, y = 0, w = 0, h = 0; } rect_;
};

} // namespace prosophor
