#pragma once

#include "media_engine/media_engine.h"

namespace prosophor {

/// RAII 辅助：带边框的 ScopedChild（Borders | AutoResizeY），可选底色
class BorderedContainer {
public:
    /// @param pos_x, pos_y  可选显式光标定位（<0 表示不设置，沿用当前光标）
    BorderedContainer(const char* name, float width,
                      const media_engine::Color* bg_color = nullptr,
                      float radius = 6.0f,
                      float pos_x = -1, float pos_y = -1);
    ~BorderedContainer();
    BorderedContainer(const BorderedContainer&) = delete;
    BorderedContainer& operator=(const BorderedContainer&) = delete;
private:
    int pushed_styles_ = 0;
    int pushed_colors_ = 0;
    float radius_;
};

} // namespace prosophor
