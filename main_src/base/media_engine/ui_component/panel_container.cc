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
    TitleBar::Draw(cont_x, cont_y, cfg);

    auto frame = Background::ComputeFrame(cont_x, cont_y, cont_w, cont_h, cfg);
    Background::Draw(frame, cfg);

    a = ContentArea::Compute(frame, cfg);
}

// TitleBar
void PanelContainer::TitleBar::Draw(float cont_x, float cont_y, const Config& cfg) {
    media_engine::DrawList::Text(
        cont_x + cfg.title_x, cont_y + cfg.title_y,
        media_engine::Colors::OrangeDeep, cfg.title);
}

// Background
auto PanelContainer::Background::ComputeFrame(
    float cont_x, float cont_y, float cont_w, float cont_h, const Config& cfg) -> Frame
{
    return {
        cont_x + cfg.panel_pad_x,
        cont_y + cfg.panel_pad_y,
        cont_w - cfg.panel_extra_w,
        cont_h - cfg.panel_extra_h
    };
}

void PanelContainer::Background::Draw(const Frame& frame, const Config& cfg) {
    media_engine::DrawList::RoundRect(
        frame.x, frame.y, frame.w, frame.h, cfg.radius, cfg.bg_color);
    media_engine::DrawList::RoundRectOutline(
        frame.x, frame.y, frame.w, frame.h, cfg.radius,
        cfg.border_color, 1.0f);
}

// ContentArea
Area PanelContainer::ContentArea::Compute(const PanelContainer::Background::Frame& frame, const Config& cfg) {
    return {
        frame.x + cfg.inner_pad,
        frame.y + cfg.inner_pad,
        frame.w - cfg.inner_pad - cfg.scroll_w_extra,
        frame.h - cfg.inner_pad * 2
    };
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
