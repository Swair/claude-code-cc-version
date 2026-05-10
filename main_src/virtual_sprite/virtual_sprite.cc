// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite.h"
#include "scene/agent_state_observer.h"
#include "scene/galgame_mode.h"
#include "scene/ui_renderer.h"
#include "scene/layout_config.h"
#include "media_engine/media_engine.h"
#include "common/log_wrapper.h"
#include "agent_engine.h"

#include <memory>

namespace prosophor {

VirtualSprite& VirtualSprite::GetInstance() {
    static VirtualSprite instance;
    return instance;
}

VirtualSprite::VirtualSprite() = default;

VirtualSprite::~VirtualSprite() {
    Shutdown();
}

void VirtualSprite::HandleTextInput(const char* text) {
    if (input_callback_) {
        InputEvent event;
        event.source = InputSource::SDL;
        event.type = InputEvent::Type::Text;
        event.data = TextInputEvent{text, true};
        input_callback_(event);
    }
}

void VirtualSprite::HandleKeyDown(int key_code) {
    if (current_scene_ == UIMode::GALGAME) {
        GalgameScene::Instance().HandleKeyDown(key_code);
        return;
    }
    if (input_callback_) {
        InputEvent event;
        event.source = InputSource::SDL;
        event.type = InputEvent::Type::Key;
        event.data = KeyEvent{key_code, false, false, false};
        input_callback_(event);
    }
}

void VirtualSprite::HandleMouseButtonDown(int x, int y) {
    if (input_callback_) {
        InputEvent event;
        event.source = InputSource::SDL;
        event.type = InputEvent::Type::Mouse;
        event.data = MouseEvent{MouseEvent::Click, x, y, 0};
        input_callback_(event);
    }
}

void VirtualSprite::SetInputCallback(InputCallback callback) {
    input_callback_ = callback;
}

void VirtualSprite::Initialize() {
    LOG_INFO("Initializing SDL application...");

    MediaCore::Instance().MediaInit(2500, 1400);
    MediaCore::Instance().SetFPS(60);

    AgentStateVisualizer::GetInstance().Initialize();
    GalgameScene::Instance().Initialize();
    UIRenderer::Instance().Initialize();
    HomeScreen::GetInstance().Initialize();

    // Status bar state color palette
    constexpr StateColor kStatusIdle{100, 100, 100, 255};
    constexpr StateColor kStatusThinking{65, 105, 225, 255};
    constexpr StateColor kStatusExecuting{255, 165, 0, 255};
    constexpr StateColor kStatusWaiting{255, 255, 0, 255};
    constexpr StateColor kStatusError{255, 0, 0, 255};
    constexpr StateColor kStatusComplete{0, 255, 0, 255};
    constexpr StateColor kStatusDefault{128, 128, 128, 255};

    UIRenderer::Instance().SetStatePropsGetter([](prosophor::AgentRuntimeState state) -> StateVisualProps {
        switch (state) {
            case prosophor::AgentRuntimeState::IDLE:
                return MakeVisualProps(kStatusIdle, "Idle");
            case prosophor::AgentRuntimeState::BEGINNING:
                return MakeVisualProps(kStatusThinking, "Thinking");
            case prosophor::AgentRuntimeState::EXECUTING_TOOL:
                return MakeVisualProps(kStatusExecuting, "Executing");
            case prosophor::AgentRuntimeState::WAITING_PERMISSION:
                return MakeVisualProps(kStatusWaiting, "Waiting");
            case prosophor::AgentRuntimeState::STATE_ERROR:
                return MakeVisualProps(kStatusError, "Error");
            case prosophor::AgentRuntimeState::COMPLETE:
                return MakeVisualProps(kStatusComplete, "Complete");
            default:
                return MakeVisualProps(kStatusDefault, "Idle");
        }
    });

    // Provide session data for UIRenderer rendering
    UIRenderer::Instance().SetSnapshotGetter([]() {
        return AgentEngine::GetInstance().GetFocusedSessionSnapshot();
    });

    // Route user input to the last active session
    UIRenderer::Instance().SetOnMessageSubmit([](const std::string& message) {
        auto snap = AgentEngine::GetInstance().GetFocusedSessionSnapshot();
        if (snap) {
            AgentEngine::GetInstance().SendUserMessage(snap->session_id, message);
        }
    });

    HomeScreen::GetInstance().SetOnModeSelect([this](UIMode mode) {
        SwitchMode(mode);
    });

    MediaCore::Instance().RegEventHandler([this](std::vector<EventType>& event_list) {
        for (const auto& event : event_list) {
            switch (event) {
                case EventType::ESCAPE:
                    if (current_scene_ != UIMode::HOME) {
                        SwitchMode(UIMode::HOME);
                    } else {
                        HandleKeyDown(0);
                    }
                    break;
                case EventType::ENTER:
                case EventType::KP_ENTER:
                    HandleKeyDown('\n');
                    break;
                case EventType::BACKSPACE:
                    HandleKeyDown('\b');
                    break;
                default:
                    break;
            }
        }
    });

    MediaCore::Instance().RegUpdateHandler([this]() {
        float dt = MediaCore::Instance().GetDeltaTimeS();
        AgentStateVisualizer::GetInstance().Update(dt);   // default (single-role) instance
        AgentStateVisualizer::UpdateAll(dt);              // all per-role instances
        GalgameScene::Instance().Update(dt);
    });

    MediaCore::Instance().RegRenderHandler([this]() {
        switch (current_scene_) {
            case UIMode::HOME:
                HomeScreen::GetInstance().Render();
                break;
            case UIMode::VIRTUAL_HUMAN:
                AgentStateVisualizer::GetInstance().Render();
                AgentStateVisualizer::RenderAll();
                UIRenderer::Instance().Render();
                UIRenderer::Instance().RenderImGui();
                break;
            case UIMode::GALGAME:
                GalgameScene::Instance().Render();
                UIRenderer::Instance().Render();
                UIRenderer::Instance().RenderImGui();
                break;
            case UIMode::TERMINAL:
                break;
        }
    });

    LOG_INFO("SDL application initialized successfully.");
}

void VirtualSprite::Shutdown() {
    LOG_INFO("Shutting down SDL application...");
    MediaCore::Instance().ImGuiShutdown();
    LOG_INFO("SDL application shutdown complete.");
}

int VirtualSprite::Run() {
    try {
        Initialize();
        MediaCore::Instance().MainRun();
        return 0;
    } catch (const std::exception& e) {
        LOG_ERROR("SDL app fatal error: {}", e.what());
        return 1;
    }
}

void VirtualSprite::Stop() {
    MediaCore::Instance().Quit();
}

void VirtualSprite::SwitchMode(UIMode mode) {
    if (current_scene_ == mode) return;

    LOG_INFO("Switching mode: {} -> {}", static_cast<int>(current_scene_), static_cast<int>(mode));
    current_scene_ = mode;

    switch (mode) {
        case UIMode::HOME:
            input_callback_ = saved_callback_;
            break;

        case UIMode::VIRTUAL_HUMAN: {
            saved_callback_ = input_callback_;
            UIRenderer::Instance().SetVisible(true);
            AgentStateVisualizer::GetInstance().SetVisible(true);
            break;
        }

        case UIMode::GALGAME: {
            saved_callback_ = input_callback_;
            break;
        }

        case UIMode::TERMINAL:
            saved_callback_ = input_callback_;
            break;
    }
}

}  // namespace prosophor
