// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "ui_component/header_bar.h"
#include "drawer.h"

namespace media_engine {

HeaderBar::HeaderBar(float x, float y, float width, float height)
    : Widget(x, y, width, height) {
}

void HeaderBar::Render(const RenderContext& ctx) {
    if (!visible_) return;
    Drawer::Instance().DrawFilledRectWithBorder(
        x_, y_, width_, height_,
        bg_color_, Colors::Transparent);

    label_.SetText(title_);
    label_.RenderAt(x_ + 10, y_ + 7);

    for (auto* child : children_) {
        child->Render(ctx);
    }
}

}  // namespace media_engine
