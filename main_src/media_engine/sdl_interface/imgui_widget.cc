/**
 * @file imgui_widget.cc
 * @brief ImGui 组件封装实现 - Impl 模式，对外不暴露 ImGui 依赖
 *
 * 所有 ImGui 相关代码都隐藏在此文件中，外部模块通过 imgui_widget.h 调用。
 */

#include "media/imgui_widget.h"
#include "media/colors.h"

// 所有 ImGui 依赖都隐藏在实现文件中
#include "imgui.h"
#include "imgui_internal.h"

namespace media_engine {

// ============================================================================
// Button 实现 - 按钮组件
// ============================================================================
struct Button::Impl {
    std::string label_;         // 按钮文本
    VoidCallback on_click_;     // 点击回调
    VoidCallback on_hold_start_;
    VoidCallback on_hold_end_;
    Color bg_color_;            // 背景色 (默认透明=不覆盖)
    Color hovered_color_;       // 悬停色
    Color active_color_;        // 按下色
    Color text_color_;          // 文字色
    bool was_held_ = false;     // 上一帧的按下状态（用于边缘检测）
};

/**
 * @brief 构造按钮对象
 * @param label 按钮显示文本
 * @param on_click 点击回调函数
 */
Button::Button(const std::string& label, VoidCallback on_click)
    : impl_(std::make_unique<Impl>()) {
    impl_->label_ = label;
    impl_->on_click_ = on_click;
}

/**
 * @brief 析构函数
 */
Button::~Button() = default;

/**
 * @brief 渲染按钮
 * @return true 表示按钮被点击
 */
bool Button::Render() {
    int pushed = 0;
    if (impl_->bg_color_.a > 0)       { Style::PushColor(ImGuiCol_Button, impl_->bg_color_); pushed++; }
    if (impl_->hovered_color_.a > 0)  { Style::PushColor(ImGuiCol_ButtonHovered, impl_->hovered_color_); pushed++; }
    if (impl_->active_color_.a > 0)   { Style::PushColor(ImGuiCol_ButtonActive, impl_->active_color_); pushed++; }

    bool clicked = false;
    if (ImGui::Button(impl_->label_.c_str())) {
        if (impl_->on_click_) {
            impl_->on_click_();
        }
        clicked = true;
    }

    if (pushed > 0) Style::PopColor(pushed);
    return clicked;
}

bool Button::RenderHold() {
    int pushed = 0;
    if (impl_->bg_color_.a > 0)       { Style::PushColor(ImGuiCol_Button, impl_->bg_color_); pushed++; }
    if (impl_->hovered_color_.a > 0)  { Style::PushColor(ImGuiCol_ButtonHovered, impl_->hovered_color_); pushed++; }
    if (impl_->active_color_.a > 0)   { Style::PushColor(ImGuiCol_ButtonActive, impl_->active_color_); pushed++; }

    ImGui::Button(impl_->label_.c_str());

    if (pushed > 0) Style::PopColor(pushed);

    bool held = ImGui::IsItemActive() && ImGui::IsMouseDown(0);
    if (held && !impl_->was_held_) {
        if (impl_->on_hold_start_) impl_->on_hold_start_();
    }
    if (!held && impl_->was_held_) {
        if (impl_->on_hold_end_) impl_->on_hold_end_();
    }
    impl_->was_held_ = held;
    return held;
}

void Button::SetLabel(const std::string& label) {
    impl_->label_ = label;
}

void Button::SetOnClick(VoidCallback cb) {
    impl_->on_click_ = cb;
}

void Button::SetOnHoldStart(VoidCallback cb) {
    impl_->on_hold_start_ = std::move(cb);
}

void Button::SetOnHoldEnd(VoidCallback cb) {
    impl_->on_hold_end_ = std::move(cb);
}

void Button::SetBgColor(const Color& color) {
    impl_->bg_color_ = color;
}

void Button::SetTextColor(const Color& color) {
    impl_->text_color_ = color;
}

void Button::SetHoveredColor(const Color& color) {
    impl_->hovered_color_ = color;
}

void Button::SetActiveColor(const Color& color) {
    impl_->active_color_ = color;
}

// ============================================================================
// InputText 实现 - 文本输入框组件
// ============================================================================
struct InputText::Impl {
    std::string label_;             // 标签
    std::vector<char> buffer_;      // 输入缓冲区
    StringCallback on_text_changed_; // 文本变化回调
    bool enter_returns_true_ = false; // Enter 键返回 true
    bool enter_pressed_ = false;    // Enter 是否被按下
};

/**
 * @brief 构造文本输入框对象
 * @param label 标签
 * @param default_text 默认文本
 * @param max_length 最大长度
 * @param on_text_changed 文本变化回调函数
 */
InputText::InputText(const std::string& label, const std::string& default_text,
                     int max_length, StringCallback on_text_changed)
    : impl_(std::make_unique<Impl>()) {
    impl_->label_ = label;
    impl_->buffer_.resize(max_length);
    strncpy(impl_->buffer_.data(), default_text.c_str(), max_length - 1);
    impl_->buffer_[max_length - 1] = '\0';
    impl_->on_text_changed_ = on_text_changed;
}

/**
 * @brief 析构函数
 */
InputText::~InputText() = default;

/**
 * @brief 获取当前文本
 * @return 当前输入的文本
 */
std::string InputText::GetText() const {
    return std::string(impl_->buffer_.data());
}

/**
 * @brief 设置文本
 * @param text 新的文本内容
 */
void InputText::SetText(const std::string& text) {
    strncpy(impl_->buffer_.data(), text.c_str(), impl_->buffer_.size() - 1);
}

/**
 * @brief 获取底层缓冲区指针（用于细粒度控制）
 * @return 缓冲区指针
 */
char* InputText::GetBuffer() {
    return impl_->buffer_.data();
}

/**
 * @brief 获取缓冲区大小
 * @return 缓冲区容量
 */
int InputText::GetBufferSize() const {
    return static_cast<int>(impl_->buffer_.size());
}

/**
 * @brief 设置 Enter 键返回 true
 * @param enable true=启用
 */
void InputText::SetEnterReturnsTrue(bool enable) {
    impl_->enter_returns_true_ = enable;
}

/**
 * @brief 检查 Enter 键是否被按下
 * @return true 表示 Enter 被按下
 */
bool InputText::IsEnterPressed() const {
    return impl_->enter_pressed_;
}

/**
 * @brief 渲染输入框
 * @return true 表示文本发生变化
 */
bool InputText::Render(float pos_x, float pos_y) {
    impl_->enter_pressed_ = false;

    if (pos_x >= 0.0f) {
        ImGui::SetCursorScreenPos(ImVec2(pos_x, pos_y));
    }

    ImGuiInputTextFlags flags = 0;
    if (impl_->enter_returns_true_) {
        flags |= ImGuiInputTextFlags_EnterReturnsTrue;
    }

    if (bg_color_.a > 0) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(
            bg_color_.r / 255.0f, bg_color_.g / 255.0f,
            bg_color_.b / 255.0f, bg_color_.a / 255.0f));
    }

    bool changed = ImGui::InputText(impl_->label_.c_str(), impl_->buffer_.data(),
                                    impl_->buffer_.size(), flags);

    if (bg_color_.a > 0) {
        ImGui::PopStyleColor();
    }

    if (changed) {
        if (impl_->on_text_changed_) {
            impl_->on_text_changed_(std::string(impl_->buffer_.data()));
        }
        if (impl_->enter_returns_true_ && ImGui::IsItemDeactivatedAfterEdit()) {
            impl_->enter_pressed_ = true;
        }
        return true;
    }

    // 保持输入框焦点（每帧自动聚焦）
    ImGui::SetItemDefaultFocus();

    return false;
}

