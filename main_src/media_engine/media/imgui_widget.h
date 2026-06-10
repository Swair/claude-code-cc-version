#pragma once

#include <string>
#include <memory>
#include <functional>
#include <vector>
#include <cstdarg>
#include <stdint.h>
#include "colors.h"

// ============================================================================
// ImGui 组件封装 - 使用 Impl 模式，对外不暴露 imgui.h
// 所有组件都使用 Pimpl 模式，头文件不依赖 ImGui
// ============================================================================

namespace media_engine {

// 回调类型定义
using VoidCallback = std::function<void()>;
using StringCallback = std::function<void(const std::string&)>;

// ============================================================================
// 按钮组件
// ============================================================================
class Button {
public:
    Button(const std::string& label, VoidCallback on_click = nullptr);
    ~Button();

    bool Render();                 // 渲染按钮，返回 true 表示被点击
    bool RenderHold();             // 渲染并返回是否被按住（hold-to-talk）

    void SetLabel(const std::string& label);   // 设置按钮文字
    void SetOnClick(VoidCallback cb);          // 设置点击回调
    void SetOnHoldStart(VoidCallback cb);      // 按下时回调
    void SetOnHoldEnd(VoidCallback cb);        // 松开时回调

    // 按钮颜色
    void SetBgColor(const Color& color);           // 默认背景色
    void SetTextColor(const Color& color);         // 文字颜色
    void SetHoveredColor(const Color& color);      // 鼠标悬停时的颜色
    void SetActiveColor(const Color& color);       // 鼠标按下时的颜色

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ============================================================================
// 文本输入框组件
// ============================================================================
class InputText {
public:
    InputText(const std::string& label, const std::string& default_text = "",
              int max_length = 256, StringCallback on_text_changed = nullptr);
    ~InputText();

    std::string GetText() const;
    void SetText(const std::string& text);

    // 获取底层 buffer 指针（用于细粒度控制）
    char* GetBuffer();
    int GetBufferSize() const;

    // 设置标志
    void SetEnterReturnsTrue(bool enable);
    bool IsEnterPressed() const;  // 检查 Enter 是否被按下

    /// pos_x/pos_y >= 0 时内部自动 SetCursorScreenPos，否则由调用方管理光标位置
    bool Render(float pos_x = -1, float pos_y = -1);

    void SetBackgroundColor(const Color& color) { bg_color_ = color; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    Color bg_color_{0, 0, 0, 0};  // 默认 0=不覆盖 ImGui 风格
};

// ============================================================================
// 滚动窗口 - 通用滚动容器
// ============================================================================
// 用法：
//   ScrollWindow scrollWin(x, y, w, h);
//   scrollWin.Begin("MyScroll");
//   // ... 添加内容 ...
//   scrollWin.End();
// ============================================================================
class ScrollWindow {
public:
    ScrollWindow(float x, float y, float width, float height);
    ~ScrollWindow();

    bool Begin(const std::string& name, const Color* title_color = nullptr);
    void End();

    void SetPosition(float x, float y);
    void SetSize(float width, float height);

    float GetX() const { return x_; }
    float GetY() const { return y_; }
    float GetWidth() const { return width_; }
    float GetHeight() const { return height_; }

