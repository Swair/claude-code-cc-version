#pragma once

#include "media_engine/media_engine.h"

namespace prosophor {

/// 可选中列表容器 — Y 追踪 + 列表项渲染 + 可选 checkbox。
/// 用于 SplitPanel 左侧列表（config/providers/roles）。
class ItemList {
public:
    ItemList(float x, float y, float width, float sm = 1.0f);

    /// 渲染一个可选中列表项。
    /// @param id       唯一 ImGui ID
    /// @param text     显示文字
    /// @param selected 是否选中
    /// @param checked  可选 checkbox 指针（null = 不显示 checkbox）
    /// @return true 表示被点击选中
    bool Item(const char* id, const char* text, bool selected,
              bool* checked = nullptr);

    float NextY() const { return current_y_; }

private:
    float x_, y_, width_;
    float current_y_;
    float item_h_;
    float gap_;
    float sm_;
};

} // namespace prosophor
