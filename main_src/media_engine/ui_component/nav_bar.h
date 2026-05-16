// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui_component/widget.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace media_engine {

/// 水平导航栏 - 底部工具栏
///
/// 在父 ImGui 窗口底部渲染一个半透明横条，
/// 左侧显示状态文本（如 "1/3"），右侧排列按钮。
///
/// 用法：
///   NavBar nav_bar;
///   nav_bar.Render(win_w, "1/3", {
///       {"< Prev", [](){ /* ... */ }},
///       {"Next >", [](){ /* ... */ }},
///   });
class NavBar : public Widget {
public:
    struct Item {
        std::string label;
        std::function<void()> on_click;
    };

    NavBar();
    ~NavBar();

    using Widget::Render;

    /// 在父窗口底部渲染导航栏。
    /// @param parent_width  父窗口宽度（用于撑满底部）
    /// @param status_text   左侧状态文本
    /// @param items         按钮列表（水平排列在状态文本右侧）
    void Render(float parent_width, const std::string& status_text,
                const std::vector<Item>& items);

    // -- 外观 setter --
    void SetTextColor(const Color& color)        { text_color_ = color; }
    void SetHeight(float h)                      { height_ = h; }
    void SetPadding(float left, float top)       { pad_left_ = left; pad_top_ = top; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    Color text_color_{Colors::White};
    float pad_left_ = 8.0f;
    float pad_top_ = 3.0f;
};

}  // namespace media_engine
