#include "media_core.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <unordered_map>
#include <fstream>
#include "sdl_common.h"
#include "imgui.h"

namespace media_engine {

namespace {
    const std::unordered_map<SDL_Scancode, EventType> kScancodeMap{
        // 字母键
        {SDL_SCANCODE_A, EventType::A},
        {SDL_SCANCODE_B, EventType::B},
        {SDL_SCANCODE_C, EventType::C},
        {SDL_SCANCODE_D, EventType::D},
        {SDL_SCANCODE_E, EventType::E},
        {SDL_SCANCODE_F, EventType::F},
        {SDL_SCANCODE_G, EventType::G},
        {SDL_SCANCODE_H, EventType::H},
        {SDL_SCANCODE_I, EventType::I},
        {SDL_SCANCODE_J, EventType::J},
        {SDL_SCANCODE_K, EventType::K},
        {SDL_SCANCODE_L, EventType::L},
        {SDL_SCANCODE_M, EventType::M},
        {SDL_SCANCODE_N, EventType::N},
        {SDL_SCANCODE_O, EventType::O},
        {SDL_SCANCODE_P, EventType::P},
        {SDL_SCANCODE_Q, EventType::Q},
        {SDL_SCANCODE_R, EventType::R},
        {SDL_SCANCODE_S, EventType::S},
        {SDL_SCANCODE_T, EventType::T},
        {SDL_SCANCODE_U, EventType::U},
        {SDL_SCANCODE_V, EventType::V},
        {SDL_SCANCODE_W, EventType::W},
        {SDL_SCANCODE_X, EventType::X},
        {SDL_SCANCODE_Y, EventType::Y},
        {SDL_SCANCODE_Z, EventType::Z},
        // 数字键
        {SDL_SCANCODE_0, EventType::NUM_0},
        {SDL_SCANCODE_1, EventType::NUM_1},
        {SDL_SCANCODE_2, EventType::NUM_2},
        {SDL_SCANCODE_3, EventType::NUM_3},
        {SDL_SCANCODE_4, EventType::NUM_4},
        {SDL_SCANCODE_5, EventType::NUM_5},
        {SDL_SCANCODE_6, EventType::NUM_6},
        {SDL_SCANCODE_7, EventType::NUM_7},
        {SDL_SCANCODE_8, EventType::NUM_8},
        {SDL_SCANCODE_9, EventType::NUM_9},
        // 功能键
        {SDL_SCANCODE_F1, EventType::F1},
        {SDL_SCANCODE_F2, EventType::F2},
        {SDL_SCANCODE_F3, EventType::F3},
        {SDL_SCANCODE_F4, EventType::F4},
        {SDL_SCANCODE_F5, EventType::F5},
        {SDL_SCANCODE_F6, EventType::F6},
        {SDL_SCANCODE_F7, EventType::F7},
        {SDL_SCANCODE_F8, EventType::F8},
        {SDL_SCANCODE_F9, EventType::F9},
        {SDL_SCANCODE_F10, EventType::F10},
        {SDL_SCANCODE_F11, EventType::F11},
        {SDL_SCANCODE_F12, EventType::F12},
        // 控制键
        {SDL_SCANCODE_ESCAPE, EventType::ESCAPE},
        {SDL_SCANCODE_TAB, EventType::TAB},
        {SDL_SCANCODE_CAPSLOCK, EventType::CAPSLOCK},
        {SDL_SCANCODE_LSHIFT, EventType::LSHIFT},
        {SDL_SCANCODE_RSHIFT, EventType::RSHIFT},
        {SDL_SCANCODE_LCTRL, EventType::LCTRL},
        {SDL_SCANCODE_RCTRL, EventType::RCTRL},
        {SDL_SCANCODE_LALT, EventType::LALT},
        {SDL_SCANCODE_RALT, EventType::RALT},
        // 导航键
        {SDL_SCANCODE_UP, EventType::UP},
        {SDL_SCANCODE_DOWN, EventType::DOWN},
        {SDL_SCANCODE_LEFT, EventType::LEFT},
        {SDL_SCANCODE_RIGHT, EventType::RIGHT},
        {SDL_SCANCODE_HOME, EventType::HOME},
        {SDL_SCANCODE_END, EventType::END},
        {SDL_SCANCODE_PAGEUP, EventType::PAGEUP},
        {SDL_SCANCODE_PAGEDOWN, EventType::PAGEDOWN},
        {SDL_SCANCODE_INSERT, EventType::INSERT},
        {SDL_SCANCODE_DELETE, EventType::DEL},
        // 常用键
        {SDL_SCANCODE_RETURN, EventType::ENTER},
        {SDL_SCANCODE_SPACE, EventType::SPACE},
        {SDL_SCANCODE_BACKSPACE, EventType::BACKSPACE},
        // 符号键
        {SDL_SCANCODE_MINUS, EventType::MINUS},
        {SDL_SCANCODE_EQUALS, EventType::EQUALS},
        {SDL_SCANCODE_LEFTBRACKET, EventType::LEFTBRACKET},
        {SDL_SCANCODE_RIGHTBRACKET, EventType::RIGHTBRACKET},
        {SDL_SCANCODE_BACKSLASH, EventType::BACKSLASH},
        {SDL_SCANCODE_SEMICOLON, EventType::SEMICOLON},
        {SDL_SCANCODE_APOSTROPHE, EventType::APOSTROPHE},
        {SDL_SCANCODE_GRAVE, EventType::GRAVE},
        {SDL_SCANCODE_COMMA, EventType::COMMA},
        {SDL_SCANCODE_PERIOD, EventType::PERIOD},
        {SDL_SCANCODE_SLASH, EventType::SLASH},
        // 小键盘
        {SDL_SCANCODE_KP_0, EventType::KP_0},
        {SDL_SCANCODE_KP_1, EventType::KP_1},
        {SDL_SCANCODE_KP_2, EventType::KP_2},
        {SDL_SCANCODE_KP_3, EventType::KP_3},
        {SDL_SCANCODE_KP_4, EventType::KP_4},
        {SDL_SCANCODE_KP_5, EventType::KP_5},
        {SDL_SCANCODE_KP_6, EventType::KP_6},
        {SDL_SCANCODE_KP_7, EventType::KP_7},
        {SDL_SCANCODE_KP_8, EventType::KP_8},
        {SDL_SCANCODE_KP_9, EventType::KP_9},
        {SDL_SCANCODE_KP_ENTER, EventType::KP_ENTER},
        {SDL_SCANCODE_KP_PLUS, EventType::KP_PLUS},
        {SDL_SCANCODE_KP_MINUS, EventType::KP_MINUS},
        {SDL_SCANCODE_KP_MULTIPLY, EventType::KP_MULTIPLY},
        {SDL_SCANCODE_KP_DIVIDE, EventType::KP_DIVIDE},
        {SDL_SCANCODE_KP_PERIOD, EventType::KP_PERIOD},
    };
}

static EventType EventConvert(const SDL_Event& sdl_event) {
    if (sdl_event.type != SDL_EVENT_KEY_DOWN)
        return EventType::NONE;
    auto it = kScancodeMap.find(sdl_event.key.scancode);
    return it != kScancodeMap.end() ? it->second : EventType::NONE;
}

void MediaCore::GetKeyboardState(std::vector<EventType>& event_list) {
    auto* keyboardState = SDL_GetKeyboardState(NULL);
    for (const auto& [sc, ev] : kScancodeMap) {
        if (keyboardState[sc])
            event_list.push_back(ev);
    }
}


void MediaCore::MediaInit() {
    SdlResource::Instance().Init();

    LoadSharedChineseFont();

    last_timestamp_ns_ = SDL_GetTicksNS();
    frame_duration_ns_ = 1e9 / FPS_; // 纳秒转换
}

void MediaCore::SetFPS(uint64_t FPS) {
    FPS_ = FPS;
    frame_duration_ns_ = 1e9 / FPS_; // 纳秒转换
}

void MediaCore::FPSControl() {
    // 帧率控制
    uint64_t elapsed = SDL_GetTicksNS() - last_timestamp_ns_;
    if (elapsed < frame_duration_ns_) {
        uint64_t left_sleep = frame_duration_ns_ - elapsed;
        SDL_DelayNS(left_sleep);
        delta_s_ = frame_duration_ns_ / 1.0e9;
        // LOG_INFO("TaskManager", "{}, {}", frame_duration_ns_, delta_s_);
    }
    else {
        delta_s_ = elapsed / 1.0e9;
    }

    // RuntimeFPS_ = 1e9 / (SDL_GetTicksNS() - last_timestamp_ns_);
    // LOG_INFO(SDL_LOG_CATEGORY_APPLICATION, "{}, {}, {}, {}, {}", SDL_GetTicksNS(), last_timestamp_ns_, elapsed, frame_duration_ns_, RuntimeFPS_);

    last_timestamp_ns_ = SDL_GetTicksNS();
}


float MediaCore::GetDeltaTimeS() { return delta_s_; }

void MediaCore::LoadSharedChineseFont() {
    if (!shared_font_.data.empty()) return;
    const char* font_paths[] = {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/simsun.ttc",
        "C:/Windows/Fonts/simhei.ttf",
    };
    for (const auto* path : font_paths) {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f) continue;
        auto sz = f.tellg();
        shared_font_.data.resize(static_cast<std::size_t>(sz));
        f.seekg(0);
        f.read(reinterpret_cast<char*>(shared_font_.data.data()), sz);
        LOG_INFO("[MediaCore] Loaded CJK font ({} bytes) from {}", static_cast<std::size_t>(sz), path);
        return;
    }
    LOG_WARN("[MediaCore] No Chinese font found, atlas will be default");
}

