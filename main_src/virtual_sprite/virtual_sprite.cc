// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite.h"
#include "scene/agent_state_observer.h"
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
    UIRenderer::Instance().Initialize();

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

    // Route user input to the last active session (auto-create if none exists)
    UIRenderer::Instance().SetOnMessageSubmit([](const std::string& message) {
        auto& engine = AgentEngine::GetInstance();
        auto snap = engine.GetFocusedSessionSnapshot();
        std::string session_id;
        if (snap) {
            session_id = snap->session_id;
        } else {
            session_id = engine.CreateSession(engine.GetConfig().default_role, "");
        }
        engine.SendUserMessage(session_id, message);
    });

    MediaCore::Instance().RegEventHandler([this](std::vector<EventType>& event_list) {
        for (const auto& event : event_list) {
            switch (event) {
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
        AgentStateVisualizer::GetInstance().Update(dt);
        AgentStateVisualizer::UpdateAll(dt);
    });

    MediaCore::Instance().RegRenderHandler([this]() {
        AgentStateVisualizer::GetInstance().Render();
        AgentStateVisualizer::RenderAll();
        UIRenderer::Instance().Render();
        UIRenderer::Instance().RenderImGui();
    });

    // Ensure a default session exists so the UI shows a ready state
    auto& engine = AgentEngine::GetInstance();
    if (!engine.GetFocusedSessionSnapshot()) {
        engine.CreateSession(engine.GetConfig().default_role, "");
    }

    // Set character portrait type based on default role
    AgentStateVisualizer::GetInstance().SetCharacterType(
        AnimeCharacterTypeFromRoleId(engine.GetConfig().default_role));

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

}  // namespace prosophor
