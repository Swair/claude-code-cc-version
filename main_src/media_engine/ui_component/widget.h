// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <vector>

#include "colors.h"

namespace media_engine {

/// 帧渲染上下文 — 预留未来扩展（clip rect、全局 alpha 等）
struct RenderContext {};

/// 可定位 UI 组件基类 — 全百分比坐标系统
///
/// 坐标模式：
///   最外层：SetRoot(w_px, h_px) — 传入实际像素
///   子组件：SetPosition(x%, y%, w%, h%) — 全百分比，相对父容器
///   内部自动解算为像素值（x_/y_/width_/height_），供渲染使用
///
/// 布局级联：
///   SetPosition/SetRoot → ResolveSelf → OnResize → 子组件 ResolveSelf → ...
///   父尺寸变化后自动遍历整棵布局树重新解算
class Widget {
public:
    /// @param x_pct  左上角 x 占父容器宽度的百分比 (0~100)
    /// @param y_pct  左上角 y 占父容器高度的百分比 (0~100)
    /// @param w_pct  宽度占父容器宽度的百分比 (0~100)
    /// @param h_pct  高度占父容器高度的百分比 (0~100)
    Widget(float x_pct = 0, float y_pct = 0, float w_pct = 100, float h_pct = 100);
    virtual ~Widget();

    // -- 顶层入口（实际像素） --
    /// 声明本组件为布局树根节点，传入实际像素尺寸。
    void SetRoot(float w_px, float h_px);

    // -- 全百分比坐标 (0~100)，相对父容器 --
    /// @param x_pct  左上角 x = 父.x + 父.w * x_pct%
    /// @param y_pct  左上角 y = 父.y + 父.h * y_pct%
    /// @param w_pct  宽度   = 父.w * w_pct%
    /// @param h_pct  高度   = 父.h * h_pct%
    void SetPosition(float x_pct, float y_pct, float w_pct, float h_pct);

    // -- 像素坐标（只读，渲染用） --
    float GetX() const { return x_; }
    float GetY() const { return y_; }
    float GetWidth() const { return width_; }
    float GetHeight() const { return height_; }

    // -- 直接设置像素坐标（跳过百分比解算，用于手动布局场景） --
    void SetPixelRect(float x, float y, float w, float h);

    // -- 百分比坐标（只读） --
    float GetXPercent() const { return x_pct_; }
    float GetYPercent() const { return y_pct_; }
    float GetWidthPercent() const { return w_pct_; }
    float GetHeightPercent() const { return h_pct_; }

    // -- 布局树 --
    void AddChild(Widget* child);
    void RemoveChild(Widget* child);
    Widget* GetParent() const { return parent_; }
    const std::vector<Widget*>& GetChildren() const { return children_; }

    // -- 布局回调 --
    /// 尺寸变化时自动调用。默认遍历 children_ 并调用 ResolveSelf()。
    /// 派生类可重写（如 UIPanel 传递内容区而非面板区）。
    virtual void OnResize();

    // -- 渲染树 --
    /// 默认实现：画 bg_color_ 背景 + 递归遍历 children_。
    /// 派生类可重写以添加自定义内容渲染。
    virtual void Render(const RenderContext& ctx);

    // -- 可见性 --
    void SetVisible(bool v) { visible_ = v; }
    bool IsVisible() const { return visible_; }

    // -- 样式 --
    void SetBackgroundColor(const Color& c) { bg_color_ = c; }
    const Color& GetBackgroundColor() const { return bg_color_; }

protected:
    /// 根据 parent_ 或 root_ 解算百分比为像素值。
    /// 若尺寸变化则触发 OnResize()。
    void ResolveSelf();

    // 百分比 (用户设)
    float x_pct_ = 0.0f;
    float y_pct_ = 0.0f;
    float w_pct_ = 100.0f;  // 默认填满
    float h_pct_ = 100.0f;

    // 像素 (解算结果，渲染用)
    float x_ = 0.0f;
    float y_ = 0.0f;
    float width_ = 0.0f;
    float height_ = 0.0f;

    bool visible_ = true;
    Color bg_color_{Colors::GrayDarkest};

    Widget* parent_ = nullptr;
    std::vector<Widget*> children_;

private:
    float root_w_ = 0.0f;
    float root_h_ = 0.0f;
    bool has_root_ = false;
};

} // namespace media_engine
