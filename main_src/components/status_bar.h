// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "media_engine/media_engine.h"
#include "ui_types.h"
#include <string>
#include <functional>
#include <memory>

namespace prosophor {

/// 状态栏 - 可复用的状态显示组件
class StatusBar : public media_engine::Widget {
public:
    StatusBar(float x, float y, float width, float height);
    ~StatusBar() = default;

    void OnResize() override;

    void SetStatus(const std::string& text, AgentRuntimeState state) {
        status_text_ = text; state_ = state;
    }

    using media_engine::Widget::Render;

    void Render() const;
    void Render(const media_engine::RenderContext& ctx) override;
    void RenderContent(const std::string& status_text, AgentRuntimeState state);

    using StatePropsGetter = std::function<StateVisualProps(AgentRuntimeState)>;
    void SetStatePropsGetter(StatePropsGetter getter);

    void SetVisible(bool visible) { media_engine::Widget::SetVisible(visible); if (panel_) panel_->SetVisible(visible); }

    // -- 外观 setter --
    void SetBackgroundColor(const media_engine::Color& color) { media_engine::Widget::SetBackgroundColor(color); if (panel_) panel_->SetBackgroundColor(color); }
    void SetBorderColor(const media_engine::Color& color)     { if (panel_) panel_->SetBorderColor(color); }
    void SetBorderWidth(float w)                               { if (panel_) panel_->SetBorderWidth(w); }

private:
    std::unique_ptr<media_engine::UIPanel> panel_;
    StatePropsGetter state_props_getter_;
    std::string status_text_;
    AgentRuntimeState state_ = AgentRuntimeState::IDLE;
};

}  // namespace prosophor
