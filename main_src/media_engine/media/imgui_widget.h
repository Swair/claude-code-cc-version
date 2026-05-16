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

    void SetLabel(const std::string& label);   // 设置按钮文字
    void SetOnClick(VoidCallback cb);          // 设置点击回调

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
constexpr int ImGuiWindowFlags_NoFocusOnAppearing = 1 << 12;
constexpr int ImGuiWindowFlags_AlwaysVerticalScrollbar = 1 << 14;
constexpr int ImGuiWindowFlags_NoDecoration =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse;

// ============================================================================
// ImGui 工具函数封装 - 避免外部文件直接 include imgui.h
// ============================================================================

/// 窗口位置/尺寸设置
void SetImGuiNextWindowPos(float x, float y);
void SetImGuiNextWindowSize(float w, float h);
void SetImGuiNextWindowBgAlpha(float alpha);

/// 窗口控制
bool ImGuiBegin(const char* name, bool* open = nullptr, int flags = 0);
void ImGuiEnd();

/// 布局控制
void ImGuiPushItemWidth(float width);
void ImGuiPopItemWidth();
void ImGuiSameLine();
void ImGuiSetCursorPos(float x, float y);
void ImGuiSetCursorScreenPos(float x, float y);
void ImGuiPushStyleVar_ItemSpacing(float x, float y);
void ImGuiPopStyleVar(int count = 1);

/// 不可见按钮（用于点击区域检测，返回是否被点击）
bool ImGuiInvisibleButton(const char* id, float w, float h);

/// ImGui 窗口内绘制圆角矩形（填充，color 为 ARGB/IM_COL32 格式）
void DrawFilledRoundRect(float x, float y, float w, float h, float radius, const Color& color);
/// ImGui 窗口内绘制圆角矩形（边框）
void DrawRoundRectOutline(float x, float y, float w, float h, float radius, const Color& color, float thickness = 1.0f);
/// ImGui 窗口内绘制填充三角形
void DrawFilledTriangle(float x1, float y1, float x2, float y2, float x3, float y3, const Color& color);
/// ImGui 窗口内绘制三角形边框
void DrawTriangleOutline(float x1, float y1, float x2, float y2, float x3, float y3, const Color& color, float thickness = 1.0f);

/// 组合组件：圆角面板（填充 + 边框，一步完成）
void DrawPanel(float x, float y, float w, float h, float radius,
               const Color& fill_color, const Color& border_color, float border_thickness = 1.5f);
/// 组合组件：右下角缩放拖拽柄（两个三角叠加）
void DrawResizeGrip(float x, float y, float size,
                    const Color& outer_color, const Color& inner_color);

/// 渲染带圆角背景的图标按钮
/// 使用屏幕坐标定位，不受 WindowPadding 影响
/// @returns true 表示被点击
/// @param id 唯一标识
/// @param icon 图标文本
/// @param x,y 相对当前窗口 content 区域的偏移
/// @param size 按钮宽高
/// @param bg_color 背景色（ARGB 格式）
/// @param text_color 文字色（默认白）
/// @param radius 圆角半径（默认 4.0f）
bool IconButtonRender(const char* id, const char* icon,
                      float x, float y, float size,
                      const Color& bg_color,
                      const Color& text_color = Colors::White,
                      float radius = 4.0f);

/// 文本渲染
void ImGuiText(const char* fmt, ...);
void ImGuiTextUnformatted(const char* text);
/// 文本渲染（自动换行，wrap_width=0 表示窗口右边界）
void ImGuiTextWrapped(const char* text, float wrap_width = 0.0f, const Color& color = Colors::White);
/// 文本渲染（带颜色，无换行）
void ImGuiTextColored(const Color& color, const char* text);
/// 文本渲染（统一接口：颜色 + 可选换行）
void ImGuiText(const char* text, const Color& color = Colors::White, float wrap_width = 0.0f);

/// 滚动
void ImGuiSetScrollHereY(float center_y_ratio = 0.5f);

/// Child 窗口
bool BeginChild(const char* name, float width = 0.0f, float height = 0.0f, int child_flags = 0);
bool BeginChild(const char* name, float width, float height, int child_flags, int window_flags);
void EndChild();

/// 滚动控制
float GetScrollY();
float GetScrollMaxY();
void SetScrollY(float scroll_y);

/// 占位符（用于增长窗口边界）
void Dummy(float width, float height);

/// ImVec2 封装（GetMouseDragDelta 返回类型）
struct ImVec2Wrapper {
    float x, y;
    ImVec2Wrapper(float _x = 0, float _y = 0) : x(_x), y(_y) {}
};

/// 鼠标/交互状态查询
bool IsItemHovered();
bool IsItemActive();
ImVec2Wrapper GetMouseDragDelta(float threshold = 0.0f);
void ResetMouseDragDelta();

/// 鼠标光标类型常量（值匹配 ImGui 1.92.8）
constexpr int ImGuiMouseCursor_None = -1;
constexpr int ImGuiMouseCursor_Arrow = 0;
constexpr int ImGuiMouseCursor_ResizeNWSE = 4;

/// 设置鼠标光标
void SetMouseCursor(int cursor_type);

/// 样式颜色
void PushStyleColor(int color_index, const Color& color);
void PopStyleColor(int count = 1);

/// 获取当前 ImGui 上下文的显示尺寸（viewport 尺寸，NewFrame 后可用）
void ImGuiGetDisplaySize(float* w, float* h);

} // namespace media_engine