void MediaCore::MainRun() {
    while(!game_exit_) {
        EventProcess();
        Update();

        // Each window: BeginFrame → its own render handlers → EndFrame
        for (auto& w : all_windows_) {
            w->BeginFrame(0, 0, 0, 0);
            auto it = window_handlers_.find(w.get());
            if (it != window_handlers_.end()) {
                for (const auto& handler : it->second) {
                    handler();
                }
            }
            w->EndFrame();
        }

        FPSControl();
    }
}

void MediaCore::RegEventHandler(EventHandler handler) {
    event_handler_list_.push_back(handler);
}

void MediaCore::GetWindowPosition(int* x, int* y) const {
    if (primary_window_) {
        SDL_GetWindowPosition(primary_window_->GetSDLWindow(), x, y);
    }
}

void MediaCore::SetWindowSize(int w, int h) {
    if (!primary_window_) return;
    primary_window_->SetWindowSize(w, h);
}

void MediaCore::GetGlobalMousePosition(float* x, float* y) const {
    SDL_GetGlobalMouseState(x, y);
}

void MediaCore::SetWindowPosition(int x, int y) const {
    if (primary_window_) {
        SDL_SetWindowPosition(primary_window_->GetSDLWindow(), x, y);
    }
}

void MediaCore::CenterWindow(int w, int h) {
    if (!primary_window_) return;
    SDL_Window* win = primary_window_->GetSDLWindow();
    SDL_DisplayID display = SDL_GetDisplayForWindow(win);
    if (display == 0) {
        SetWindowSize(w, h);
        return;
    }
    SDL_Rect bounds;
    if (SDL_GetDisplayBounds(display, &bounds)) {
        primary_window_->SetWindowSize(w, h);
        int cx = bounds.x + (bounds.w - w) / 2;
        int cy = bounds.y + (bounds.h - h) / 2;
        SDL_SetWindowPosition(win, cx, cy);
    }
}