// ============================================================================
// Keep-alive functions used externally
// ============================================================================

// ============================================================================
// ScrollWindow 实现 - 滚动窗口（通用滚动容器）
// ============================================================================

/**
 * @brief 构造滚动窗口对象
 * @param x 窗口 X 坐标
 * @param y 窗口 Y 坐标
 * @param width 窗口宽度
 * @param height 窗口高度
 */
ScrollWindow::ScrollWindow(float x, float y, float width, float height)
    : x_(x), y_(y), width_(width), height_(height) {
}

/**
 * @brief 析构函数
 */
ScrollWindow::~ScrollWindow() = default;

/**
 * @brief 开始滚动窗口渲染
 * @param name 窗口名称（可用作标题）
 * @param title_color 标题颜色（可选，nullptr 表示不使用自定义颜色）
 * @return true 表示窗口打开成功
 */
bool ScrollWindow::Begin(const std::string& name, const Color* /*title_color*/) {
    // 使用 BeginChild 创建可滚动区域（而非 Begin），
    // 因为 ScrollWindow 总是嵌套在父窗口（ContentArea）内部。
    // BeginChild 是 ImGui 中创建可滚动子区域的正确方式。
    Layout::SetCursorScreenPos(x_, y_);
    Child::Begin(name.c_str(), width_, height_, ImGuiChildFlags_None,
                 ImGuiWindowFlags_AlwaysVerticalScrollbar);

    Style::PushVar_ItemSpacing(4, 4);  // 设置内容间距

    return true;
}

