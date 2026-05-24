// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/virtual_sprite.h"
#include "virtual_sprite/sprite.h"
#include "virtual_sprite/ui_renderer.h"
#include "virtual_sprite/layout_config.h"
#include "media_engine/media_engine.h"
#include "common/log_wrapper.h"
#include "common/i18n.h"
#include "agent_engine.h"

#include "providers/tts/tts_speaker.h"
#include "media_engine/media/audio_streamer.h"
#include <memory>

namespace prosophor {

VirtualSprite& VirtualSprite::GetInstance() {
    static VirtualSprite instance;
    return instance;
}

VirtualSprite::VirtualSprite() = default;

VirtualSprite::~VirtualSprite() {
    if (!shutdown_) Shutdown();
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

void VirtualSprite::GlobalInit() {
    LOG_INFO("Initializing SDL application...");

    // Initialise i18n (load default translations)
    I18n::Instance().Init("zh-CN");

    auto& media = media_engine::MediaCore::Instance();
    media.MediaInit();
    media.SetFPS(60);

    // ── Create central chat window FIRST (becomes primary window) ──
    {
        LayoutConfig cfg;
        central_window_.Create(cfg.chat_window_width, cfg.chat_window_height);
        central_window_.SetVisible(true);

        central_window_.SetOnSubmit([](const std::string& msg) {
            auto& engine = AgentEngine::GetInstance();
            auto snap = engine.GetFocusedSessionSnapshot();
            std::string sid = snap ? snap->session_id
                                   : engine.CreateSession(
                                       engine.GetConfig().default_role.empty()
                                           ? "default"
                                           : engine.GetConfig().default_role[0], "");
            engine.SendUserMessage(sid, msg);
        });

        LOG_INFO("Central window created (primary)");
    }

    // ── Wire up TTS streaming audio playback ─────────────────
    {
        auto& tts = TtsSpeaker::GetInstance();
        tts.SetOnStreamStarted([this](int sample_rate, int channels) {
            std::lock_guard<std::mutex> lock(audio_mutex_);
            current_streamer_ = std::make_unique<media_engine::AudioStreamer>(sample_rate, channels);
        });
        tts.SetOnAudioChunk([this](const uint8_t* data, size_t len) {
            std::lock_guard<std::mutex> lock(audio_mutex_);
            if (current_streamer_) {
                current_streamer_->PushChunk(data, len);
            }
        });
    }

    // Route session state changes to the matching sprite
    AgentEngine::GetInstance().SetOutputCallback(
        [this](const std::string& session_id, const std::string& /*role_id*/,
               AgentRuntimeState state, const std::string& state_msg,
               const std::optional<MessageSchema>& reply) {
            if (auto* s = SpriteManager::GetInstance().FindBySessionId(session_id)) {
                s->SetAgentState(state, state_msg);
            }
            // Trigger TTS when a reply completes
            if (AgentEngine::GetInstance().GetConfig().tts.enabled &&
                (state == AgentRuntimeState::COMPLETE ||
                 state == AgentRuntimeState::STREAM_MODE_COMPLETE) &&
                reply && !reply->text().empty()) {
                TtsSpeaker::GetInstance().SpeakStream(reply->text());
            }
        });

    // Right-click context menu callbacks — toggle the requesting sprite's bubble
    UIRenderer::Instance().SetOnToggleChat([](media_engine::Window* win) {
        if (auto* s = SpriteManager::GetInstance().FindByWindow(win)) {
            s->ToggleSpeechBubble();
        }
    });
    UIRenderer::Instance().SetOnShowMainWindow([this]() {
        central_window_.SetVisible(true);
    });
    UIRenderer::Instance().SetOnOpenSettings([this]() {
        central_window_.OpenSettings();
        central_window_.SetVisible(true);
    });
    UIRenderer::Instance().SetOnNewSprite([]() {
        static int counter = 1;
        auto& vs = VirtualSprite::GetInstance();
        auto* s = SpriteManager::GetInstance().CreateSprite(
            "Assistant " + std::to_string(counter++),
            LayoutConfig{}.sprite_window_width, LayoutConfig{}.sprite_window_height);
        if (s) {
            s->SetOnToggleCentralWindow([&vs]() {
                vs.GetCentralWindow().SetVisible(!vs.GetCentralWindow().IsVisible());
            });
        }
    });

    // Route user input from central window to focused session is handled in
    // central_window_.SetOnSubmit above.

    // Global event handler (keyboard)
    media.RegEventHandler([this](std::vector<media_engine::EventType>& event_list) {
        for (const auto& event : event_list) {
            switch (event) {
                case media_engine::EventType::ENTER:
                case media_engine::EventType::KP_ENTER:
                    HandleKeyDown('\n');
                    break;
                case media_engine::EventType::BACKSPACE:
                    HandleKeyDown('\b');
                    break;
                case media_engine::EventType::ESCAPE:
                    if (central_window_.IsVisible()) {
                        central_window_.SetVisible(false);
                    }
                    break;
                default:
                    break;
            }
        }
    });

    // Helper: create sprite with role_id + wire toggle central window
    auto create_sprite = [this](const std::string& name, int w, int h,
                                 const std::string& role_id = "") -> Sprite* {
        auto* s = SpriteManager::GetInstance().CreateSprite(name, w, h, role_id);
        if (s) {
            s->SetOnToggleCentralWindow([this]() {
                central_window_.SetVisible(!central_window_.IsVisible());
            });
        }
        return s;
    };

    // Create sprites bound to roles (one per default_role entry)
    LayoutConfig sprite_cfg;
    auto& role_list = AgentEngine::GetInstance().GetConfig().default_role;
    for (const auto& role_id : role_list) {
        create_sprite(role_id, sprite_cfg.sprite_window_width, sprite_cfg.sprite_window_height, role_id);
    }

    // Global update: animate all sprites
    media.RegUpdateHandler([]() {
        float dt = media_engine::MediaCore::Instance().GetDeltaTimeS();
        SpriteManager::GetInstance().UpdateAll(dt);
    });

    // Wire "+New" nav button to create additional sprites at runtime
    SpriteManager::GetInstance().SetOnNewSprite([]() {
        static int counter = 1;
        auto& vs = VirtualSprite::GetInstance();
        auto* s = SpriteManager::GetInstance().CreateSprite(
            "Assistant " + std::to_string(counter++),
            LayoutConfig{}.sprite_window_width, LayoutConfig{}.sprite_window_height);
        if (s) {
            s->SetOnToggleCentralWindow([&vs]() {
                vs.GetCentralWindow().SetVisible(!vs.GetCentralWindow().IsVisible());
            });
        }
    });

    LOG_INFO("SDL application initialized successfully.");
}

void VirtualSprite::Shutdown() {
    LOG_INFO("Shutting down SDL application...");
    SpriteManager::GetInstance().Clear();
    media_engine::MediaCore::Instance().Shutdown();
    LOG_INFO("SDL application shutdown complete.");
    shutdown_ = true;
}

int VirtualSprite::Run() {
    try {
        GlobalInit();
        media_engine::MediaCore::Instance().MainRun();
    } catch (const std::exception& e) {
        LOG_ERROR("SDL app fatal error: {}", e.what());
        Shutdown();
        return 1;
    }
    Shutdown();
    return 0;
}

void VirtualSprite::Stop() {
    media_engine::MediaCore::Instance().Quit();
}

}  // namespace prosophor
