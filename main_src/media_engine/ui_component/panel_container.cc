// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "ui_component/panel_container.h"


namespace media_engine {

PanelContainer::PanelContainer(float cont_x, float cont_y, float cont_w, float cont_h,
                               const Config& cfg)
    : btn_h(cfg.bottom_btn_h)
    , border_(media_engine::ScopedStyleVar::FrameBorderSize(1.0f))
    , thin_scrollbar_(media_engine::ScopedStyleVar::ScrollbarSize(2.0f))
{
    auto x = static_cast<float>(cont_x);
    auto y = static_cast<float>(cont_y);
    auto w = static_cast<float>(cont_w);

    // 1. View header: title only
    media_engine::DrawList::Text(x + cfg.title_x, y + cfg.title_y,
        media_engine::Colors::OrangeDeep, cfg.title);

    // 2. Background panel (可换色/圆角或直角)
    float panel_x = static_cast<float>(cont_x) + cfg.panel_pad_x;
    float panel_y = static_cast<float>(cont_y) + cfg.panel_pad_y;
    float panel_w = static_cast<float>(cont_w) - cfg.panel_extra_w;
    float panel_h = static_cast<float>(cont_h) - cfg.panel_extra_h;

    media_engine::DrawList::RoundRect(panel_x, panel_y, panel_w, panel_h, cfg.radius, cfg.bg_color);
    media_engine::DrawList::RoundRectOutline(panel_x, panel_y, panel_w, panel_h, cfg.radius,
        cfg.border_color, 1.0f);

    // 3. Content area (面板内部，自带内边距 + 右侧滚动条间距)
    a.x = panel_x + cfg.inner_pad;
    a.y = panel_y + cfg.inner_pad;
    a.w = panel_w - cfg.inner_pad - cfg.scroll_w_extra;
    a.h = panel_h - cfg.inner_pad * 2;
}

// 快捷构造 → 默认白底圆角
PanelContainer::PanelContainer(float cont_x, float cont_y, float cont_w, float cont_h,
                               const char* title, float bottom_btn_h)
    : PanelContainer(cont_x, cont_y, cont_w, cont_h,
                     Config{title, bottom_btn_h, {255,255,255,255}, {236,224,204,255},
                            6.0f, 12.0f, 12.0f, 32.0f, 12.0f, 44.0f, 24.0f, 56.0f,
                            8.0f, 16.0f})
{
}

PanelContainer::~PanelContainer() = default;

PanelContainer::Split PanelContainer::SplitRight(const Area& area, float side_w) {
    return {{area.x, area.y, area.w - side_w, area.h},
            {area.x + area.w - side_w, area.y, side_w, area.h}};
}

auto PanelContainer::BeginScroll(const Area& area, float btn_h, float gap,
                                 bool scrollbar)
    -> media_engine::ScopedChild
{
    auto flags = scrollbar ? media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar
                           : media_engine::ImGuiWindowFlags_None;
    media_engine::Layout::SetCursorScreenPos(area.x, area.y);
    return media_engine::ScopedChild(
        "panel_scroll", area.w, area.h - btn_h - gap,
        0, flags);
}

}  // namespace media_engine