void MediaCore::ClampToDisplay() {
    if (!primary_window_) return;
    SDL_Window* win = primary_window_->GetSDLWindow();
    SDL_DisplayID display = SDL_GetDisplayForWindow(win);
    if (display == 0) return;
    SDL_Rect bounds;
    if (SDL_GetDisplayBounds(display, &bounds)) {
        int wx, wy;
        SDL_GetWindowPosition(win, &wx, &wy);
        int ww = primary_window_->GetWidth(), wh = primary_window_->GetHeight();
        if (wx + ww > bounds.x + bounds.w) wx = bounds.x + bounds.w - ww;
        if (wy + wh > bounds.y + bounds.h) wy = bounds.y + bounds.h - wh;
        if (wx < bounds.x) wx = bounds.x;
        if (wy < bounds.y) wy = bounds.y;
        SDL_SetWindowPosition(win, wx, wy);
    }
}

void MediaCore::RegMouseHandler(Window* window, MouseHandler handler) {
    if (window) {
        mouse_window_handlers_[window].push_back(std::move(handler));
    }
}

bool MediaCore::GetDisplayBoundsForWindow(Window* window, int* x, int* y, int* w, int* h) const {
    if (!window) return false;
    SDL_Window* win = window->GetSDLWindow();
    if (!win) return false;
    SDL_DisplayID display = SDL_GetDisplayForWindow(win);
    if (display == 0) return false;
    SDL_Rect bounds;
    if (!SDL_GetDisplayBounds(display, &bounds)) return false;
    if (x) *x = bounds.x;
    if (y) *y = bounds.y;
    if (w) *w = bounds.w;
    if (h) *h = bounds.h;
    return true;
}

void MediaCore::Quit() {
    game_exit_ = true;
    LOG_INFO("MediaCore, Quit game");
}

void MediaCore::RegUpdateHandler(UpdateHandler handler) {
    update_handlers_list_.push_back(handler);
}

void MediaCore::RegRenderHandler(Window* window, RenderHandler handler) {
    if (window) {
        window_handlers_[window].push_back(std::move(handler));
    }
}

