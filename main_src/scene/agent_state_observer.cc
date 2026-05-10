// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "scene/agent_state_observer.h"
#include "scene/anime_character.h"
#include "scene/ui_renderer.h"
#include "scene/layout_config.h"
#include "scene/asset_define.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "media_engine/media_engine.h"
#include "common/log_wrapper.h"

namespace prosophor {

// ============================================================================
// AgentStateNotifier Implementation
// ============================================================================

AgentStateNotifier& AgentStateNotifier::GetInstance() {
    static AgentStateNotifier instance;
    return instance;
}

void AgentStateNotifier::AddObserver(std::weak_ptr<AgentStateObserver> observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.push_back(observer);
}

void AgentStateNotifier::RemoveObserver(std::weak_ptr<AgentStateObserver> observer) {
    std::lock_guard<std::mutex> lock(mutex_);
    observers_.erase(
        std::remove_if(observers_.begin(), observers_.end(),
            [&observer](const std::weak_ptr<AgentStateObserver>& wp) {
                return observer.owner_before(wp) && wp.owner_before(observer);
            }),
        observers_.end());
}

void AgentStateNotifier::NotifyStateChanged(const std::string& session_id,
                                             const std::string& role_id,
                                             AgentRuntimeState new_state,
                                             const std::string& details) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_state_ = new_state;

    for (auto& weak_observer : observers_) {
        if (auto observer = weak_observer.lock()) {
            observer->OnAgentStateChanged(session_id, role_id, new_state, details);
        }
    }
}

AgentRuntimeState AgentStateNotifier::GetCurrentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_state_;
}

// ============================================================================
// AgentStateVisualizer Implementation
// ============================================================================

namespace {
std::mutex         g_registry_mutex;
std::unordered_map<std::string, std::unique_ptr<AgentStateVisualizer>> g_registry;
}  // namespace

AgentStateVisualizer& AgentStateVisualizer::GetInstance() {
    static AgentStateVisualizer instance;
    return instance;
}

AgentStateVisualizer& AgentStateVisualizer::GetOrCreate(const std::string& role_id) {
    if (role_id.empty()) return GetInstance();
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    auto it = g_registry.find(role_id);
    if (it == g_registry.end()) {
        auto viz = std::unique_ptr<AgentStateVisualizer>(new AgentStateVisualizer());
        viz->Initialize();
        viz->SetCharacterType(AnimeCharacterTypeFromRoleId(role_id));
        it = g_registry.emplace(role_id, std::move(viz)).first;
    }
    return *it->second;
}

void AgentStateVisualizer::UpdateAll(float delta_time) {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    for (auto& [id, viz] : g_registry) {
        viz->Update(delta_time);
    }
}

void AgentStateVisualizer::RenderAll() {
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    for (auto& [id, viz] : g_registry) {
        viz->Render();
    }
}

void AgentStateVisualizer::Initialize() {
    LoadBackground();
    LOG_INFO("AgentStateVisualizer initialized.");
}

void AgentStateVisualizer::LoadBackground() {
    std::string bg_path = BackwallDir() + "solitude.jpg";
    if (std::filesystem::exists(bg_path)) {
        bg_texture_ = std::make_unique<Texture>(bg_path);
        LOG_INFO("Loaded background: {}", bg_path);
    } else {
        LOG_WARN("Background not found: {}", bg_path);
    }
}

void AgentStateVisualizer::Update(float delta_time) {
    animation_time_ += delta_time;
}

void AgentStateVisualizer::Render() {
    if (!visible_) return;

    DrawBlackboard();
    DrawVirtualHumanCharacter();
}

// ── 状态→视觉属性映射 ──────────────────────────────────────────────
// Character scarf / aura color palette
namespace {
    constexpr StateColor kCharIdle{128, 128, 128, 255};
    constexpr StateColor kCharThinking{255, 200, 50, 255};
    constexpr StateColor kCharToolUse{100, 200, 255, 255};
    constexpr StateColor kCharWaiting{255, 160, 50, 255};
    constexpr StateColor kCharError{255, 60, 60, 255};
    constexpr StateColor kCharDone{80, 220, 120, 255};
    constexpr StateColor kCharStreaming{50, 180, 255, 255};
    constexpr StateColor kCharDeepThinking{180, 140, 255, 255};
    constexpr StateColor kCharUnknown{128, 128, 128, 255};

StateVisualProps GetStateVisualProps(AgentRuntimeState state) {
    switch (state) {
        case AgentRuntimeState::IDLE:
            return MakeVisualProps(kCharIdle, "idle");
        case AgentRuntimeState::BEGINNING:
        case AgentRuntimeState::EXECUTING_TOOL:
            return MakeVisualProps(kCharThinking, "thinking");
        case AgentRuntimeState::TOOL_USE:
            return MakeVisualProps(kCharToolUse, "using tool");
        case AgentRuntimeState::WAITING_PERMISSION:
            return MakeVisualProps(kCharWaiting, "waiting");
        case AgentRuntimeState::STATE_ERROR:
            return MakeVisualProps(kCharError, "error");
        case AgentRuntimeState::COMPLETE:
            return MakeVisualProps(kCharDone, "done");
        case AgentRuntimeState::STREAM_CONTENT_TYPING:
        case AgentRuntimeState::STREAM_CONTENT_START:
        case AgentRuntimeState::STREAM_CONTENT_END:
            return MakeVisualProps(kCharStreaming, "streaming");
        case AgentRuntimeState::STREAM_THINKING_START:
        case AgentRuntimeState::STREAM_THINKING:
        case AgentRuntimeState::STREAM_THINKING_END:
            return MakeVisualProps(kCharDeepThinking, "deep thinking");
        case AgentRuntimeState::STREAM_MODE_COMPLETE:
            return MakeVisualProps(kCharDone, "done");
    }
    return MakeVisualProps(kCharUnknown, "unknown");
}
}

