#include "components/bordered_container.h"

namespace prosophor {

BorderedContainer::BorderedContainer(const char* name, float width,
                                     const media_engine::Color* bg_color,
                                     float radius,
                                     float pos_x, float pos_y)
    : radius_(radius) {
    if (pos_x >= 0 && pos_y >= 0)
        media_engine::Layout::SetCursorScreenPos(pos_x, pos_y);
    if (radius_ > 0) {
        media_engine::Style::PushVar_WindowRounding(radius_);
        pushed_styles_++;
    }
    media_engine::Style::PushVar_WindowBorderSize(1.0f);
    pushed_styles_++;
    if (bg_color && bg_color->a > 0) {
        media_engine::Style::PushColor(media_engine::Color::ChildBg, *bg_color);
        pushed_colors_ = 1;
    }
    media_engine::Child::Begin(name, width, 0,
        media_engine::ImGuiChildFlags_Borders | media_engine::ImGuiChildFlags_AutoResizeY,
        media_engine::ImGuiWindowFlags_None);
}

BorderedContainer::~BorderedContainer() {
    media_engine::Child::End();
    if (pushed_colors_)
        media_engine::Style::PopColor(pushed_colors_);
    if (pushed_styles_)
        media_engine::Style::PopVar(pushed_styles_);
}

} // namespace prosophor