void MediaCore::EventProcess() {
    std::vector<EventType> event_list{};
    SDL_Event event;

    while (SDL_PollEvent(&event) != 0) {
        // Route to ImGui + all windows
        for (auto& w : all_windows_) {
            w->ProcessEvent(&event);
        }

        // Helper: match SDL window ID → our Window*
        auto find_win = [&](SDL_WindowID id) -> Window* {
            if (!id) return nullptr;
            for (auto& w : all_windows_) {
                if (SDL_GetWindowID(w->GetSDLWindow()) == id) return w.get();
            }
            return nullptr;
        };

        // Route to mouse handlers
        MouseEvent me{};
        switch (event.type) {
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
                me.type = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN
                              ? MouseEventType::DOWN : MouseEventType::UP;
                me.x = static_cast<int>(event.button.x);
                me.y = static_cast<int>(event.button.y);
                me.button = static_cast<MouseButton>(event.button.button);
                me.window = find_win(event.button.windowID);
                break;
            case SDL_EVENT_MOUSE_MOTION:
                me.type = MouseEventType::MOTION;
                me.x = static_cast<int>(event.motion.x);
                me.y = static_cast<int>(event.motion.y);
                me.dx = static_cast<int>(event.motion.xrel);
                me.dy = static_cast<int>(event.motion.yrel);
                me.window = find_win(event.motion.windowID);
                break;
            default:
                break;
        }
        // Per-window mouse handlers
        if (me.window) {
            auto wit = mouse_window_handlers_.find(me.window);
            if (wit != mouse_window_handlers_.end()) {
                for (const auto& handler : wit->second) {
                    handler(me);
                }
            }
        }

        // Skip keyboard events captured by ImGui
        bool is_key = event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP ||
                      event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_TEXT_EDITING;
        if (is_key && ImGui::GetIO().WantCaptureKeyboard) {
            continue;
        }

        // Route to app event list
        switch (event.type) {
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                auto* src = find_win(event.window.windowID);
                if (src && src == primary_window_) {
                    src->Hide();
                    continue;
                }
                break;
            }
            case SDL_EVENT_QUIT:
                Quit();
                break;
            case SDL_EVENT_WINDOW_RESIZED: {
                auto* src = find_win(event.window.windowID);
                if (src) {
                    src->NotifyResized(event.window.data1, event.window.data2);
                }
                break;
            }
            default: {
                EventType et = EventConvert(event);
                if (et != EventType::NONE) {
                    event_list.push_back(et);
                }
                break;
            }
        }
    }

    for (const auto& handler : event_handler_list_) {
        handler(event_list);
    }
}

void MediaCore::Update() {
    for (const auto& handler : update_handlers_list_) {
        handler();
    }
}




// ============================================================================
// MediaUtil 实现
// ============================================================================

bool MediaUtil::RectHasIntersection(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    SDL_Rect a = {x1, y1, w1, h1};
    SDL_Rect b = {x2, y2, w2, h2};
    return SDL_HasRectIntersection(&a, &b);
}

bool MediaUtil::DrawTextRect(const std::string& text, float x, float y, float w, float h,
                             const Color& color, const char* font_path) {
    int font_size = static_cast<int>(h);
    Font font(font_path, font_size);
    return font.RenderText(text, x, y, color.r / 255.0f, color.g / 255.0f,
                           color.b / 255.0f, color.a / 255.0f);
}



// ============================================================================
// MediaCore ImGui / multi-window
// ============================================================================

void MediaCore::GetPrimaryDisplaySize(int* w, int* h) {
    SDL_DisplayID id = SDL_GetPrimaryDisplay();
    if (id == 0) { if (w) *w = 1920; if (h) *h = 1080; return; }
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(id);
    if (!mode) { if (w) *w = 1920; if (h) *h = 1080; return; }
    if (w) *w = mode->w;
    if (h) *h = mode->h;
}

void MediaCore::Shutdown() {
    all_windows_.clear();
    primary_window_ = nullptr;
    shared_font_.data.clear();
}

Window* MediaCore::CreateMediaWindow(const char* title, int w, int h,
                                 const WindowConfig& cfg) {
    auto win = std::make_unique<Window>(title, w, h, cfg);
    if (!win->GetSDLWindow()) return nullptr;

    auto* ptr = win.get();
    bool is_first = all_windows_.empty();
    all_windows_.push_back(std::move(win));

    if (is_first) {
        primary_window_ = ptr;
        // Wire up SDL handles for drawer/texture/TTF
        SdlResource::Instance().SetPrimarySDLHandles(
            ptr->GetSDLWindow(), ptr->GetSDLRenderer());
    }

    return ptr;
}

} // namespace media_engine
