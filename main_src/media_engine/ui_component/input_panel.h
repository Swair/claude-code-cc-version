// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <memory>
#include <string>

#include "ui_component/widget.h"

namespace media_engine {

class InputText;
class Button;

/// 输入面板 - 输入框 + 发送按钮 + 麦克风按钮的复合组件
///
/// 内部构成：背景(自绘) + InputText + Button(发送) + Button(麦克风)，
/// 所有渲染在 RenderContent 中完成。
class InputPanel : public Widget {
public:
    using SubmitCallback = std::function<void(const std::string&)>;
    using MicToggleCallback = std::function<void(bool)>;

    InputPanel(float x, float y, float width, float height);
    ~InputPanel();

    void OnResize() override {}

    /// 渲染所有内容：背景 + 输入框 + 发送按钮 + 麦克风按钮（RenderContext 版本）
    void Render(const RenderContext& ctx) override;

    /// 渲染所有内容：背景 + 输入框 + 发送按钮 + 麦克风按钮（旧转发 wrapper）
    void RenderContent();

    std::string GetText() const;                 // 获取输入框文字
    void SetText(const std::string& text);       // 设置输入框文字
    void SetOnSubmit(SubmitCallback cb);         // 设置提交回调（回车/点击发送）
    void SetSendButtonColor(const Color& color); // 设置发送按钮颜色（自动计算悬停/按下色）
    void SetInputRatio(float ratio);             // 输入框占内容区比例(0~1)，默认 0.75

    // -- 麦克风 --
    void SetOnMicToggle(MicToggleCallback cb) { on_mic_toggle_ = std::move(cb); }

    void SetVisible(bool visible) { Widget::SetVisible(visible); }

    // -- 外观 setter --
    void SetBackgroundColor(const Color& color) { Widget::SetBackgroundColor(color); }
    void SetBorderColor(const Color& color)     { border_color_ = color; }
    void SetBorderWidth(float w)                { border_width_ = w; }
    void SetCornerRadius(float r)               { corner_radius_ = r; }

private:
    std::unique_ptr<InputText> input_text_;
    std::unique_ptr<Button> send_button_;
    std::unique_ptr<Button> mic_button_;
    SubmitCallback on_submit_;
    MicToggleCallback on_mic_toggle_;
    bool mic_on_ = false;
    Color border_color_{Colors::CreamBorder};
    float border_width_ = 1.0f;
    float padding_ = 8.0f;
    float corner_radius_ = 0.0f;
    float input_ratio_ = 0.88f;  // 输入框占内容区比例
};

}  // namespace media_engine
