// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "components/status_bar.h"
#include "platform/platform.h"
#include "common/log_wrapper.h"

namespace prosophor {

StatusBar::StatusBar(float x, float y, float width, float height)
    : media_engine::Widget(x, y, width, height) {
    panel_ = std::make_unique<media_engine::UIPanel>(0, 0, 100, 100, media_engine::PanelStyle::StatusBar());
    AddChild(panel_.get());
}

void StatusBar::OnResize() {
    Widget::OnResize();  // 级联到 panel_ 填满自身
}

void StatusBar::Render() const {
    if (!visible_) return;
    panel_->Render();
}

void StatusBar::Render(const media_engine::RenderContext& ctx) {
    if (!visible_) return;
    // Recurse children (panel_ draws background)
    for (auto* child : children_) {
        child->Render(ctx);
    }

    StateVisualProps state_props;
    if (state_props_getter_) {
        state_props = state_props_getter_(state_);
    } else {
        state_props = MakeVisualProps(media_engine::Colors::Gray, "Unknown");
    }

    float status_y = panel_->GetY() + 5.0f;

    const char* icon = "";
    switch (state_) {
        case AgentRuntimeState::IDLE: icon = "○"; break;
        case AgentRuntimeState::BEGINNING: icon = "◐"; break;
        case AgentRuntimeState::STREAM_TOOL_START:
        case AgentRuntimeState::STREAM_TOOL:
        case AgentRuntimeState::STREAM_TOOL_END:
        case AgentRuntimeState::EXECUTING_TOOL: icon = "⚙"; break;
        case AgentRuntimeState::WAITING_PERMISSION: icon = "⏳"; break;
        case AgentRuntimeState::STATE_ERROR: icon = "⚠"; break;
        case AgentRuntimeState::COMPLETE: icon = "✓"; break;
        default: icon = "○"; break;
    }

    float window_width = panel_->GetWidth();
    float icon_x = 20;
    media_engine::MediaUtil::DrawTextRect(icon, icon_x, status_y, 100, 16,
                            media_engine::Color(state_props.color.r, state_props.color.g, state_props.color.b, 255),
                            Platform::kDefaultFontPath);

    media_engine::MediaUtil::DrawTextRect(state_props.name, icon_x + 25, status_y, 200, 14,
                            media_engine::Colors::LightGray, Platform::kDefaultFontPath);

    media_engine::MediaUtil::DrawTextRect(status_text_, icon_x + 120, status_y, 400, 14,
                            media_engine::Colors::Gray, Platform::kDefaultFontPath);

    media_engine::MediaUtil::DrawTextRect("[ESC] Exit", window_width - 100, status_y, 200, 14,
                            media_engine::Colors::DarkGray, Platform::kDefaultFontPath);
}

void StatusBar::RenderContent(const std::string& status_text, AgentRuntimeState state) {
    status_text_ = status_text;
    state_ = state;
    Render(media_engine::RenderContext{});
}

void StatusBar::SetStatePropsGetter(StatePropsGetter getter) {
    state_props_getter_ = getter;
}

}  // namespace prosophor
