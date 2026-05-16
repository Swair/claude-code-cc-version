// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "ui_component/label.h"
#include "drawer.h"

namespace media_engine {

Label::Label(float x, float y, const std::string& text, const Color& color)
    : Widget(x, y), text_(text), color_(color) {}

void Label::Render(const RenderContext& /*ctx*/) {
    RenderAt(x_, y_);
}

void Label::Render() const {
    RenderAt(x_, y_);
}

void Label::RenderAt(float x, float y) const {
    if (!visible_ || text_.empty()) return;

    int max = std::min(max_chars_, static_cast<int>(text_.size()));
    for (int i = 0; i < max; i++) {
        char c = text_[i];
        if (c >= 32 && c < 127) {
            Drawer::Instance().DrawFillRect(x + i * char_step_, y, char_w_, char_h_, color_);
        }
    }
}

} // namespace media_engine