void AgentStateVisualizer::DrawBlackboard() {
    int win_w = MediaCore::Instance().GetWindowWidth();
    int win_h = MediaCore::Instance().GetWindowHeight();

    if (bg_texture_ && bg_texture_->GetOriginWidth() > 0) {
        float tw = bg_texture_->GetOriginWidth();
        float th = bg_texture_->GetOriginHeight();

        if (tw >= win_w && th >= win_h) {
            // 壁纸比窗口大或相等：直接拉伸铺满
            bg_texture_->RenderTexture(0.0f, 0.0f, static_cast<float>(win_w), static_cast<float>(win_h));
        } else {
            // 壁纸比窗口小：平铺
            for (float y = 0; y < win_h; y += th) {
                for (float x = 0; x < win_w; x += tw) {
                    bg_texture_->RenderTexture(0.0f, 0.0f, tw, th, x, y, tw, th, false, false);
                }
            }
        }
    } else {
        // Fallback: solid dark background
        ::Drawer::Instance().DrawFillRect(0, 0, static_cast<float>(win_w), static_cast<float>(win_h),
                                           Color(20, 20, 35, 255));
    }
}

void AgentStateVisualizer::DrawVirtualHumanCharacter() {
    int win_w = MediaCore::Instance().GetWindowWidth();
    int win_h = MediaCore::Instance().GetWindowHeight();

    // 角色位置：左侧 65% 区域居中
    float board_w = win_w * 0.63f;
    float board_h = win_h * 0.94f;
    float board_x = board_w * 0.01f;
    float board_y = board_h * 0.03f;

    float base_x = board_x + board_w / 2.0f;
    float base_y = board_y + board_h * 0.72f;  // 脚部位置

    // 呼吸动画
    float breathe_y = std::sin(animation_time_ * 3.14f) * 6.0f;  // ±6px
    float breathe_s = 1.0f + std::sin(animation_time_ * 3.14f) * 0.01f;  // ±1%

    // 眨眼动画：周期 4s，闭合 0.12s
    float blink_period = 4.0f;
    float blink_close = 0.12f;
    float blink_phase = std::fmod(animation_time_, blink_period);
    bool is_blinking = (blink_phase > blink_period - blink_close);

    // 状态颜色
    auto props = GetStateVisualProps(agent_state_);
    Color scarf_color(props.r, props.g, props.b);

    // 脉冲效果
    float pulse_alpha = 1.0f;
    if (agent_state_ == AgentRuntimeState::BEGINNING ||
        agent_state_ == AgentRuntimeState::EXECUTING_TOOL) {
        pulse_alpha = 0.88f + 0.12f * std::sin(animation_time_ * 8.0f);
    }

    float scale = 7.5f * breathe_s;

    AnimeCharacterRenderer::Instance().Render(
        character_type_, base_x, base_y + breathe_y, agent_state_, scarf_color, scale, pulse_alpha, is_blinking);

    // 状态名称文本
    float name_y = base_y + 30.0f * scale;
    UIRenderer::Instance().RenderFloatingText(
        props.name, base_x - 30, name_y, 204, 204, 204, pulse_alpha);

    // 详情文本
    if (!state_details_.empty()) {
        float details_y = name_y + 18.0f;
        UIRenderer::Instance().RenderFloatingText(
            state_details_.substr(0, 25), base_x - 40, details_y,
            153, 153, 153, pulse_alpha * 0.8f);
    }
}

void AgentStateVisualizer::SetAgentState(AgentRuntimeState state, const std::string& details) {
    agent_state_ = state;
    state_details_ = details;
}

void AgentStateVisualizer::SetVisible(bool visible) {
    visible_ = visible;
}

void AgentStateVisualizer::SetCharacterType(AnimeCharacterType type) {
    character_type_ = type;
}

}  // namespace prosophor
