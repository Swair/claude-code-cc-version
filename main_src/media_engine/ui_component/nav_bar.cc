// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "ui_component/nav_bar.h"

#include "imgui.h"
#include "imgui_widget.h"

namespace media_engine {

struct NavBar::Impl {};

NavBar::NavBar() : impl_(std::make_unique<Impl>()) {
    bg_color_ = Colors::OverlayDark;
}
NavBar::~NavBar() = default;

void NavBar::Render(float parent_width, const std::string& status_text,
                    const std::vector<Item>& items) {
    float win_h = ImGui::GetWindowHeight();
    float nav_y = win_h - height_;

    // Compact window at bottom center, auto-sized to content
    ImGui::SetNextWindowPos(ImVec2(parent_width * 0.5f, nav_y), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGuiBegin("##nav_bar", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_AlwaysAutoResize);

    // Background bar matching content width
    float content_w = ImGui::GetWindowWidth();
    DrawFilledRoundRect(0, 0, content_w, height_, 0, bg_color_);

    // Centered content: status text + buttons
    ImGuiSetCursorPos(pad_left_, pad_top_);
    ImGuiTextColored(text_color_, status_text.c_str());
    ImGui::SameLine(0, 6);

    for (size_t i = 0; i < items.size(); i++) {
        if (ImGui::Button(items[i].label.c_str())) {
            if (items[i].on_click) items[i].on_click();
        }
        if (i + 1 < items.size()) ImGui::SameLine(0, 0);
    }

    ImGuiEnd();
}

}  // namespace media_engine
