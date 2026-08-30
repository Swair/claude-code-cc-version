// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui_component/widget.h"
#include <string>

namespace media_engine {

/// 面板样式配置 - 定义面板的视觉属性
struct PanelStyle {
    Color background_color{20, 20, 20, 200};      // 背景色（RGBA）
    Color border_color{80, 80, 80, 255};          // 边框色（RGBA）
    float border_width = 1.0f;                     // 边框宽度（像素）
    float corner_radius = 0.0f;                    // 圆角半径（暂不支持）
    bool has_border = true;                        // 是否绘制边框
    float padding = 10.0f;                         // 内边距（内容与边框的距离）
    Color header_bg_color{30, 30, 30, 220};       // 标题栏背景色
    bool has_header = false;                       // 是否显示标题栏
    float header_height = 28.0f;                   // 标题栏高度（像素）

    // 预设样式
    static PanelStyle Default();                   // 默认样式
    static PanelStyle InputField();                // 输入框样式（深色背景）
    static PanelStyle StatusBar();                 // 状态栏样式
    static PanelStyle ChatPanel();                 // 聊天面板样式
    static PanelStyle MessageUser();               // 用户消息气泡
    static PanelStyle MessageAgent();              // Agent 消息气泡
    static PanelStyle MessageSystem();             // 系统消息气泡
    static PanelStyle Card();                      // 卡片样式（圆角）
};

/// 浮动文本位置枚举
enum class FloatPosition {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    Center
};

/// 面板组件 - 管理背景、边框和内容区域
///
/// 核心功能：
///   - 背景渲染（SDL 层）
///   - 边框渲染（SDL 层）
///   - 内容区域计算（考虑内边距和标题栏）
///   - 浮动文本渲染（如标题）
class UIPanel : public Widget {
public:
    UIPanel(float x, float y, float width, float height, PanelStyle style = PanelStyle::Default());
    ~UIPanel() = default;

    // --- SDL 渲染方法 ---
    void RenderBackground() const;
    void RenderBorder() const;
    void Render() const;

    // --- 几何属性 ---
    float GetContentX() const;
    float GetContentY() const;
    float GetContentWidth() const;
    float GetContentHeight() const;

    // --- 设置方法 ---
    void SetStyle(const PanelStyle& style);

    // -- 外观 setter（仅修改对应字段，不重置整个 style） --
    void SetBackgroundColor(const Color& color) { Widget::SetBackgroundColor(color); style_.background_color = color; }
    void SetBorderColor(const Color& color)     { style_.border_color = color; }
    void SetBorderWidth(float w)                { style_.border_width = w; }
    void SetCornerRadius(float r)               { style_.corner_radius = r; }
    void SetHasBorder(bool b)                   { style_.has_border = b; }
    void SetPadding(float p)                    { style_.padding = p; }

    // --- 布局级联 ---
    void OnResize() override;

    // --- 渲染树 ---
    void Render(const RenderContext& ctx) override;

    // --- 文本渲染 ---
    void RenderFloatText(const std::string& text, FloatPosition pos = FloatPosition::TopLeft,
                         float offset_x = 10.0f, float offset_y = 8.0f) const;

protected:
    PanelStyle style_;
};

}  // namespace media_engine