/**
 * @brief 结束滚动窗口渲染
 */
void ScrollWindow::End() {
    // 如果需要滚动到底部
    if (scroll_to_bottom_) {
        Scroll::SetHereY(1.0f);
        scroll_to_bottom_ = false;
    }

    Style::PopVar();  // 恢复间距样式
    Child::End();
}

/**
 * @brief 设置窗口位置
 * @param x X 坐标
 * @param y Y 坐标
 */
void ScrollWindow::SetPosition(float x, float y) {
    x_ = x;
    y_ = y;
}

/**
 * @brief 设置窗口尺寸
 * @param width 宽度
 * @param height 高度
 */
void ScrollWindow::SetSize(float width, float height) {
    width_ = width;
    height_ = height;
}

/**
 * @brief 标记需要滚动到底部
 */
void ScrollWindow::ScrollToBottom() {
    scroll_to_bottom_ = true;
}

/**
 * @brief 检查是否已滚动到底部
 * @return true 表示已滚动到底部
 */
bool ScrollWindow::IsScrolledToBottom() const {
    return scroll_to_bottom_ || Scroll::GetY() >= Scroll::GetMaxY() - 1.0f;
}

// ============================================================================
// Window 实现 — 窗口生命周期、位置尺寸、查询
// ============================================================================

void ImGuiWindow::SetNextPos(float x, float y) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
}

void ImGuiWindow::SetNextSize(float w, float h) {
    ImGui::SetNextWindowSize(ImVec2(w, h));
}

void ImGuiWindow::SetNextSize(float w, float h, int cond) {
    ImGui::SetNextWindowSize(ImVec2(w, h), static_cast<ImGuiCond>(cond));
}

void ImGuiWindow::SetNextBgAlpha(float alpha) {
    ImGui::SetNextWindowBgAlpha(alpha);
}

bool ImGuiWindow::Begin(const char* name, bool* open, int flags) {
    return ImGui::Begin(name, open, flags);
}

void ImGuiWindow::End() {
    ImGui::End();
}

void ImGuiWindow::GetPos(float* x, float* y) {
    ImVec2 pos = ImGui::GetWindowPos();
    if (x) *x = pos.x;
    if (y) *y = pos.y;
}

void ImGuiWindow::GetDisplaySize(float* w, float* h) {
    auto& io = ImGui::GetIO();
    if (w) *w = io.DisplaySize.x;
    if (h) *h = io.DisplaySize.y;
}