    void ScrollToBottom();
    bool IsScrolledToBottom() const;

private:
    float x_, y_;
    float width_, height_;
    bool scroll_to_bottom_ = false;
};

// ============================================================================
// ImGui 窗口标志（常用）— 值匹配 ImGui 1.92.8
// ============================================================================
constexpr int ImGuiWindowFlags_None = 0;
constexpr int ImGuiWindowFlags_NoTitleBar = 1 << 0;
constexpr int ImGuiWindowFlags_NoResize = 1 << 1;
constexpr int ImGuiWindowFlags_NoMove = 1 << 2;
constexpr int ImGuiWindowFlags_NoScrollbar = 1 << 3;
constexpr int ImGuiWindowFlags_NoScrollWithMouse = 1 << 4;
constexpr int ImGuiWindowFlags_NoCollapse = 1 << 5;
constexpr int ImGuiWindowFlags_AlwaysAutoResize = 1 << 6;
constexpr int ImGuiWindowFlags_NoBackground = 1 << 7;
constexpr int ImGuiWindowFlags_NoSavedSettings = 1 << 8;
constexpr int ImGuiWindowFlags_NoMouseInputs = 1 << 9;
constexpr int ImGuiWindowFlags_MenuBar = 1 << 11;
constexpr int ImGuiWindowFlags_NoFocusOnAppearing = 1 << 12;
constexpr int ImGuiWindowFlags_AlwaysVerticalScrollbar = 1 << 14;
constexpr int ImGuiWindowFlags_NoDecoration =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;

// ============================================================================
// ImGui 工具函数封装 - 避免外部文件直接 include imgui.h
// ============================================================================

/// DrawList — ImGui 窗口内 2D 绘图原语集合（静态方法，各方法独立获取 draw list）。
class DrawList {
public:
    static void RoundRect(float x, float y, float w, float h, float radius, const Color& color);
    static void RoundRectOutline(float x, float y, float w, float h, float radius,
                                 const Color& color, float thickness = 1.0f);
    /// Draw selection indicator: left accent bar + filled background
    static void Selection(float x, float y, float w, float h, float bar_w,
                          const Color& bar_color, const Color& bg_color,
                          float radius = 0);
    static void OverlayRectOutline(float x, float y, float w, float h, float radius,
                                   const Color& color, float thickness = 1.0f);
    static void CircleFilled(float cx, float cy, float radius, const Color& color);
    static void CircleOutline(float cx, float cy, float radius, const Color& color,
                              float thickness = 1.0f);
    static void Line(float x1, float y1, float x2, float y2, const Color& color,
                     float thickness = 1.0f);
    static void FilledTriangle(float x1, float y1, float x2, float y2,
                               float x3, float y3, const Color& color);
    static void TriangleOutline(float x1, float y1, float x2, float y2,
                                float x3, float y3, const Color& color,
                                float thickness = 1.0f);
    static void Panel(float x, float y, float w, float h, float radius,
                      const Color& fill_color, const Color& border_color,
                      float border_thickness = 1.5f);
    static void ResizeGrip(float x, float y, float size,
                           const Color& outer_color, const Color& inner_color);
    static void Text(float x, float y, const Color& color, const char* text);
};

/// ImGui 条件常量 — 匹配 ImGuiCond_ enum (power-of-two bits)
constexpr int ImGuiCond_Always = 1;
constexpr int ImGuiCond_Once = 2;
constexpr int ImGuiCond_FirstUseEver = 4;
constexpr int ImGuiCond_Appearing = 8;

/// 窗口控制 — 窗口生命周期、位置尺寸、查询
class ImGuiWindow {
public:
    static void SetNextPos(float x, float y);
    static void SetNextSize(float w, float h);
    static void SetNextSize(float w, float h, int cond);
    static void SetNextBgAlpha(float alpha);
    static bool Begin(const char* name, bool* open = nullptr, int flags = 0);
    static void End();
    static void GetPos(float* x, float* y);
    static void GetDisplaySize(float* w, float* h);
    static float GetWidth();
};

/// Layout — 布局控制（光标位置、同行、占位）
class Layout {
public:
    static void SameLine();
    static void SameLine(float offset, float spacing);
    static void Dummy(float width, float height);
    static void SetCursorPos(float x, float y);
    static void SetCursorPosX(float x);
    static void SetCursorScreenPos(float x, float y);
    static void GetCursorScreenPos(float* x, float* y);
    /// Get the available content region width in the current window/child.
    /// Accounts for window padding and scrollbars. Returns 0 if not inside a window.
    static float GetContentRegionAvailWidth();
    /// Current global font scale (FontScaleMain, 1.0=small, 1.5=large)
    static float GetFontScale();
};

/// 文本渲染
class Text {
public:
    static void Fmt(const char* fmt, ...);
    static void Raw(const char* text);
    static void Colored(const Color& color, const char* text);
    static void Wrapped(const char* text, float wrap_width = 0.0f,
                        const Color& color = Colors::White);
    /// Measure the height of text when wrapped at wrap_width (before rendering)
    static float CalcWrappedHeight(const char* text, float wrap_width);
};

/// Child 窗口
class Child {
public:
    static bool Begin(const char* name, float width = 0.0f, float height = 0.0f,
                      int child_flags = 0);
    static bool Begin(const char* name, float width, float height,
                      int child_flags, int window_flags);
    static void End();
};

/// 滚动控制
class Scroll {
public:
    static void SetHereY(float center_y_ratio = 0.5f);
    static float GetY();
    static float GetMaxY();
    static void SetY(float scroll_y);
};

/// ImVec2 封装（GetDragDelta 返回类型）
struct ImVec2Wrapper {
    float x, y;
    ImVec2Wrapper(float _x = 0, float _y = 0) : x(_x), y(_y) {}
};

/// ID 栈操作
class ID {
public:
    static void Push(const char* str_id);
    static void Pop();
};

/// 鼠标光标类型常量（值匹配 ImGui 1.92.8）
constexpr int ImGuiMouseCursor_None = -1;
constexpr int ImGuiMouseCursor_Arrow = 0;
constexpr int ImGuiMouseCursor_ResizeNWSE = 4;

/// Mouse — 鼠标/交互状态查询
class Mouse {
public:
    static bool IsItemHovered();
    static bool IsItemActive();
    static ImVec2Wrapper GetDragDelta(float threshold = 0.0f);
    static void ResetDragDelta();
    static void SetCursor(int cursor_type);
};

/// Style — 样式栈操作
class Style {
public:
    static void PushColor(int color_index, const Color& color);
    static void PopColor(int count = 1);
    static void PushItemWidth(float width);
    static void PopItemWidth();
    static void PushVar_ItemSpacing(float x, float y);
    static void PushVar_WindowBorderSize(float size);
    static void PushVar_ScrollbarSize(float size);
    static void PushVar_FrameBorderSize(float size);
    static void PushVar_FramePadding(float x, float y);
    static void PushVar_WindowPadding(float x, float y);
    static void PopVar(int count = 1);
};

/// RAII 辅助：作用域内临时修改 StyleVar（单次 PushVar/PopVar）
class ScopedStyleVar {
public:
    ~ScopedStyleVar() { Style::PopVar(1); }
    ScopedStyleVar(const ScopedStyleVar&) = delete;
    ScopedStyleVar& operator=(const ScopedStyleVar&) = delete;

