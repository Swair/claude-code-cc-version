// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace media_engine { class Window; class Texture; }

namespace prosophor {

enum class NavItem {
    None = -1,
    Chat,
    Status,
    Sessions,
    Logs,
    Usage,
    Config,
    Roles,
    Providers,
    Memory,
    LocalModels,
    Tts,
    Security,
    Skills,
    KnowledgeBase,
    Scheduler,
    Mcp,
    PetStore,
    ComputerOrganize,
    About,
    ActiveTriggers
};

enum class SidebarGroup {
    Capabilities,
    Monitor,
    Agents,
    Store,
    Settings
};

class Sidebar {
public:
    Sidebar();
    ~Sidebar();

    void Render(int win_w, int win_h);

    NavItem GetActiveItem() const { return active_item_; }
    void SetActiveItem(NavItem item) { active_item_ = item; }

    bool IsCollapsed() const { return collapsed_; }
    void SetCollapsed(bool v) { collapsed_ = v; }
    void ToggleCollapsed() { collapsed_ = !collapsed_; }

    int GetWidth() const;

    using NavCallback = std::function<void(NavItem)>;
    void SetOnNav(NavCallback cb) { on_nav_ = std::move(cb); }
    void SetRenderWindow(media_engine::Window* win) { render_window_ = win; }

private:
    void RenderBrand();
    void RenderCollapseGroup(SidebarGroup group, const char* label);
    void RenderNavItem(NavItem item, const char* icon, const char* label);
    void RenderSeparator();
    void RenderFooter(int win_h, float label_x);

    bool IsGroupExpanded(SidebarGroup group) const;
    void ToggleGroup(SidebarGroup group);

    bool collapsed_ = false;
    NavItem active_item_ = NavItem::Chat;
    NavCallback on_nav_;
    mutable int sidebar_width_ = 300;
    int last_win_h_ = 0;
    std::unordered_map<SidebarGroup, bool> group_expanded_;
    media_engine::Window* render_window_ = nullptr;
    std::unique_ptr<media_engine::Texture> logo_tex_;
};

} // namespace prosophor