float ImGuiWindow::GetWidth() {
    return ImGui::GetWindowWidth();
}

// ============================================================================
// Style 实现 — 样式栈操作
// ============================================================================

void Style::PushItemWidth(float width) {
    ImGui::PushItemWidth(width);
}

void Style::PopItemWidth() {
    ImGui::PopItemWidth();
}

void Style::PushVar_ItemSpacing(float x, float y) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(x, y));
}

void Style::PushVar_WindowBorderSize(float size) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, size);
}

void Style::PushVar_ScrollbarSize(float size) {
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, size);
}

void Style::PushVar_WindowPadding(float x, float y) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(x, y));
}

void Style::PushVar_WindowRounding(float radius) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, radius);
}

void Style::PushVar_FrameBorderSize(float size) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, size);
}

void Style::PushVar_FrameRounding(float radius) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, radius);
}

void Style::PushVar_FramePadding(float x, float y) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(x, y));
}

void Style::PopVar(int count) {
    ImGui::PopStyleVar(count);
}

void Style::PushColor(int color_index, const Color& color) {
    ImGui::PushStyleColor((::ImGuiCol)color_index, ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f));
}

void Style::PopColor(int count) {
    ImGui::PopStyleColor(count);
}

void ID::Push(const char* str_id) {
    ImGui::PushID(str_id);
}

void ID::Pop() {
    ImGui::PopID();
}

// ============================================================================
// Layout 实现 — 布局控制
// ============================================================================

void Layout::SameLine() {
    ImGui::SameLine();
}

void Layout::SameLine(float offset, float spacing) {
    ImGui::SameLine(offset, spacing);
}

void Layout::SetCursorPos(float x, float y) {
    ImGui::SetCursorPos(ImVec2(x, y));
}

void Layout::SetCursorScreenPos(float x, float y) {
    ImGui::SetCursorScreenPos(ImVec2(x, y));
}

void Layout::SetCursorPosX(float x) {
    ImGui::SetCursorPosX(x);
}

void Layout::Dummy(float width, float height) {
    ImGui::Dummy(ImVec2(width, height));
}

float Layout::GetContentRegionAvailWidth() {
    return ImGui::GetContentRegionAvail().x;
}

float Layout::GetFontScale() {
    return ImGui::GetStyle().FontScaleMain;
}

void Layout::GetCursorScreenPos(float* x, float* y) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    if (x) *x = p.x;
    if (y) *y = p.y;
}

bool ImGuiWidget::InvisibleButton(const char* id, float w, float h) {
    bool ret = ImGui::InvisibleButton(id, ImVec2(w, h));
    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return ret;
}

void ImGuiWidget::GetItemRectSize(float* w, float* h) {
    ImVec2 size = ImGui::GetItemRectSize();
    if (w) *w = size.x;
    if (h) *h = size.y;
}

bool ImGuiWidget::IsItemHovered() {
    return ImGui::IsItemHovered();
}

void ImGuiWidget::SetHandCursorOnHover() {
    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

// ============================================================================
// DrawList 实现 — ImGui 2D 绘图原语
// ============================================================================

void DrawList::RoundRect(float x, float y, float w, float h,
                         float radius, const Color& color) {
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(x, y), ImVec2(x + w, y + h), ColorToRGBA(color), radius);
}

void DrawList::Selection(float x, float y, float w, float h, float bar_w,
                         const Color& bar_color, const Color& bg_color,
                         float radius) {
    auto* dl = ImGui::GetWindowDrawList();
    auto bg = ColorToRGBA(bg_color);
    auto bar = ColorToRGBA(bar_color);
    if (radius > 0 && bar_w > 0) {
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + bar_w, y + h), bar, radius,
            ImDrawFlags_RoundCornersLeft);
        dl->AddRectFilled(ImVec2(x + bar_w, y), ImVec2(x + w, y + h), bg, radius,
            ImDrawFlags_RoundCornersRight);
    } else {
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + bar_w, y + h), bar, 0);
        dl->AddRectFilled(ImVec2(x + bar_w, y), ImVec2(x + w, y + h), bg, 0);
    }
}

