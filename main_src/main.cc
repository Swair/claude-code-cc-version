// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "common/log_wrapper.h"
#include "config/config.h"
#include "platform/platform.h"

#ifdef PROSOPHOR_SDL_UI
#include "virtual_sprite/virtual_sprite.h"
#else
#include "ai_coding.h"
#endif

// 统一入口：所有平台都用 main()
int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    prosophor::Platform::SetConsoleUtf8();

    if (prosophor::Platform::IsAlreadyRunning()) {
        fprintf(stderr, "Prosophor is already running.\n");
        return 1;
    }

    const auto& config = prosophor::ProsophorConfig::GetInstance();
    prosophor::InitLog(config.log_level);
    LOG_DEBUG("Prosophor v{}", PROSOPHOR_VERSION);

    try {
#ifdef PROSOPHOR_SDL_UI
        return prosophor::VirtualSprite::GetInstance().Run();
#else
        return prosophor::AiCoding::GetInstance().Run();
#endif
    } catch (const std::exception& e) {
        LOG_ERROR("Fatal error: {}", e.what());
        return 1;
    }
}
