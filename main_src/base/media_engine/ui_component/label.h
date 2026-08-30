// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui_component/widget.h"
#include <string>

namespace media_engine {

/// 像素文本标签 — 用 Drawer 逐字符绘制，不走 ImGui 字体
class Label : public Widget {
public:
    Label(float x = 0, float y = 0, const std::string& text = "",
          const Color& color = Colors::Gray78);

    void SetText(const std::string& text) { text_ = text; }
    const std::string& GetText() const { return text_; }

    void SetColor(const Color& color) { color_ = color; }
    const Color& GetColor() const { return color_; }

    using Widget::Render;

    /// 在 x_/y_ 位置绘制文本
    void Render() const;

    /// 在 x_/y_ 位置绘制文本（渲染树）
    void Render(const RenderContext& ctx) override;

    /// 在指定位置绘制（不修改 x_/y_）
    void RenderAt(float x, float y) const;

    void SetCharWidth(float w) { char_w_ = w; }
    void SetCharHeight(float h) { char_h_ = h; }
    float GetCharStep() const { return char_step_; }
    float GetCharHeight() const { return char_h_; }

private:
    std::string text_;
    Color color_{Colors::Gray78};
    float char_w_ = 8.0f;
    float char_h_ = 14.0f;
    float char_step_ = 12.0f;   // 字间距
    int max_chars_ = 50;
};

} // namespace media_engine
