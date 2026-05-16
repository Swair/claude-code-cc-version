// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "ui_component/ui_panel.h"
#include "ui_component/label.h"
#include "drawer.h"

namespace media_engine {

PanelStyle PanelStyle::Default() {
    PanelStyle style;
    style.background_color = Colors::GrayDarkest;
    style.border_color = Colors::Gray31;
    style.has_border = true;
    style.padding = 10.0f;
    return style;
}

PanelStyle PanelStyle::InputField() {
    PanelStyle style;
    style.background_color = Colors::OrangeLight;
    style.border_color = Colors::CreamBorder;
    style.padding = 8.0f;
    style.has_border = true;
    return style;
}

PanelStyle PanelStyle::StatusBar() {
    PanelStyle style;
    style.background_color = Colors::GrayBlack;
    style.border_color = Colors::Gray24;
    style.has_border = true;
    style.padding = 5.0f;
    return style;
}

PanelStyle PanelStyle::ChatPanel() {
    PanelStyle style;
    style.background_color = Colors::Cream;
    style.border_color = Colors::CreamBorder;
    style.has_border = true;
    style.padding = 8.0f;
    return style;
}

PanelStyle PanelStyle::MessageUser() {
    PanelStyle style;
    style.background_color = Colors::BlueSlate;
    style.border_color = Colors::BlueSoft;
    style.padding = 6.0f;
    style.has_border = false;
    return style;
}

PanelStyle PanelStyle::MessageAgent() {
    PanelStyle style;
    style.background_color = Colors::Gray24a;
    style.border_color = Colors::Gray40;
    style.padding = 6.0f;
    style.has_border = false;
    return style;
}

PanelStyle PanelStyle::MessageSystem() {
    PanelStyle style;
    style.background_color = Colors::Gray24b;
    style.border_color = Colors::Gray40;
    style.padding = 6.0f;
    style.has_border = false;
    return style;
}

PanelStyle PanelStyle::Card() {
    PanelStyle style;
    style.background_color = Colors::GrayDarkest;
    style.border_color = Colors::Gray35;
    style.has_border = true;
    style.corner_radius = 8.0f;
    style.padding = 12.0f;
    return style;
}

UIPanel::UIPanel(float x, float y, float width, float height, PanelStyle style)
    : Widget(x, y, width, height), style_(style) {
}

void UIPanel::RenderBackground() const {
    if (!visible_) return;
    Drawer::Instance().DrawFilledRectWithBorder(
        x_, y_, width_, height_,
        style_.background_color, Colors::Transparent);
}

void UIPanel::RenderBorder() const {
    if (!visible_ || !style_.has_border) return;
    Drawer::Instance().DrawFilledRectWithBorder(
        x_, y_, width_, height_,
        Colors::Transparent, style_.border_color);
}

void UIPanel::Render() const {
    if (!visible_) return;
    RenderBackground();
    RenderBorder();
}

float UIPanel::GetContentX() const { return x_ + style_.padding; }

float UIPanel::GetContentY() const {
    if (style_.has_header) {
        return y_ + style_.header_height + style_.padding;
    }
    return y_ + style_.padding;
}

float UIPanel::GetContentWidth() const { return width_ - style_.padding * 2; }

float UIPanel::GetContentHeight() const {
    if (style_.has_header) {
        return height_ - style_.header_height - style_.padding * 2;
    }
    return height_ - style_.padding * 2;
}

void UIPanel::SetStyle(const PanelStyle& style) { style_ = style; }

void UIPanel::OnResize() {
    // 所有子组件全用百分比，直接级联解算
    Widget::OnResize();
}

void UIPanel::Render(const RenderContext& ctx) {
    if (!visible_) return;
    RenderBackground();
    RenderBorder();
    for (auto* child : children_) {
        child->Render(ctx);
    }
}
void UIPanel::RenderFloatText(const std::string& text, FloatPosition pos,
                               float offset_x, float offset_y) const {
    if (!visible_) return;

    Label lbl(0, 0, text, Colors::Gray78);
    float cs = lbl.GetCharStep();
    float ch = lbl.GetCharHeight();

    switch (pos) {
        case FloatPosition::TopLeft:
            lbl.RenderAt(x_ + offset_x, y_ + offset_y);
            return;
        case FloatPosition::TopRight:
            lbl.RenderAt(x_ + width_ - offset_x - text.length() * cs, y_ + offset_y);
            return;
        case FloatPosition::BottomLeft:
            lbl.RenderAt(x_ + offset_x, y_ + height_ - offset_y - ch);
            return;
        case FloatPosition::BottomRight:
            lbl.RenderAt(x_ + width_ - offset_x - text.length() * cs, y_ + offset_y);
            return;
        case FloatPosition::Center:
            lbl.RenderAt(x_ + (width_ - text.length() * cs) / 2,
                         y_ + (height_ - ch) / 2);
            return;
    }
}

}  // namespace media_engine
