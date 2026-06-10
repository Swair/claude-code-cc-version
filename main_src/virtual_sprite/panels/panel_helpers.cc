// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/layout_config.h"
#include "config/config.h"
#include "media_engine/media_engine.h"
#include "media_engine/media/imgui_widget.h"
#include "common/i18n.h"

namespace prosophor {

// ============================================================================
// SaveCancelPanel — 底部 Save/Cancel 按钮栏（I18n 依赖，保持在 app 层）
// ============================================================================
void SaveCancelPanel(const Area& area, float btn_h,
                     std::function<void()> on_save,
                     std::function<void()> on_cancel)
{
    auto Lc = LayoutConfig{};
    auto& L = I18n::Instance();
    float btn_w = Lc.panel_save_btn_w, btn_d = Lc.panel_btn_gap;
    float by = area.y + area.h - btn_h - 4.0f;
    float bx = area.x + area.w - (on_cancel ? btn_w * 2 + btn_d : btn_w) - Lc.panel_btn_right_gap;
    media_engine::Layout::SetCursorScreenPos(bx, by);
    if (media_engine::ImGuiWidget::Button(L.Get("btn_save").c_str(), btn_w, 0)) {
        if (on_save) on_save();
    }
    if (on_cancel) {
        media_engine::Layout::SameLine();
        media_engine::Layout::Dummy(btn_d, 0);
        media_engine::Layout::SameLine();
        if (media_engine::ImGuiWidget::Button(L.Get("btn_cancel").c_str(), btn_w, 0)) {
            if (on_cancel) on_cancel();
        }
    }
}

// ============================================================================
// SplitView — 左右分割面板布局计算
// ============================================================================
SplitView::SplitView(const Area& content, float left_width,
                     float btn_h, float gap)
{
    auto Lc = LayoutConfig{};
    inner_h = content.h - btn_h - gap - Lc.split_list_text_y;
    divider_x = content.x + left_width;

    left_x = content.x + Lc.split_list_item_gap;
    left_y = content.y + Lc.split_list_item_gap;
    left_w = left_width - Lc.split_list_item_gap * 2;

    right_x = content.x + left_width + Lc.split_left_gap + Lc.split_right_child_pad;
    right_y = content.y + Lc.split_right_child_pad;
    right_w = content.w - left_width - Lc.split_left_gap - Lc.split_right_child_wextra;
}

void SplitView::DrawDivider() const {
    auto Lc = LayoutConfig{};
    media_engine::DrawList::RoundRect(divider_x, right_y - Lc.split_right_child_pad + Lc.split_list_text_y,
        Lc.split_divider_w, inner_h - Lc.split_list_text_y * 2, 0,
        media_engine::Colors::CreamBorder);
}

// ============================================================================
// SectionForm — BeginScroll + SectionCard 表单容器
// ============================================================================
SectionForm::SectionForm(const Area& area, const char* title, float card_h,
                         float btn_h, float gap)
    : _child(PanelFrame::BeginScroll(area, btn_h, gap))
{
    auto Lc = LayoutConfig{};
    cx = area.x;
    wx = cx + Lc.card_content_indent + Lc.card_widget_offset;
    float card_y = area.y + 8.0f;
    float cw = area.w - Lc.section_card_right_margin;
    PanelHelper::SectionCard(cx, card_y, cw, card_h, title);
    iy = card_y + 42.0f;
}

// ============================================================================
// PanelHelper (legacy)
// ============================================================================

PanelHelper::Area PanelHelper::ContentArea(float cont_x, float cont_y, float cont_w, float cont_h) {
    return {cont_x + 12.0f, cont_y + 44.0f, cont_w - 24.0f, cont_h - 56.0f};
}

void PanelHelper::BeginPanel(float cx, float cy, float cw, float ch) {
    media_engine::DrawList::RoundRect(cx, cy, cw, ch, 6.0f, media_engine::Colors::White);
    media_engine::DrawList::RoundRectOutline(cx, cy, cw, ch, 6.0f,
        media_engine::Colors::CreamBorder, 1.0f);
}

void PanelHelper::PanelHeader(const char* title, float cx, float cy, float cw) {
    media_engine::DrawList::Text(cx + 14.0f, cy + 10.0f,
        media_engine::Colors::OrangeDeep, title);
    media_engine::DrawList::RoundRect(cx + 12.0f, cy + 30.0f, cw - 24.0f, 1.0f, 0,
        media_engine::Colors::CreamBorder);
}

media_engine::ScopedChild PanelHelper::BeginScrollContent(
    float cx, float cy, float cw, float ch, float btn_h, float gap)
{
    media_engine::Layout::SetCursorScreenPos(cx + 8.0f, cy + 8.0f);
    return media_engine::ScopedChild(
        "panel_scroll", cw - 16.0f, ch - btn_h - gap - 8.0f,
        0, media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);
}

void PanelHelper::SaveCancelBar(float cx, float cy, float cw, float ch,
                                float btn_h,
                                std::function<void()> on_save,
                                std::function<void()> on_cancel) {
    auto Lc = LayoutConfig{};
    auto& L = I18n::Instance();
    float btn_w = Lc.panel_save_btn_w, btn_d = Lc.panel_btn_gap;
    float by = cy + ch - btn_h - 4.0f;
    float bx = cx + cw - (on_cancel ? btn_w * 2 + btn_d : btn_w) - Lc.panel_btn_right_gap;
    media_engine::Layout::SetCursorScreenPos(bx, by);
    if (media_engine::ImGuiWidget::Button(L.Get("btn_save").c_str(), btn_w, 0)) {
        if (on_save) on_save();
    }
    if (on_cancel) {
        media_engine::Layout::SameLine();
        media_engine::Layout::Dummy(btn_d, 0);
        media_engine::Layout::SameLine();
        if (media_engine::ImGuiWidget::Button(L.Get("btn_cancel").c_str(), btn_w, 0)) {
            if (on_cancel) on_cancel();
        }
    }
}

void PanelHelper::SectionCard(float cx, float cy, float cw, float h, const char* title) {
    auto Lc = LayoutConfig{};
    float sx = cx + Lc.section_card_pad, sw = cw - Lc.section_card_w_extra;
    media_engine::DrawList::RoundRect(sx, cy, sw, h, Lc.panel_radius,
        media_engine::Colors::Beige);
    media_engine::DrawList::RoundRectOutline(sx, cy, sw, h, Lc.panel_radius,
        media_engine::Colors::Gray63, 1.0f);
    media_engine::DrawList::Text(cx + Lc.label_row_pad, cy + 10.0f,
        media_engine::Colors::OrangeDeep, title);
}

float PanelHelper::LabelRow(float cx, float iy, const char* label, float wx,
                            std::function<void()> widget_fn,
                            float spacing_scale) {
    auto Lc = LayoutConfig{};
    media_engine::Layout::SetCursorScreenPos(cx + Lc.label_row_pad, iy);
    media_engine::Text::Colored(media_engine::Colors::Gray55, label);
    media_engine::Layout::SetCursorScreenPos(wx, iy);
    if (widget_fn) widget_fn();
    return iy + Lc.panel_widget_spacing * spacing_scale;
}

float PanelHelper::Spacing(float base, float scale) {
    return base * scale;
}

// ============================================================================
// 通用组件实现
// ============================================================================

float CardBox(float x, float y, float w, const char* title) {
    float h = 60.0f;
    media_engine::DrawList::RoundRect(x, y, w, h, 6.0f, media_engine::Colors::White);
    media_engine::DrawList::RoundRectOutline(x, y, w, h, 6.0f,
        media_engine::Colors::CreamBorder, 1.0f);
    media_engine::DrawList::Text(x + 14.0f, y + 10.0f,
        media_engine::Colors::Gray55, title);
    media_engine::DrawList::RoundRect(x + 12.0f, y + 30.0f, w - 24.0f, 1.0f, 0,
        media_engine::Colors::CreamBorder);
    return y + h;
}

float InfoRow(float x, float iy, const char* label, const char* value, float label_w) {
    auto Lc = LayoutConfig{};
    media_engine::DrawList::Text(x, iy, media_engine::Colors::Gray55, label);
    media_engine::DrawList::Text(x + label_w, iy, media_engine::Colors::Gray40, value);
    return iy + Lc.info_row_h * Spacing();
}

float SeparatorLine(float x, float iy, float w, float spacing) {
    media_engine::DrawList::RoundRect(x, iy, w, 1.0f, 0, media_engine::Colors::CreamBorder);
    return iy + spacing;
}

void PlaceholderView(int cont_x, int cont_y, int cont_w, int cont_h,
                     const char* view_title, const char* section_title,
                     std::initializer_list<const char*> lines)
{
    auto Lc = LayoutConfig{};
    auto& L = I18n::Instance();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, view_title);
    float sm = Spacing();
    float iy = f.a.y + 10.0f * sm;
    if (section_title && section_title[0]) {
        media_engine::DrawList::Text(f.a.x + 14.0f, iy, media_engine::Colors::OrangeDeep, section_title);
        iy = f.a.y + 40.0f * sm;
    }
    for (auto& line : lines) {
        media_engine::DrawList::Text(f.a.x + Lc.content_pad, iy, media_engine::Colors::Gray55, line);
        iy += 22.0f * sm;
    }
    media_engine::DrawList::RoundRect(f.a.x + Lc.content_pad, iy, f.a.w - Lc.content_pad * 2, 1.0f, 0, media_engine::Colors::CreamBorder);
    iy += 16.0f;
    media_engine::DrawList::Text(f.a.x + Lc.content_pad, iy, media_engine::Colors::Gray47, L.Get("panel_coming_soon").c_str());
}

void StatCard(float x, float y, float w, float h,
              const char* title, const char* value,
              const media_engine::Color& accent)
{
    float sm = Spacing();
    media_engine::DrawList::RoundRect(x, y, w, h, 6.0f, media_engine::Colors::White);
    media_engine::DrawList::RoundRectOutline(x, y, w, h, 6.0f,
        media_engine::Colors::CreamBorder, 1.0f);
    media_engine::DrawList::RoundRect(x, y, 3.0f, h, 0, accent);
    media_engine::DrawList::Text(x + 14.0f, y + 10.0f * sm,
        media_engine::Colors::Gray55, title);
    media_engine::DrawList::Text(x + 14.0f, y + 28.0f * sm,
        media_engine::Colors::Gray40, value);
}

} // namespace prosophor
