// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "media/window.h"
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "log_wrapper.h"
#include "media/media_core.h"

namespace media_engine {

struct Window::Impl {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    ImGuiContext* imgui_ctx = nullptr;
    int width = 0;
    int height = 0;
};

namespace {

void SetupImGui(ImGuiContext* ctx, SDL_Window* window, SDL_Renderer* renderer,
                const WindowConfig& cfg, int width, int height) {
    ImGui::SetCurrentContext(ctx);
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 4.0f;

    if (cfg.transparent_bg) {
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_PopupBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        style.Colors[ImGuiCol_FrameBg] = ImVec4(0.2f, 0.2f, 0.2f, 0.9f);
        style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.3f, 0.5f, 0.8f, 0.5f);
        style.Colors[ImGuiCol_Button] = ImVec4(0.3f, 0.5f, 0.8f, 0.8f);
        style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.4f, 0.6f, 0.9f, 0.9f);
        style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.2f, 0.4f, 0.7f, 1.0f);
    } else {
        style.Colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
    }

    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.5f, 0.5f, 0.5f, 0.5f);

    ImGuiIO& io = ImGui::GetIO();

    if (cfg.use_shared_font) {
        auto& shared = MediaCore::Instance().GetSharedChineseFont();
        if (!shared.data.empty()) {
            ImFontConfig font_cfg;
            font_cfg.FontDataOwnedByAtlas = false;
            io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(shared.data.data()),
                static_cast<int>(shared.data.size()), 16.0f, &font_cfg,
                io.Fonts->GetGlyphRangesChineseFull());
        } else {
            io.Fonts->AddFontDefault();
        }
    } else {
        io.Fonts->AddFontDefault();
    }

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window, renderer)) {
        LOG_ERROR("[Window] Failed to init SDL3 backend");
    }
    if (!ImGui_ImplSDLRenderer3_Init(renderer)) {
        LOG_ERROR("[Window] Failed to init SDLRenderer3 backend");
    }

    LOG_INFO("[Window] Window + ImGui created ({}x{}, transparent={})",
             width, height, cfg.transparent_bg);
}

} // anonymous namespace

Window::Window(const char* title, int width, int height,
               const WindowConfig& cfg)
    : impl_(std::make_unique<Impl>()) {
    impl_->width = width;
    impl_->height = height;

    uint32_t sdl_flags = 0;
    if (cfg.resizable)          sdl_flags |= SDL_WINDOW_RESIZABLE;
    if (cfg.borderless)         sdl_flags |= SDL_WINDOW_BORDERLESS;
    if (cfg.transparent_window) sdl_flags |= SDL_WINDOW_TRANSPARENT;
    if (cfg.skip_taskbar)       sdl_flags |= SDL_WINDOW_UTILITY;
    if (cfg.always_on_top)      sdl_flags |= SDL_WINDOW_ALWAYS_ON_TOP;

    if (!SDL_CreateWindowAndRenderer(title, width, height, sdl_flags,
                                      &impl_->window, &impl_->renderer)) {
        LOG_ERROR("[Window] Failed to create window '{}': {}", title, SDL_GetError());
        return;
    }

    impl_->imgui_ctx = ImGui::CreateContext();
    SetupImGui(impl_->imgui_ctx, impl_->window, impl_->renderer, cfg, width, height);
}

Window::~Window() {
    if (impl_ && impl_->imgui_ctx) {
        ImGui::SetCurrentContext(impl_->imgui_ctx);
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(impl_->imgui_ctx);
        impl_->imgui_ctx = nullptr;
    }
}

Window::Window(Window&& other) noexcept
    : impl_(std::move(other.impl_)) {}

SDL_Window* Window::GetSDLWindow() const { return impl_->window; }
SDL_Renderer* Window::GetSDLRenderer() const { return impl_->renderer; }
int Window::GetWidth() const { return impl_->width; }
int Window::GetHeight() const { return impl_->height; }

void Window::SetWindowSize(int w, int h) {
    impl_->width = w;
    impl_->height = h;
    if (impl_->window) SDL_SetWindowSize(impl_->window, w, h);
}

void Window::BeginFrame(uint8_t clear_r, uint8_t clear_g,
                         uint8_t clear_b, uint8_t clear_a) {
    if (!impl_->renderer || !impl_->imgui_ctx) return;
    SDL_SetRenderDrawColor(impl_->renderer, clear_r, clear_g, clear_b, clear_a);
    SDL_RenderClear(impl_->renderer);
    ImGui::SetCurrentContext(impl_->imgui_ctx);
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void Window::EndFrame() {
    if (!impl_->renderer || !impl_->imgui_ctx) return;
    ImGui::SetCurrentContext(impl_->imgui_ctx);
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), impl_->renderer);
    SDL_RenderPresent(impl_->renderer);
}

void Window::RenderFrame(const std::function<void()>& ui_fn,
                          uint8_t clear_r, uint8_t clear_g,
                          uint8_t clear_b, uint8_t clear_a) {
    if (!ui_fn || !impl_->renderer || !impl_->imgui_ctx) return;

    SDL_SetRenderDrawColor(impl_->renderer, clear_r, clear_g, clear_b, clear_a);
    SDL_RenderClear(impl_->renderer);

    auto* prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(impl_->imgui_ctx);

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ui_fn();

    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), impl_->renderer);

    ImGui::SetCurrentContext(prev);

    SDL_RenderPresent(impl_->renderer);
}

void Window::NotifyResized(int w, int h) {
    impl_->width = w;
    impl_->height = h;
}

void Window::SetAspectRatio(float ratio) {
    if (impl_->window) {
        SDL_SetWindowAspectRatio(impl_->window, ratio, ratio);
    }
}

void Window::SetMinSize(int min_w, int min_h) {
    if (impl_->window) {
        SDL_SetWindowMinimumSize(impl_->window, min_w, min_h);
    }
}

void Window::Show() {
    if (impl_->window) {
        SDL_ShowWindow(impl_->window);
    }
}

void Window::Hide() {
    if (impl_->window) {
        SDL_HideWindow(impl_->window);
    }
}

bool Window::IsShown() const {
    return impl_->window && SDL_GetWindowFlags(impl_->window) & SDL_WINDOW_HIDDEN == 0;
}

void Window::SetTitle(const char* title) {
    if (impl_->window) SDL_SetWindowTitle(impl_->window, title);
}


void Window::SetPosition(int x, int y) {
    if (impl_->window) SDL_SetWindowPosition(impl_->window, x, y);
}

void Window::GetPosition(int* x, int* y) const {
    if (impl_->window) {
        SDL_GetWindowPosition(impl_->window, x, y);
    } else {
        if (x) *x = 0;
        if (y) *y = 0;
    }
}

bool Window::ProcessEvent(const void* sdl_event) {
    if (!impl_->imgui_ctx || !sdl_event) return false;
    auto* prev = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(impl_->imgui_ctx);
    bool ret = ImGui_ImplSDL3_ProcessEvent(static_cast<const SDL_Event*>(sdl_event));
    ImGui::SetCurrentContext(prev);
    return ret;
}

} // namespace media_engine
