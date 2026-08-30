// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui_component/label.h"
#include <string>

namespace media_engine {

/// 标题栏组件 - 用于面板顶部的标题条
class HeaderBar : public Widget {
public:
    HeaderBar(float x, float y, float width, float height);
    void OnResize() override {}

    void SetTitle(const std::string& title) { title_ = title; }
    const std::string& GetTitle() const { return title_; }

    void Render(const RenderContext& ctx) override;

    // -- 外观 setter --
    void SetTextColor(const Color& color) { label_.SetColor(color); }

private:
    std::string title_;
    mutable Label label_{0, 0, "", Colors::White};
};

}  // namespace media_engine
