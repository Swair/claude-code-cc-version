// Copyright 2026 AiCode Contributors
// SPDX-License-Identifier: Apache-2.0

#include "common/log_wrapper.h"
#include "common/config.h"

#ifdef AICODE_SDL_UI
#include "scene/sdl_app.h"
// SDL 需要 main 函数重定向（Windows 下使用 WIN32 时隐藏控制台）
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR cmdLine, int nCmdShow) {
#else
int main(int argc, char* argv[]) {
#endif

#ifdef _WIN32
    // 隐藏控制台窗口
    FreeConsole();
#else
    (void)argc; (void)argv;
#endif

    const auto& config = aicode::AiCodeConfig::GetInstance();
    aicode::InitLog(config.log_level);
    LOG_INFO("AiCode v{}", AICODE_VERSION);

    try {
#ifdef AICODE_SDL_UI
        // SDL 图形界面模式
        return aicode::SdlApp::GetInstance().Run();
#else
        // 命令行 Agent 模式
        return aicode::AgentCommander::GetInstance().Run();
#endif
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }
}