void DrawList::RoundRectOutline(float x, float y, float w, float h,
                                float radius, const Color& color,
                                float thickness) {
    ImGui::GetWindowDrawList()->AddRect(
        ImVec2(x, y), ImVec2(x + w, y + h), ColorToRGBA(color), radius, 0, thickness);
}

void DrawList::ChannelsSplit(int count) {
    ImGui::GetWindowDrawList()->ChannelsSplit(count);
}

void DrawList::ChannelsSetCurrent(int n) {
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(n);
}

void DrawList::ChannelsMerge() {
    ImGui::GetWindowDrawList()->ChannelsMerge();
}

void DrawList::CircleFilled(float cx, float cy, float radius, const Color& color) {
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(cx, cy), radius, ColorToRGBA(color), 0);
}

void DrawList::CircleOutline(float cx, float cy, float radius, const Color& color,
                              float thickness) {
    ImGui::GetWindowDrawList()->AddCircle(
        ImVec2(cx, cy), radius, ColorToRGBA(color), 0, thickness);
}

void DrawList::Line(float x1, float y1, float x2, float y2, const Color& color,
                     float thickness) {
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(x1, y1), ImVec2(x2, y2), ColorToRGBA(color), thickness);
}

void DrawList::OverlayRectOutline(float x, float y, float w, float h,
                                  float radius, const Color& color,
                                  float thickness) {
    ImDrawList* fg = ImGui::GetForegroundDrawList();
    fg->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), ColorToRGBA(color), radius, 0, thickness);
}

void DrawList::FilledTriangle(float x1, float y1, float x2, float y2,
                              float x3, float y3, const Color& color) {
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ColorToRGBA(color));
}

void DrawList::TriangleOutline(float x1, float y1, float x2, float y2,
                               float x3, float y3, const Color& color,
                               float thickness) {
    ImGui::GetWindowDrawList()->AddTriangle(
        ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ColorToRGBA(color), thickness);
}

void DrawList::Panel(float x, float y, float w, float h, float radius,
                     const Color& fill_color, const Color& border_color,
                     float border_thickness) {
    RoundRect(x, y, w, h, radius, fill_color);
    RoundRectOutline(x, y, w, h, radius, border_color, border_thickness);
}

void DrawList::ResizeGrip(float x, float y, float size,
                          const Color& outer_color, const Color& inner_color) {
    float gx = x + size;
    float gy = y + size;
    FilledTriangle(gx - 8, gy, gx, gy, gx, gy - 8, outer_color);
    FilledTriangle(gx - 4, gy, gx, gy, gx, gy - 4, inner_color);
}

void DrawList::Text(float x, float y, const Color& color, const char* text) {
    ImGui::GetWindowDrawList()->AddText(ImVec2(x, y), ColorToRGBA(color), text);
}

