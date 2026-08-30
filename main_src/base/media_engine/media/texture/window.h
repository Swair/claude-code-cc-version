// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <memory>
#include <functional>

// Opaque forward declarations — no SDL headers needed.
// These must live outside the media_engine namespace so they refer to ::SDL_Window
// and ::SDL_Renderer, not media_engine::SDL_Window.
struct SDL_Window;
struct SDL_Renderer;
struct ImFontAtlas;
struct ImGuiContext;

namespace media_engine {

struct WindowConfig {
    bool transparent_bg = false;      // ImGui transparent style
    bool use_shared_font = true;      // use MediaCore shared ImFontAtlas (CJK)
    bool borderless = false;
    bool transparent_window = false;  // SDL_WINDOW_TRANSPARENT
    bool resizable = true;
    bool skip_taskbar = false;        // SDL_WINDOW_UTILITY (no taskbar entry)
    bool always_on_top = false;       // SDL_WINDOW_ALWAYS_ON_TOP
};

/// Window = SDL window + renderer + ImGui context, all-in-one.
/// No SDL types exposed in the public API.
class Window {
public:
    Window(const char* title, int width, int height,
           const WindowConfig& cfg = {});
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;

    int GetWidth() const;
    int GetHeight() const;
    void SetWindowSize(int w, int h);

    // -- lifecycle -------------------------------------------------------
    // 全包：SDL clear → ui_fn → ImGui render → SDL present
    void RenderFrame(const std::function<void()>& ui_fn,
                     uint8_t clear_r = 0, uint8_t clear_g = 0,
                     uint8_t clear_b = 0, uint8_t clear_a = 0);
    // 分步（主循环用）：SDL clear + ImGui new frame
    void BeginFrame(uint8_t clear_r = 0, uint8_t clear_g = 0,
                    uint8_t clear_b = 0, uint8_t clear_a = 0);
    // 分步：ImGui render + SDL present
    void EndFrame();
    bool ProcessEvent(const void* sdl_event);

    // Called by MediaCore on SDL_EVENT_WINDOW_RESIZED — updates internal size
    void NotifyResized(int w, int h);

    // Show/hide the SDL window
    void Show();
    void Hide();
    bool IsShown() const;

    // Window position (screen coordinates)
    void SetPosition(int x, int y);
    void GetPosition(int* x, int* y) const;

    // Window title
    void SetTitle(const char* title);

    // Aspect ratio lock (width/height, e.g. 4/3 ≈ 1.333f)
    void SetAspectRatio(float ratio);
    void SetMinSize(int min_w, int min_h);

    ImGuiContext* GetImGuiContext() const;
    bool HasTransparentBg() const;

private:
    friend class MediaCore;
    friend class SdlResource;
    friend class Texture;
    SDL_Window* GetSDLWindow() const;
    SDL_Renderer* GetSDLRenderer() const;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace media_engine
