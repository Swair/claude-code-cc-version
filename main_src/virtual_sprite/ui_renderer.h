// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <functional>
#include <string>
#include <vector>
#include <algorithm>

namespace media_engine { class Window; }

namespace prosophor {

class UIRenderer {
public:
    static UIRenderer& Instance();

    void RenderContextMenu();
    void RequestContextMenu(media_engine::Window* win);
    void RenderContextMenu(media_engine::Window* current_win);

    using ToggleChatCallback = std::function<void(media_engine::Window*)>;
    void SetOnToggleChat(ToggleChatCallback cb) { on_toggle_chat_ = std::move(cb); }

    using NewSpriteCallback = std::function<void()>;
    void SetOnNewSprite(NewSpriteCallback cb) { on_new_sprite_ = std::move(cb); }

    using ShowMainWindowCallback = std::function<void()>;
    void SetOnShowMainWindow(ShowMainWindowCallback cb) { on_show_main_window_ = std::move(cb); }

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
};

}  // namespace prosophor
