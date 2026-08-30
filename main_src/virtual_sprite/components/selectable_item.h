#pragma once

#include "media_engine/media_engine.h"

namespace prosophor {

/// 可选中列表项 — InvisibleButton + 三态视觉（选中/悬浮/中性）
/// 用于 SplitPanel 左侧列表（config/providers/roles）。
/// @return true 表示被点击
class SelectableItem {
public:
    static bool Render(const char* id, float x, float y, float w, float h,
                       const char* text, bool selected, float sm = 1.0f);
};

} // namespace prosophor
