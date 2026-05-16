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
    Color bg_color_;            // 背景色 (默认透明=不覆盖)
    Color hovered_color_;       // 悬停色
    Color active_color_;        // 按下色
    Color text_color_;          // 文字色
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
    if (impl_->bg_color_.a > 0)       { PushStyleColor(ImGuiCol_Button, impl_->bg_color_); pushed++; }
    if (impl_->hovered_color_.a > 0)  { PushStyleColor(ImGuiCol_ButtonHovered, impl_->hovered_color_); pushed++; }
    if (impl_->active_color_.a > 0)   { PushStyleColor(ImGuiCol_ButtonActive, impl_->active_color_); pushed++; }

    bool clicked = false;
    if (ImGui::Button(impl_->label_.c_str())) {
        if (impl_->on_click_) {
            impl_->on_click_();
        }
        clicked = true;
    }

    if (pushed > 0) PopStyleColor(pushed);
    return clicked;
}

void Button::SetLabel(const std::string& label) {
    impl_->label_ = label;
}

void Button::SetOnClick(VoidCallback cb) {
    impl_->on_click_ = cb;
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
        if (impl_->enter_returns_true_) {
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

bool IsItemHovered() {
    return ImGui::IsItemHovered();
}

bool IsItemActive() {
    return ImGui::IsItemActive();
}

ImVec2Wrapper GetMouseDragDelta(float threshold) {
    ImVec2 delta = ImGui::GetMouseDragDelta(0, threshold);
    return ImVec2Wrapper(delta.x, delta.y);
}

void ResetMouseDragDelta() {
    ImGui::ResetMouseDragDelta(0);
}

void SetMouseCursor(int cursor_type) {
    ImGui::SetMouseCursor(cursor_type);
}

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
    ImGuiSetCursorScreenPos(x_, y_);
    BeginChild(name.c_str(), width_, height_, ImGuiChildFlags_None,
        ImGuiWindowFlags_AlwaysVerticalScrollbar);

    ImGuiPushStyleVar_ItemSpacing(4, 4);  // 设置内容间距

    return true;
}

/**
 * @brief 结束滚动窗口渲染
 */
void ScrollWindow::End() {
    // 如果需要滚动到底部
    if (scroll_to_bottom_) {
        ImGuiSetScrollHereY(1.0f);
        scroll_to_bottom_ = false;
    }

    ImGuiPopStyleVar();  // 恢复间距样式
    EndChild();
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
    return scroll_to_bottom_ || GetScrollY() >= GetScrollMaxY() - 1.0f;
}

// ============================================================================
// ImGui 工具函数封装实现
// ============================================================================

/**
 * @brief 设置下一窗口位置
 * @param x X 坐标
 * @param y Y 坐标
 */
void SetImGuiNextWindowPos(float x, float y) {
    ImGui::SetNextWindowPos(ImVec2(x, y));
}

/**
 * @brief 设置下一窗口尺寸
 * @param w 宽度
 * @param h 高度
 */
void SetImGuiNextWindowSize(float w, float h) {
    ImGui::SetNextWindowSize(ImVec2(w, h));
}

/**
 * @brief 设置下一窗口背景透明度
 * @param alpha 透明度 (0.0-1.0)
 */
void SetImGuiNextWindowBgAlpha(float alpha) {
    ImGui::SetNextWindowBgAlpha(alpha);
}

/**
 * @brief 开始窗口
 * @param name 窗口名称
 * @param open 窗口打开状态指针（可选）
 * @param flags 窗口标志
 * @return true 表示窗口打开成功
 */
bool ImGuiBegin(const char* name, bool* open, int flags) {
    return ImGui::Begin(name, open, flags);
}

/**
 * @brief 结束窗口
 */
void ImGuiEnd() {
    ImGui::End();
}

/**
 * @brief 推入项目宽度
 * @param width 宽度值
 */
void ImGuiPushItemWidth(float width) {
    ImGui::PushItemWidth(width);  // 设置下一项的宽度
}

/**
 * @brief 弹出项目宽度
 */
void ImGuiPopItemWidth() {
    ImGui::PopItemWidth();
}

/**
 * @brief 在同一行渲染下一项
 */
void ImGuiSameLine() {
    ImGui::SameLine();
}

/**
 * @brief 设置光标位置
 * @param x X 坐标
 * @param y Y 坐标
 */
void ImGuiSetCursorPos(float x, float y) {
    ImGui::SetCursorPos(ImVec2(x, y));
}

void ImGuiSetCursorScreenPos(float x, float y) {
    ImGui::SetCursorScreenPos(ImVec2(x, y));
}

/**
 * @brief 推入文本换行位置
 * @param wrap_width 换行宽度
 */
void ImGuiPushTextWrapPos(float wrap_width) {
    ImGui::PushTextWrapPos(wrap_width);
}

/**
 * @brief 弹出文本换行位置
 */
void ImGuiPopTextWrapPos() {
    ImGui::PopTextWrapPos();
}

/**
 * @brief 推入项目间距样式
 * @param x 水平间距
 * @param y 垂直间距
 */
void ImGuiPushStyleVar_ItemSpacing(float x, float y) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(x, y));
}

/**
 * @brief 弹出样式变量
 * @param count 弹出数量
 */
void ImGuiPopStyleVar(int count) {
    ImGui::PopStyleVar(count);
}

bool ImGuiInvisibleButton(const char* id, float w, float h) {
    return ImGui::InvisibleButton(id, ImVec2(w, h));
}

void DrawFilledRoundRect(float x, float y, float w, float h,
                                        float radius, const Color& color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), ColorToRGBA(color), radius);
}