bool ImGuiWidget::IconButton(const char* id, const char* icon,
                         float x, float y, float size,
                         const Color& bg_color, const Color& text_color,
                         float radius) {
    ImVec2 win_pos = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();

    // Screen-coordinate positioning: immune to WindowPadding
    ImGui::SetCursorScreenPos(ImVec2(win_pos.x + x, win_pos.y + y));
    bool clicked = ImGui::InvisibleButton(id, ImVec2(size, size));

    // Hover / active feedback
    ImVec2 p_min(win_pos.x + x, win_pos.y + y);
    ImVec2 p_max(win_pos.x + x + size, win_pos.y + y + size);
    uint32_t draw_color = ColorToRGBA(bg_color);
    if (ImGui::IsItemActive()) {
        // Darken when pressed
        uint32_t rgba = ColorToRGBA(bg_color);
        uint8_t a = (rgba >> 24) & 0xFF;
        uint8_t r = static_cast<uint8_t>(((rgba >> 16) & 0xFF) * 0.75f);
        uint8_t g = static_cast<uint8_t>(((rgba >> 8) & 0xFF) * 0.75f);
        uint8_t b = static_cast<uint8_t>((rgba & 0xFF) * 0.75f);
        draw_color = (a << 24) | (r << 16) | (g << 8) | b;
    } else if (ImGui::IsItemHovered()) {
        // Lighten on hover
        uint32_t rgba = ColorToRGBA(bg_color);
        uint8_t a = (rgba >> 24) & 0xFF;
        uint8_t r = std::min(255, static_cast<int>(((rgba >> 16) & 0xFF) + 40));
        uint8_t g = std::min(255, static_cast<int>(((rgba >> 8) & 0xFF) + 40));
        uint8_t b = std::min(255, static_cast<int>((rgba & 0xFF) + 40));
        draw_color = (a << 24) | (r << 16) | (g << 8) | b;
    }

    // Background rounded rect
    dl->AddRectFilled(p_min, p_max, draw_color, radius);

    // Centered text
    ImVec2 text_size = ImGui::CalcTextSize(icon);
    float text_x = win_pos.x + x + (size - text_size.x) * 0.5f;
    float text_y = win_pos.y + y + (size - text_size.y) * 0.5f;
    dl->AddText(ImVec2(text_x, text_y), ColorToRGBA(text_color), icon);

    return clicked;
}

// ============================================================================
// Text 实现 — 文本渲染
// ============================================================================

void Text::Fmt(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

void Text::Raw(const char* text) {
    ImGui::TextUnformatted(text);
}

void Text::Colored(const Color& color, const char* text) {
    Style::PushColor(ImGuiCol_Text, color);
    ImGui::TextUnformatted(text);
    Style::PopColor();
}

void Text::Wrapped(const char* text, float wrap_width, const Color& color) {
    if (wrap_width > 0.0f) {
        ImGui::PushTextWrapPos(wrap_width);
    }
    Style::PushColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", text);
    Style::PopColor();
    if (wrap_width > 0.0f) {
        ImGui::PopTextWrapPos();
    }
}

float Text::CalcWrappedHeight(const char* text, float wrap_width) {
    ImGui::PushTextWrapPos(wrap_width);
    ImVec2 size = ImGui::CalcTextSize(text, nullptr, false, wrap_width);
    ImGui::PopTextWrapPos();
    return size.y;
}

// ============================================================================
// Child 实现 — Child 窗口
// ============================================================================

bool Child::Begin(const char* name, float width, float height, int child_flags) {
    ImVec2 size(width, height);
    return ImGui::BeginChild(name, size, child_flags);
}

bool Child::Begin(const char* name, float width, float height, int child_flags, int window_flags) {
    ImVec2 size(width, height);
    return ImGui::BeginChild(name, size, child_flags, window_flags);
}

void Child::End() {
    ImGui::EndChild();
}

// ============================================================================
// Scroll 实现 — 滚动控制
// ============================================================================

void Scroll::SetHereY(float center_y_ratio) {
    ImGui::SetScrollHereY(center_y_ratio);
}

float Scroll::GetY() {
    return ImGui::GetScrollY();
}

float Scroll::GetMaxY() {
    return ImGui::GetScrollMaxY();
}

void Scroll::SetY(float scroll_y) {
    ImGui::SetScrollY(scroll_y);
}

// ============================================================================
// Mouse 实现 — 鼠标/交互状态查询
// ============================================================================

bool Mouse::IsItemHovered() {
    return ImGui::IsItemHovered();
}

bool Mouse::IsItemActive() {
    return ImGui::IsItemActive();
}

ImVec2Wrapper Mouse::GetDragDelta(float threshold) {
    ImVec2 delta = ImGui::GetMouseDragDelta(0, threshold);
    return ImVec2Wrapper(delta.x, delta.y);
}

void Mouse::ResetDragDelta() {
    ImGui::ResetMouseDragDelta(0);
}

void Mouse::SetCursor(int cursor_type) {
    ImGui::SetMouseCursor(cursor_type);
}

void Popup::Open(const char* name) {
    ImGui::OpenPopup(name);
}

bool Popup::Begin(const char* name) {
    return ImGui::BeginPopup(name);
}

bool Popup::BeginContextVoid(const char* str_id) {
    return ImGui::BeginPopupContextVoid(str_id);
}

bool Popup::BeginModal(const char* name, bool* open, int flags) {
    return ImGui::BeginPopupModal(name, open, flags);
}

bool Popup::MenuItem(const char* label, bool selected, bool enabled) {
    return ImGui::MenuItem(label, nullptr, selected, enabled);
}

void Popup::End() {
    ImGui::EndPopup();
}

bool TabBar::BeginBar(const char* name) {
    return ImGui::BeginTabBar(name);
}

void TabBar::EndBar() {
    ImGui::EndTabBar();
}

bool TabBar::BeginItem(const char* name) {
    return ImGui::BeginTabItem(name);
}

void TabBar::EndItem() {
    ImGui::EndTabItem();
}

bool ImGuiWidget::Checkbox(const char* label, bool* value) {
    return ImGui::Checkbox(label, value);
}

bool ImGuiWidget::Combo(const char* label, int* current_item, const char* const items[], int items_count) {
    return ImGui::Combo(label, current_item, items, items_count);
}

bool ImGuiWidget::InputText(const char* label, char* buf, size_t buf_size) {
    return ImGui::InputText(label, buf, buf_size);
}

bool ImGuiWidget::InputTextMultiline(const char* label, char* buf, size_t buf_size,
                                      float width, float height, bool read_only) {
    int flags = read_only ? ImGuiInputTextFlags_ReadOnly : 0;
    if (read_only) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
    }
    bool result = ImGui::InputTextMultiline(label, buf, buf_size, ImVec2(width, height), flags);
    if (read_only) {
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }
    return result;
}

