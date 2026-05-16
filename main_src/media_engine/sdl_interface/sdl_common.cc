#include "sdl_common.h"

namespace media_engine {

void SdlResource::SetPrimarySDLHandles(SDL_Window* win, SDL_Renderer* ren) {
    sdl_renderer_ = ren;
    // TTF needs the renderer to create a text engine — do it here
    // (after SDL window/renderer are created).
    TTFInit();
}

SDL_Renderer* SdlResource::GetRender() {
    return sdl_renderer_;
}
MIX_Mixer* SdlResource::GetMixer() {
    return mixer_;
}
TTF_TextEngine* SdlResource::GetTtfEngine() {
    return ttf_engine_;
}

void SdlResource::Init() {
    LOG_INFO("[SdlResource] Initializing SDL subsystems...\n");
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    SDLInit();
    MixInit();
}

SdlResource::SdlResource() {}

SdlResource::~SdlResource() {
    if(ttf_engine_ != nullptr) {
        TTF_DestroyRendererTextEngine(ttf_engine_);
        ttf_engine_ = nullptr;
    }
    TTF_Quit();
    mixer_ = nullptr;
    MIX_Quit();
    SDL_Quit();
}

void SdlResource::SDLInit() {
    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)){
        LOG_ERROR("[SdlResource] SDL init failed: {}", SDL_GetError());
    }
}

void SdlResource::TTFInit() {
    if (!TTF_Init()){
        LOG_ERROR("[SdlResource] TTF init failed: {}", SDL_GetError());
    }
    ttf_engine_ = TTF_CreateRendererTextEngine(sdl_renderer_);
}

void SdlResource::MixInit() {
    if (MIX_Init()){
        LOG_ERROR("[SdlResource] Mix init failed: {}", SDL_GetError());
    }
    mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
    if (mixer_ == nullptr) {
        LOG_ERROR("[SdlResource] Mixer device creation failed: {}", SDL_GetError());
    }
}

} // namespace media_engine
