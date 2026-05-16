// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <string>

// ============================================================================
// UI 渲染器 — 右键上下文菜单管理
// 注：聊天面板/输入面板由 ChatWindow 直接管理，不在此类中。
// ============================================================================

namespace prosophor {

/// UIRenderer: 全局 UI 叠加层（仅上下文菜单）
/// 历史说明：曾持有 ChatPanel / InputPanel / StatusBar，但它们从未被渲染，
/// 实为僵尸对象。2026-05 清理后仅保留右键菜单职责。
class UIRenderer {
public:
    static UIRenderer& Instance();

    // ImGui 层上下文菜单渲染（在 ImGuiNewFrame 之后调用）
    void RenderContextMenu();

    // 上下文菜单显隐控制
    void ShowContextMenu() { context_menu_visible_ = true; }
    void HideContextMenu() { context_menu_visible_ = false; }
    bool IsContextMenuVisible() const { return context_menu_visible_; }

    // 右键菜单 "对话" 按钮回调（如 toggle central window）
    using ToggleChatCallback = std::function<void()>;
    void SetOnToggleChat(ToggleChatCallback cb) { on_toggle_chat_ = std::move(cb); }

    // 全局显隐
    void SetVisible(bool v) { visible_ = v; }
    bool IsVisible() const { return visible_; }

private:
    UIRenderer();
    ~UIRenderer();

    bool visible_ = true;
    bool context_menu_visible_ = false;
    ToggleChatCallback on_toggle_chat_;
};

}  // namespace prosophor