bool ImGuiWidget::InputInt(const char* label, int* value) {
    return ImGui::InputInt(label, value);
}

bool ImGuiWidget::SliderFloat(const char* label, double* value, float min, float max, const char* fmt) {
    float v = static_cast<float>(*value);
    if (ImGui::SliderFloat(label, &v, min, max, fmt)) {
        *value = static_cast<double>(v);
        return true;
    }
    return false;
}

bool ImGuiWidget::Button(const char* label, float width, float height) {
    bool ret = ImGui::Button(label, ImVec2(width, height));
    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return ret;
}

void ImGuiWidget::Separator() {
    ImGui::Separator();
}

bool ImGuiWidget::TreeNode(const char* label) {
    return ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_None);
}

bool ImGuiWidget::TreeNodeEx(const char* label, int flags) {
    return ImGui::TreeNodeEx(label, flags);
}

bool ImGuiWidget::IsTreeNodeOpen(const char* label, int default_open) {
    ImGuiID id = ImGui::GetID(label);
    return ImGui::GetStateStorage()->GetInt(id, default_open) != 0;
}

void ImGuiWidget::TreePop() {
    ImGui::TreePop();
}

void ImGuiWidget::BulletText(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::BulletTextV(fmt, args);
    va_end(args);
}

// ============================================================================
// MenuBar 实现 — 传统菜单栏
// ============================================================================

bool MenuBar::Begin() {
    return ImGui::BeginMenuBar();
}

void MenuBar::End() {
    ImGui::EndMenuBar();
}

// ============================================================================
// Menu 实现 — 菜单
// ============================================================================

bool Menu::Begin(const char* label, bool enabled) {
    return ImGui::BeginMenu(label, enabled);
}

void Menu::End() {
    ImGui::EndMenu();
}

} // namespace media_engine
