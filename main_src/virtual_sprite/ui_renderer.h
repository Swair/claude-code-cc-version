// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <string>
#include <vector>
#include <algorithm>

// ============================================================================
// UI 渲染器 — 右键上下文菜单管理
// 注：聊天面板/输入面板由 ChatWindow 直接管理，不在此类中。
// ============================================================================

namespace media_engine { class Window; }

namespace prosophor {

/// UIRenderer: 全局 UI 叠加层（仅上下文菜单）
/// 历史说明：曾持有 ChatPanel / InputPanel / StatusBar，但它们从未被渲染，
/// 实为僵尸对象。2026-05 清理后仅保留右键菜单职责。
class UIRenderer {
public:
    static UIRenderer& Instance();

    // ImGui 层上下文菜单渲染（在 ImGuiNewFrame 之后调用）
    void RenderContextMenu();

    // 请求显示上下文菜单（从鼠标事件处理器调用，在下一帧渲染）
    // win: 请求菜单的窗口（用于多窗口路由）
    void RequestContextMenu(media_engine::Window* win);

    // 在指定窗口渲染上下文菜单
    void RenderContextMenu(media_engine::Window* current_win);

    // 右键菜单 "对话" 按钮回调（如 toggle sprite bubble）
    using ToggleChatCallback = std::function<void(media_engine::Window*)>;
    void SetOnToggleChat(ToggleChatCallback cb) { on_toggle_chat_ = std::move(cb); }

    // 右键菜单 "新建" 按钮回调（创建新 sprite）
    using NewSpriteCallback = std::function<void()>;
    void SetOnNewSprite(NewSpriteCallback cb) { on_new_sprite_ = std::move(cb); }

    // 右键菜单 "显示主窗口" 按钮回调
    using ShowMainWindowCallback = std::function<void()>;
    void SetOnShowMainWindow(ShowMainWindowCallback cb) { on_show_main_window_ = std::move(cb); }

    // 右键菜单 "设置" 按钮回调
    using OpenSettingsCallback = std::function<void()>;
    void SetOnOpenSettings(OpenSettingsCallback cb) { on_open_settings_ = std::move(cb); }

    // 全局显隐
    void SetVisible(bool v) { visible_ = v; }
    bool IsVisible() const { return visible_; }

private:
    UIRenderer();
    ~UIRenderer();

    bool visible_ = true;
    std::vector<media_engine::Window*> context_menu_requests_;
    ToggleChatCallback on_toggle_chat_;
    NewSpriteCallback on_new_sprite_;
    ShowMainWindowCallback on_show_main_window_;
    OpenSettingsCallback on_open_settings_;
};

}  // namespace prosophor