void DrawRoundRectOutline(float x, float y, float w, float h,
                                         float radius, const Color& color,
                                         float thickness) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), ColorToRGBA(color), radius, 0, thickness);
}

void DrawFilledTriangle(float x1, float y1, float x2, float y2,
                                       float x3, float y3, const Color& color) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddTriangleFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ColorToRGBA(color));
}

void DrawTriangleOutline(float x1, float y1, float x2, float y2,
                                        float x3, float y3, const Color& color,
                                        float thickness) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddTriangle(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ColorToRGBA(color), thickness);
}

void DrawPanel(float x, float y, float w, float h, float radius,
                              const Color& fill_color, const Color& border_color,
                              float border_thickness) {
    DrawFilledRoundRect(x, y, w, h, radius, fill_color);
    DrawRoundRectOutline(x, y, w, h, radius, border_color, border_thickness);
}

void DrawResizeGrip(float x, float y, float size,
                                   const Color& outer_color, const Color& inner_color) {
    float gx = x + size;
    float gy = y + size;
    DrawFilledTriangle(gx - 8, gy, gx, gy, gx, gy - 8, outer_color);
    DrawFilledTriangle(gx - 4, gy, gx, gy, gx, gy - 4, inner_color);
}

bool IconButtonRender(const char* id, const char* icon,
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

/**
 * @brief 渲染格式化文本
 * @param fmt 格式字符串
 */
void ImGuiText(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::TextV(fmt, args);
    va_end(args);
}

/**
 * @brief 渲染未格式化文本（直接输出）
 * @param text 文本内容
 */
void ImGuiTextUnformatted(const char* text) {
    ImGui::TextUnformatted(text);
}

/**
 * @brief 渲染带自动换行的文本
 * @param text 文本内容
 * @param wrap_width 换行宽度（0 表示窗口右边界）
 * @param color 文本颜色（默认白色）
 */
void ImGuiTextWrapped(const char* text, float wrap_width, const Color& color) {
    ImGuiText(text, color, wrap_width);
}

/**
 * @brief 渲染带颜色的文本（统一接口）
 * @param text 文本内容
 * @param color 文本颜色（默认白色）
 * @param wrap_width 换行宽度（0 表示无换行）
 */
void ImGuiText(const char* text, const Color& color, float wrap_width) {
    if (wrap_width > 0.0f) {
        ImGuiPushTextWrapPos(wrap_width);
    }
    PushStyleColor(ImGuiCol_Text, color);
    if (wrap_width > 0.0f) {
        ImGui::TextWrapped("%s", text);
    } else {
        ImGui::TextUnformatted(text);
    }
    PopStyleColor();
    if (wrap_width > 0.0f) {
        ImGuiPopTextWrapPos();
    }
}

/**
 * @brief 渲染带颜色的文本（自动恢复样式）
 * @param color 文本颜色
 * @param text 文本内容
 */
void ImGuiTextColored(const Color& color, const char* text) {
    ImGuiText(text, color, 0.0f);  // 委托给统一接口
}

/**
 * @brief 设置滚动位置到当前可见区域
 * @param center_y_ratio 垂直位置比例 (0=顶部，1=底部，0.5=居中)
 */
void ImGuiSetScrollHereY(float center_y_ratio) {
    ImGui::SetScrollHereY(center_y_ratio);
}

/**
 * @brief 开始子窗口（Child）
 * @param name 子窗口名称
 * @param width 宽度
 * @param height 高度
 * @param child_flags 子窗口标志
 * @return true 表示子窗口打开成功
 */
bool BeginChild(const char* name, float width, float height, int child_flags) {
    ImVec2 size(width, height);
    return ImGui::BeginChild(name, size, child_flags);
}

/**
 * @brief 开始子窗口（Child）带窗口标志
 * @param name 子窗口名称
 * @param width 宽度
 * @param height 高度
 * @param child_flags 子窗口标志
 * @param window_flags 窗口标志
 * @return true 表示子窗口打开成功
 */
bool BeginChild(const char* name, float width, float height, int child_flags, int window_flags) {
    ImVec2 size(width, height);
    return ImGui::BeginChild(name, size, child_flags, window_flags);
}

/**
 * @brief 结束子窗口
 */
void EndChild() {
    ImGui::EndChild();
}

/**
 * @brief 获取当前滚动位置
 * @return 滚动 Y 坐标
 */
float GetScrollY() {
    return ImGui::GetScrollY();
}

/**
 * @brief 获取最大滚动位置
 * @return 最大滚动 Y 坐标
 */
float GetScrollMaxY() {
    return ImGui::GetScrollMaxY();
}

/**
 * @brief 设置滚动位置
 * @param scroll_y 滚动 Y 坐标
 */
void SetScrollY(float scroll_y) {
    ImGui::SetScrollY(scroll_y);
}

/**
 * @brief 绘制占位符（用于添加间距）
 * @param width 宽度
 * @param height 高度
 */
void Dummy(float width, float height) {
    ImGui::Dummy(ImVec2(width, height));
}

/**
 * @brief 推送样式颜色
 * @param color_index ImGuiCol 枚举值（如 0=ImGuiCol_Text）
 * @param color 颜色值
 */
void PushStyleColor(int color_index, const Color& color) {
    ImGui::PushStyleColor((::ImGuiCol)color_index, ImVec4(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f));
}

/**
 * @brief 弹出样式颜色
 * @param count 弹出数量
 */
void PopStyleColor(int count) {
    ImGui::PopStyleColor(count);
}

void ImGuiGetDisplaySize(float* w, float* h) {
    auto& io = ImGui::GetIO();
    if (w) *w = io.DisplaySize.x;
    if (h) *h = io.DisplaySize.y;
}

} // namespace media_engine
