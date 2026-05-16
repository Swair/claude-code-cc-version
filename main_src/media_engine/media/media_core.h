#pragma once

#include <string>
#include <list>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdint.h>
#include "audior.h"
#include "font.h"
#include "texture.h"
#include "colors.h"
#include "window.h"
#include <functional>

namespace media_engine {

// ============================================================================
// EventType - 事件类型枚举
// ============================================================================
enum class EventType {
    // 字母键
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // 数字键
    NUM_0, NUM_1, NUM_2, NUM_3, NUM_4, NUM_5, NUM_6, NUM_7, NUM_8, NUM_9,

    // 功能键
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

    // 控制键
    ESCAPE,
    TAB,
    CAPSLOCK,
    LSHIFT, RSHIFT,
    LCTRL, RCTRL,
    LALT, RALT,

    // 导航键
    UP, DOWN, LEFT, RIGHT,
    HOME, END, PAGEUP, PAGEDOWN,
    INSERT, DEL,

    // 常用键
    ENTER,
    SPACE,
    BACKSPACE,

    // 符号键
    MINUS, EQUALS, LEFTBRACKET, RIGHTBRACKET, BACKSLASH,
    SEMICOLON, APOSTROPHE, GRAVE, COMMA, PERIOD, SLASH,

    // 小键盘
    KP_0, KP_1, KP_2, KP_3, KP_4, KP_5, KP_6, KP_7, KP_8, KP_9,
    KP_ENTER, KP_PLUS, KP_MINUS, KP_MULTIPLY, KP_DIVIDE, KP_PERIOD,

    // 其他
    NONE
};


typedef std::function<void(std::vector<EventType> &event_list)> EventHandler;
typedef std::function<void()> UpdateHandler;
typedef std::function<void()> RenderHandler;
enum class MouseEventType { DOWN, UP, MOTION };
enum class MouseButton : int { LEFT = 1, RIGHT = 2, MIDDLE = 3 };

class Window;

struct MouseEvent {
    MouseEventType type;
    int x, y;
    MouseButton button = MouseButton::LEFT;
    int dx, dy;  // relative motion (MOTION)
    Window* window = nullptr;  // source window
};

typedef std::function<void(const MouseEvent&)> MouseHandler;


// ============================================================================
// MediaCore - 媒体核心类（单例模式）
// ============================================================================
class MediaCore {
    public:
        static MediaCore& Instance() {
            static MediaCore instance;
            return instance;
        }

        void MediaInit();
        void SetFPS(uint64_t FPS);
        void FPSControl();
        float GetDeltaTimeS();
        void MainRun();
        void GetKeyboardState(std::vector<EventType>& event_list);

        // 游戏控制
        void Quit();  // 退出游戏

        // 注册事件处理函数，更新函数，渲染函数，鼠标点击处理函数
        void RegEventHandler(EventHandler handler);
        // 注册渲染回调到指定窗口
        void RegRenderHandler(Window* window, RenderHandler handler);
        void RegUpdateHandler(UpdateHandler handler);
        void RegMouseHandler(Window* window, MouseHandler handler);         // per-window

        // 清理所有窗口
        void Shutdown();

        // 多窗口支持：创建额外窗口（自带 SDL + ImGui）
        Window* CreateMediaWindow(const char* title, int w, int h,
                             const WindowConfig& cfg = {});

        // 事件处理函数，更新函数
        void EventProcess();
        void Update();

        // 获取主窗口指针（Texture/Drawer 等传统绘制用）
        Window* GetPrimaryWindow() const { return primary_window_; }

        // 获取/设置窗口尺寸与位置
        static void GetPrimaryDisplaySize(int* w, int *h);
        void SetWindowSize(int w, int h);
        void GetWindowPosition(int* x, int* y) const;
        void GetGlobalMousePosition(float* x, float* y) const;
        void SetWindowPosition(int x, int y) const;
        void CenterWindow(int w, int h);  // 窗口居中并设为指定尺寸
        void ClampToDisplay();            // 确保主窗口不超出屏幕边界
        bool GetDisplayBoundsForWindow(Window* window, int* x, int* y, int* w, int* h) const;

    private:
        MediaCore() = default;

        bool game_exit_ = false;
        uint64_t FPS_ = 60;
        uint64_t RuntimeFPS_ = 0;
        uint64_t frame_duration_ns_ = 1e9 / FPS_;
        uint64_t last_timestamp_ns_ = 0;
        float delta_s_ = 0;

        std::vector<EventHandler> event_handler_list_{};
        std::vector<UpdateHandler> update_handlers_list_{};
        // Per-window render handlers — keyed by Window*, primary window = primary_window_
        std::unordered_map<Window*, std::vector<RenderHandler>> window_handlers_;
        std::unordered_map<Window*, std::vector<MouseHandler>> mouse_window_handlers_;

        Window* primary_window_ = nullptr;
        std::vector<std::unique_ptr<Window>> all_windows_;
};


class MediaUtil {
public:
    static MediaUtil& Instance() {
        static MediaUtil instance;
        return instance;
    }

    // 判断两个矩形是否相交，(x1, y1, w1, h1) 和 (x2, y2, w2, h2) 分别是两个矩形的左上角坐标和宽高
    static bool RectHasIntersection(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2);

    // 快速渲染文字（一次性使用，自动管理字体，字体路径由应用层传入）
    static bool DrawTextRect(const std::string& text, float x, float y, float w, float h,
                             const Color& color, const char* font_path);

private:
    MediaUtil() = default;
};

} // namespace media_engine