    static ScopedStyleVar ScrollbarSize(float size) {
        Style::PushVar_ScrollbarSize(size); return ScopedStyleVar();
    }
    static ScopedStyleVar WindowBorderSize(float size) {
        Style::PushVar_WindowBorderSize(size); return ScopedStyleVar();
    }
    static ScopedStyleVar ItemSpacing(float x, float y) {
        Style::PushVar_ItemSpacing(x, y); return ScopedStyleVar();
    }
    static ScopedStyleVar FramePadding(float x, float y) {
        Style::PushVar_FramePadding(x, y); return ScopedStyleVar();
    }
    static ScopedStyleVar FrameBorderSize(float size) {
        Style::PushVar_FrameBorderSize(size); return ScopedStyleVar();
    }
    static ScopedStyleVar WindowPadding(float x, float y) {
        Style::PushVar_WindowPadding(x, y); return ScopedStyleVar();
    }
private:
    ScopedStyleVar() = default;
    ScopedStyleVar(ScopedStyleVar&&) = default;
    ScopedStyleVar& operator=(ScopedStyleVar&&) = default;
};

/// RAII 辅助：作用域内临时修改颜色（支持链式 Then 多组颜色）
class ScopedColors {
public:
    explicit ScopedColors(int color_index, const Color& color) {
        Style::PushColor(color_index, color);
    }
    ~ScopedColors() { Style::PopColor(count_); }

    ScopedColors Then(int color_index, const Color& color) & {
        Style::PushColor(color_index, color);
        ++count_;
        return std::move(*this);
    }
    ScopedColors Then(int color_index, const Color& color) && {
        Style::PushColor(color_index, color);
        ++count_;
        return std::move(*this);
    }

