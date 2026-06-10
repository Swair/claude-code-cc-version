// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "media_engine.h"
#include <functional>

namespace media_engine {

/// 面板内容区坐标
struct Area { float x, y, w, h; };

// ============================================================================
// PanelContainer — 通用面板外层容器（RAII）
//
// 统一管理：ViewHeader + 背景面板（可换色/圆角/直角）+ 内容区 + 可选滚动条
//
// 用法:
//   // 默认白底圆角
//   PanelContainer f(cont_x, cont_y, cont_w, cont_h, L.Get("view_xxx"));
//   // f.a 即为内容区坐标（已内缩 inner_pad + 右侧滚动条间距）
//
//   // 自定义背景色和直角
//   PanelContainer::Config cfg;
//   cfg.title = "My Panel";
//   cfg.bg_color = Colors::Beige;
//   cfg.radius = 0;
//   PanelContainer f(cont_x, cont_y, cont_w, cont_h, cfg);
// ============================================================================
struct PanelContainer {
    /// 面板配置
    struct Config {
        const char* title = nullptr;                           // 面板标题
        float bottom_btn_h = 0;                                // 底部按钮区域高度
        Color bg_color{255, 255, 255, 255};                    // 背景色（默认白）
        Color border_color{236, 224, 204, 255};                // 边框色（CreamBorder）
        float radius = 6.0f;                                   // 圆角（0 = 直角）

        // ── 标题 & 分隔线坐标（相对于 cont_x/cont_y）──
        float title_x = 12.0f;
        float title_y = 12.0f;
        float sep_y = 32.0f;

        // ── 面板外框偏移（相对于容器 cont_*）──
        float panel_pad_x = 12.0f;      // 面板距容器左
        float panel_pad_y = 44.0f;      // 面板距容器顶
        float panel_extra_w = 24.0f;    // 面板宽缩减（左右合计）
        float panel_extra_h = 56.0f;    // 面板高缩减（上下合计）

        // ── 内容区内边距（相对于面板边框）──
        float inner_pad = 8.0f;         // 内容区距面板边缘
        float scroll_w_extra = 24.0f;   // 内容区右侧滚动条间距
    };

    Area a;            // 内容区坐标（供面板内部使用）
    float btn_h = 0;   // 底部按钮高度

    PanelContainer(float cont_x, float cont_y, float cont_w, float cont_h,
                   const Config& cfg);
    /// 快捷构造（默认 White 圆角）
    PanelContainer(float cont_x, float cont_y, float cont_w, float cont_h,
                   const char* title, float bottom_btn_h = 0);
    ~PanelContainer();

    PanelContainer(PanelContainer&&) = default;
    PanelContainer& operator=(PanelContainer&&) = default;

    /// 将内容区水平分割为 main + side（如 Chat 的右侧角色面板）
    struct Split { Area main; Area side; };
    static Split SplitRight(const Area& area, float side_w);

    /// 可滚动子窗口（预留底部按钮空间）
    /// @param scrollbar true=AlwaysVerticalScrollbar, false=自动/无滚动条
    static auto BeginScroll(const Area& area, float btn_h = 0, float gap = 12.0f,
                            bool scrollbar = true)
        -> media_engine::ScopedChild;

private:
    media_engine::ScopedStyleVar border_;
    media_engine::ScopedStyleVar thin_scrollbar_;
};

}  // namespace media_engine
