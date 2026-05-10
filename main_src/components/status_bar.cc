// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "components/status_bar.h"
#include "common/log_wrapper.h"

namespace prosophor {

StatusBar::StatusBar(float x, float y, float width, float height) {
    panel_ = std::make_unique<UIPanel>(x, y, width, height, PanelStyle::StatusBar());
}

void StatusBar::SetPosition(float x, float y) {
    panel_->SetPosition(x, y);
}

void StatusBar::SetSize(float width, float height) {
    panel_->SetSize(width, height);
}

void StatusBar::Render() const {
    if (!visible_) return;
    panel_->Render();
}

void StatusBar::RenderContent(const std::string& status_text, AgentRuntimeState state) {
    if (!visible_) return;

    constexpr StateColor kFallbackColor{128, 128, 128, 255};

    StateVisualProps state_props;
    if (state_props_getter_) {
        state_props = state_props_getter_(state);
    } else {
        state_props = MakeVisualProps(kFallbackColor, "Unknown");
    }

    float status_y = panel_->GetY() + 5.0f;

    // 状态图标（根据状态显示不同图标）
    const char* icon = "";
    switch (state) {
        case AgentRuntimeState::IDLE: icon = "○"; break;
        case AgentRuntimeState::BEGINNING: icon = "◐"; break;
        case AgentRuntimeState::EXECUTING_TOOL: icon = "⚙"; break;
        case AgentRuntimeState::WAITING_PERMISSION: icon = "⏳"; break;
        case AgentRuntimeState::STATE_ERROR: icon = "⚠"; break;
        case AgentRuntimeState::COMPLETE: icon = "✓"; break;
        default: icon = "○"; break;
    }

    // 状态图标（用 Unicode 字符通过 SDL_ttf 渲染）
    static const char* kFontPath = "C:/Windows/Fonts/msyh.ttc";
    float window_width = panel_->GetWidth();
    float icon_x = 20;
    MediaUtil::DrawTextRect(icon, icon_x, status_y, 100, 16,
                            state_props.r, state_props.g, state_props.b, 255, kFontPath);

    // 状态名称
    MediaUtil::DrawTextRect(state_props.name, icon_x + 25, status_y, 200, 14,
                            Colors::LightGray, kFontPath);

    // 状态文本
    MediaUtil::DrawTextRect(status_text, icon_x + 120, status_y, 400, 14,
                            Colors::Gray, kFontPath);

    // ESC 提示
    MediaUtil::DrawTextRect("[ESC] Exit", window_width - 100, status_y, 200, 14,
                            Colors::DarkGray, kFontPath);
}

void StatusBar::SetStatePropsGetter(StatePropsGetter getter) {
    state_props_getter_ = getter;
}

}  // namespace prosophor