    ScopedColors(const ScopedColors&) = delete;
    ScopedColors& operator=(const ScopedColors&) = delete;
    ScopedColors(ScopedColors&& other) noexcept : count_(other.count_) { other.count_ = 0; }
    ScopedColors& operator=(ScopedColors&&) = delete;
private:
    int count_ = 1;
};

/// RAII 辅助：作用域内临时修改 ItemWidth
class ScopedItemWidth {
public:
    explicit ScopedItemWidth(float width) { Style::PushItemWidth(width); }
    ~ScopedItemWidth() { Style::PopItemWidth(); }
    ScopedItemWidth(const ScopedItemWidth&) = delete;
    ScopedItemWidth& operator=(const ScopedItemWidth&) = delete;
};

/// RAII 辅助：通用的 Begin/End 自动配对
/// 注意 ImGui 有两类规则：
///   - Begin()/End(), BeginChild()/EndChild(): ALWAYS call End (unconditional)
///   - BeginMenu/EndMenu, BeginPopup/EndPopup: only call if Begin returned true (conditional)
/// 用 `always_call=true` 处理第一类，`false`（默认）处理第二类。
class ScopedGuard {
public:
    /// @param active      Begin 返回值
    /// @param cleanup     End 回调
    /// @param always_call true=无条件调用 cleanup（Begin/Child），false=仅 active 时调用
    template<typename F>
    explicit ScopedGuard(bool active, F&& cleanup, bool always_call = false)
        : always_call_(always_call), active_(active), cleanup_(std::forward<F>(cleanup)) {}
    ~ScopedGuard() { if (always_call_ || active_) cleanup_(); }
    explicit operator bool() const { return active_; }
    ScopedGuard(const ScopedGuard&) = delete;
    ScopedGuard& operator=(const ScopedGuard&) = delete;
    ScopedGuard(ScopedGuard&&) = delete;
    ScopedGuard& operator=(ScopedGuard&&) = delete;
protected:
    ScopedGuard() = default;  // 供派生类使用
private:
    bool always_call_ = false;
    bool active_ = false;
    std::function<void()> cleanup_;
};

/// 弹出窗口/模态框
class Popup {
public:
    static void Open(const char* name);
    static bool Begin(const char* name);
    static bool BeginContextVoid(const char* str_id);
    static bool BeginModal(const char* name, bool* open, int flags = 0);
    static bool MenuItem(const char* label, bool selected = false, bool enabled = true);
    static void End();
};

/// 标签栏
class TabBar {
public:
    static bool BeginBar(const char* name);
    static void EndBar();
    static bool BeginItem(const char* name);
    static void EndItem();
};

/// 控件（立即模式）
class ImGuiWidget {
public:
    static bool Checkbox(const char* label, bool* value);
    static bool Combo(const char* label, int* current_item, const char* const items[], int items_count);
    static bool InputText(const char* label, char* buf, size_t buf_size);
    static bool InputTextMultiline(const char* label, char* buf, size_t buf_size,
                                    float width, float height, bool read_only = false);
    static bool InputInt(const char* label, int* value);
    static bool SliderFloat(const char* label, double* value, float min, float max, const char* fmt = "%.3f");
    static bool Button(const char* label, float width = 0.0f, float height = 0.0f);
    static void Separator();
    static bool TreeNode(const char* label);
    static void TreePop();
    static void BulletText(const char* fmt, ...);
    static bool InvisibleButton(const char* id, float w, float h);
    static bool IconButton(const char* id, const char* icon,
                           float x, float y, float size,
                           const Color& bg_color,
                           const Color& text_color = Colors::White,
                           float radius = 4.0f);
    /// Get the size of the last rendered item (rect max - rect min)
    static void GetItemRectSize(float* w, float* h);
    /// Whether the last rendered item is hovered
    static bool IsItemHovered();
    /// Set mouse cursor to hand when last item is hovered
    static void SetHandCursorOnHover();
};

/// MenuBar — 传统菜单栏 (ImGui BeginMenuBar / EndMenuBar)
class MenuBar {
public:
    static bool Begin();
    static void End();
};

/// Menu — 菜单 (ImGui BeginMenu / EndMenu)
class Menu {
public:
    static bool Begin(const char* label, bool enabled = true);
    static void End();
};

/// RAII 辅助：Child 窗口
class ScopedChild : public ScopedGuard {
public:
    ScopedChild(const char* name, float w = 0.0f, float h = 0.0f,
                int child_flags = 0, int window_flags = 0)
        : ScopedGuard(Child::Begin(name, w, h, child_flags, window_flags),
                      []{ Child::End(); }, true) {}  // BeginChild: always call EndChild
};

/// RAII 辅助：Popup 菜单
class ScopedPopupMenu : public ScopedGuard {
public:
    explicit ScopedPopupMenu(const char* name)
        : ScopedGuard(Popup::Begin(name), []{ Popup::End(); }) {}  // conditional
};

/// RAII 辅助：Popup 模态框
class ScopedModal : public ScopedGuard {
public:
    ScopedModal(const char* name, bool* open, int flags = 0)
        : ScopedGuard(Popup::BeginModal(name, open, flags), []{ Popup::End(); }) {}  // conditional
};

/// RAII 辅助：MenuBar
class ScopedMenuBar : public ScopedGuard {
public:
    ScopedMenuBar()
        : ScopedGuard(MenuBar::Begin(), []{ MenuBar::End(); }) {}  // conditional
};

/// RAII 辅助：Menu
class ScopedMenu : public ScopedGuard {
public:
    ScopedMenu(const char* label, bool enabled = true)
        : ScopedGuard(Menu::Begin(label, enabled), []{ Menu::End(); }) {}  // conditional
};

/// RAII 辅助：TabBar
class ScopedTabBar : public ScopedGuard {
public:
    explicit ScopedTabBar(const char* name)
        : ScopedGuard(TabBar::BeginBar(name), []{ TabBar::EndBar(); }) {}  // conditional
};

/// RAII 辅助：TabItem
class ScopedTabItem : public ScopedGuard {
public:
    explicit ScopedTabItem(const char* name)
        : ScopedGuard(TabBar::BeginItem(name), []{ TabBar::EndItem(); }) {}
};

} // namespace media_engine
