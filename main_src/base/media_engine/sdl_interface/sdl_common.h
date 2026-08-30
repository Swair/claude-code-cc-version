#pragma once

// #include <SDL3/SDL_main.h> 不要加，加了就会重复 main，报错
#include <SDL3/SDL.h>
#include <string>
#include <memory>
#include "log_wrapper.h"
#include <SDL3/SDL_init.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_image/SDL_image.h>

namespace media_engine {

class SdlResource {
public:
    static SdlResource& Instance() {
        static SdlResource resource;
        return resource;
    }

    void SetPrimarySDLHandles(SDL_Window* win, SDL_Renderer* ren);

    SDL_Renderer* GetRender();
    MIX_Mixer* GetMixer();
    TTF_TextEngine* GetTtfEngine();

    void Init();

private:
    SdlResource();
    ~SdlResource();
    void SDLInit();
    void TTFInit();
    void MixInit();

    SDL_Renderer* sdl_renderer_ = nullptr;
    // sdl_window_ not stored — GetSDLWindow() on primary_window_ via MediaCore
    TTF_TextEngine* ttf_engine_ = nullptr;
    MIX_Mixer* mixer_ = nullptr;
};

} // namespace media_engine
