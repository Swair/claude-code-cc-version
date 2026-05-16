// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "scene/ui_renderer.h"
#include "media_engine/media_engine.h"

namespace prosophor {

UIRenderer& UIRenderer::Instance() {
    static UIRenderer instance;
    return instance;
}

UIRenderer::UIRenderer() = default;
UIRenderer::~UIRenderer() = default;

void UIRenderer::RenderContextMenu() {
    if (!visible_ || !context_menu_visible_) return;

    float win_w = 800.0f;
    media_engine::ImGuiGetDisplaySize(&win_w, nullptr);
    float menu_w = 140.0f;
    float menu_h = 100.0f;
    float mx = (static_cast<float>(win_w) - menu_w) / 2.0f;
    float my = 60.0f;

    media_engine::SetImGuiNextWindowPos(mx, my);
    media_engine::SetImGuiNextWindowSize(menu_w, menu_h);
    bool menu_open = true;
    media_engine::ImGuiBegin("context_menu", &menu_open,
        media_engine::ImGuiWindowFlags_NoDecoration |
        media_engine::ImGuiWindowFlags_NoMove);

    // Draw menu background
    media_engine::DrawFilledRoundRect(0, 0, menu_w, menu_h, 8,
        media_engine::Colors::Gray95);
    media_engine::DrawRoundRectOutline(0, 0, menu_w, menu_h, 8,
        media_engine::Colors::Gray70, 1.0f);

    // Chat option — toggles central window (wired by VirtualSprite)
    media_engine::ImGuiSetCursorPos(8, 8);
    if (media_engine::ImGuiInvisibleButton("chat_btn", menu_w - 16, 32)) {
        context_menu_visible_ = false;
        if (on_toggle_chat_) {
            on_toggle_chat_();
        }
    }
    media_engine::ImGuiSetCursorPos(16, 14);
    media_engine::ImGuiTextColored(media_engine::Colors::Gray24, "对话");

    // Exit option
    media_engine::ImGuiSetCursorPos(8, 52);
    if (media_engine::ImGuiInvisibleButton("exit_btn", menu_w - 16, 32)) {
        context_menu_visible_ = false;
        media_engine::MediaCore::Instance().Quit();
    }
    media_engine::ImGuiSetCursorPos(16, 58);
    media_engine::ImGuiTextColored(media_engine::Colors::RedDark, "退出");

    if (!menu_open) context_menu_visible_ = false;
    media_engine::ImGuiEnd();
}

}  // namespace prosophor
