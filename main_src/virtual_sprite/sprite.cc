// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/sprite.h"
#include "virtual_sprite/sprite_manager.h"
#include "platform/platform.h"
#include "scene/ui_renderer.h"
#include "scene/layout_config.h"
#include "scene/asset_define.h"
#include "components/speech_bubble.h"
#include "media_engine/media_engine.h"
#include "agent_engine.h"
#include "common/log_wrapper.h"
#include "common/file_utils.h"
#include "common/time_wrapper.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>

namespace prosophor {

Sprite::Sprite(const std::string& name, int width, int height)
    : name_(name), width_(width), height_(height) {}

Sprite::~Sprite() = default;

bool Sprite::Create() {
    auto& mc = media_engine::MediaCore::Instance();

    // ── Create sprite window (always-on-top, never occluded) ──
    media_engine::WindowConfig cfg;
    cfg.transparent_bg = true;
    cfg.load_chinese_font = true;
    cfg.borderless = true;
    cfg.transparent_window = true;
    cfg.skip_taskbar = true;
    cfg.always_on_top = true;
    sprite_window_ = mc.CreateMediaWindow(name_.c_str(), width_, height_, cfg);
    if (!sprite_window_) {
        LOG_ERROR("[Sprite] Failed to create window '{}'", name_);
        return false;
    }

    // ── Widget tree root ──
    root_widget_.SetBackgroundColor(media_engine::Colors::Transparent);
    root_widget_.SetRoot(static_cast<float>(width_), static_cast<float>(height_));
    root_widget_.AddChild(&name_anchor_);
    root_widget_.AddChild(&nav_anchor_);
    name_anchor_.SetPosition(36.0f, 2.0f, 28.0f, 5.0f);
    nav_anchor_.SetPosition(0.0f, 85.0f, 100.0f, 15.0f);

    // ── Per-sprite session ──
    auto& engine = AgentEngine::GetInstance();
    session_id_ = engine.CreateSession(engine.GetConfig().default_role, "");
    LOG_INFO("[Sprite] Session '{}' created for '{}'", session_id_, name_);

    // ── Pet loading (textures tied to this sprite window's renderer) ──
    LoadBackground();
    LoadPetList();
    if (!pet_list_.empty()) {
        std::mt19937 rng{std::random_device{}()};
        current_pet_index_ = std::uniform_int_distribution<int>(0, static_cast<int>(pet_list_.size()) - 1)(rng);
        LoadCurrentPet();
    }

    // ── Nav bar ──
    nav_bar_ = std::make_unique<media_engine::NavBar>();
    nav_bar_->SetTextColor(media_engine::Colors::Gray35);

    // ── Per-sprite SpeechBubble (云朵桩) ──
    speech_bubble_ = std::make_unique<SpeechBubble>();
    speech_bubble_->SetOnSubmit([this](const std::string& msg) {
        AgentEngine::GetInstance().SendUserMessage(session_id_, msg);
    });

    // ── Mouse handler: drag + double-click (+ nav popup) ──
    mc.RegMouseHandler(sprite_window_, [this](const media_engine::MouseEvent& me) {
        switch (me.type) {
            case media_engine::MouseEventType::DOWN:
                DispatchClickAction(me);
                break;
            case media_engine::MouseEventType::MOTION:
                if (dragging_) {
                    ConstrainSpriteOnScreen(me);
                } else {
                    UpdateHoverState(me);
                }
                break;
            case media_engine::MouseEventType::UP:
                EndDrag();
                break;
        }
    });

    // ── Render handler: pet + speech bubble + context menu ──
    mc.RegRenderHandler(sprite_window_, [this]() {
        root_widget_.Render(media_engine::RenderContext{});

        // Per-sprite cloud bubble (overlay, not in widget tree)
        auto snap = AgentEngine::GetInstance().GetSessionSnapshot(session_id_);
        speech_bubble_->Render(snap ? *snap : RenderSnapshot{},
                               sprite_window_->GetWidth(), sprite_window_->GetHeight());

        // Global context menu (singleton)
        UIRenderer::Instance().RenderContextMenu();
    });

    LOG_INFO("[Sprite] Window '{}' created ({}x{})", name_, width_, height_);
    return true;
}

// ── PetCanvas: root widget drawing ───────────────────────────────────────

void Sprite::PetCanvas::Render(const media_engine::RenderContext& /*ctx*/) {
    if (!visible_) return;

    auto& s = owner_;
    float win_w = width_;
    float win_h = height_;

    // 1. Pet sprite (centered at ground ratio)
    if (s.pet_sprite_ && s.pet_sprite_->IsValid()) {
        auto pet_cfg = LayoutConfig{};
        float base_x = win_w / 2.0f;
        float base_y = win_h * pet_cfg.pet_ground_ratio;
        float breathe_y = std::sin(s.animation_time_ * 3.14f) * 4.0f;

        auto action = s.GetEffectiveAction();
        int frame_count = s.pet_sprite_->GetFrameCount(action);
        float fps = 6.0f;
        if (action == SpritesheetAction::RUN_LEFT || action == SpritesheetAction::RUN_RIGHT) {
            fps = 12.0f;
        } else if (action != SpritesheetAction::IDLE) {
            fps = 10.0f;
        }
        int frame = static_cast<int>(s.animation_time_ * fps) % frame_count;

        float sx = base_x - pet_cfg.pet_sprite_size / 2.0f;
        float sy = base_y - pet_cfg.pet_sprite_size / 2.0f + breathe_y;

        s.pet_sprite_->RenderFrame(action, frame, sx, sy,
                                    pet_cfg.pet_sprite_size, pet_cfg.pet_sprite_size);
        s.sprite_bounds_ = {sx, sy, pet_cfg.pet_sprite_size, pet_cfg.pet_sprite_size};
    }

    // 3. Name label at anchor position
    media_engine::MediaUtil::DrawTextRect(s.name_,
                                          s.name_anchor_.GetX(), s.name_anchor_.GetY(),
                                          s.name_anchor_.GetWidth(), s.name_anchor_.GetHeight(),
                                          media_engine::Colors::White,
                                          platform::kDefaultFontPath);

    // 4. Nav bar at anchor position
    if (!s.pet_list_.empty()) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d/%d",
                      s.current_pet_index_ + 1, s.GetPetCount());
        std::vector<media_engine::NavBar::Item> items = {};
        // NavBar needs the parent width for ImGui layout; use nav_anchor_.GetWidth()
        // but the nav bar internally uses the window width (parent_width param).
        // The actual window width from the root is more accurate here.
        s.nav_bar_->Render(win_w, buf, items);
    }
}

// ── Mouse event handlers ─────────────────────────────────────────────

void Sprite::DispatchClickAction(const media_engine::MouseEvent& me) {
    // Right-click → context menu
    if (me.button == media_engine::MouseButton::RIGHT) {
        auto& ui = UIRenderer::Instance();
        ui.IsContextMenuVisible() ? ui.HideContextMenu() : ui.ShowContextMenu();
        return;
    }
    // Only left-click from here
    if (me.button != media_engine::MouseButton::LEFT) {
        return;
    }

    float fx = static_cast<float>(me.x);
    float fy = static_cast<float>(me.y);
    if (!sprite_bounds_.Contains(fx, fy) || fy >= static_cast<float>(height_) - 20.0f) {
        return;
    }

    // Double-click → toggle speech bubble
    if (first_click_at_ && SteadyClock::ElapsedMillis(*first_click_at_) < 500) {
        first_click_at_.reset();
        speech_bubble_->Toggle();
        SpriteManager::GetInstance().SetFocusedSession(session_id_);
        return;
    }
    // Single-click — record for potential double-click
    first_click_at_ = SteadyClock::Now();

    // Start drag
    dragging_ = true;
    hover_override_active_ = false;
    auto& mc = media_engine::MediaCore::Instance();
    int wx, wy;
    float gx, gy;
    sprite_window_->GetPosition(&wx, &wy);
    mc.GetGlobalMousePosition(&gx, &gy);
    drag_off_x_ = wx - static_cast<int>(gx);
    drag_off_y_ = wy - static_cast<int>(gy);
}

void Sprite::ConstrainSpriteOnScreen(const media_engine::MouseEvent& /*me*/) {
    auto& mc = media_engine::MediaCore::Instance();
    int pre_drag_x;
    sprite_window_->GetPosition(&pre_drag_x, nullptr);
    float gx, gy;
    mc.GetGlobalMousePosition(&gx, &gy);
    int target_x = static_cast<int>(gx) + drag_off_x_;
    int target_y = static_cast<int>(gy) + drag_off_y_;

    int disp_x, disp_y, disp_w, disp_h;
    if (mc.GetDisplayBoundsForWindow(sprite_window_, &disp_x, &disp_y, &disp_w, &disp_h)) {
        int win_w = sprite_window_->GetWidth();
        int win_h = sprite_window_->GetHeight();
        float sprite_size = LayoutConfig{}.pet_sprite_size;
        int half = static_cast<int>(sprite_size / 2);
        int char_screen_x = target_x + static_cast<int>(win_w / 2.0f - sprite_size / 2.0f);
        int char_screen_y = target_y + static_cast<int>(win_h * LayoutConfig{}.pet_ground_ratio - sprite_size / 2.0f);
        int valid_char_x = std::clamp(char_screen_x, disp_x - half, disp_x + disp_w - half);
        int valid_char_y = std::clamp(char_screen_y, disp_y - half, disp_y + disp_h - half);
        target_x += (valid_char_x - char_screen_x);
        target_y += (valid_char_y - char_screen_y);
    }
    sprite_window_->SetPosition(target_x, target_y);

    if (target_x != pre_drag_x) {
        SetDragOverride(true, target_x < pre_drag_x);
        last_motion_time_ = SteadyClock::Now();
    }
}

void Sprite::UpdateHoverState(const media_engine::MouseEvent& me) {
    float fx = static_cast<float>(me.x);
    float fy = static_cast<float>(me.y);
    SetHovering(sprite_bounds_.Contains(fx, fy));
}

void Sprite::EndDrag() {
    dragging_ = false;
    SetDragOverride(false, false);
}

// ── Background ────────────────────────────────────────────────────────

void Sprite::LoadBackground() {
    std::string bg_path = BackwallDir() + "solitude.jpg";
    if (std::filesystem::exists(bg_path)) {
        bg_texture_ = std::make_unique<media_engine::Texture>(*sprite_window_, bg_path);
        LOG_INFO("Loaded background: {}", bg_path);
    } else {
        LOG_WARN("Background not found: {}", bg_path);
    }
}

// ── Pet list ─────────────────────────────────────────────────────────

void Sprite::LoadPetList() {
    std::string path = PetdexDir() + "pet_list.json";
    auto content = ReadFile(path);
    if (!content) {
        LOG_WARN("Petdex pet_list.json not found: {}", path);
        return;
    }

    try {
        auto j = nlohmann::json::parse(*content);
        for (const auto& item : j) {
            PetEntry entry;
            entry.slug = item.value("slug", "");
            entry.name = item.value("name", entry.slug);
            if (!entry.slug.empty()) {
                pet_list_.push_back(std::move(entry));
            }
        }
        LOG_INFO("Petdex: loaded {} pets", pet_list_.size());
    } catch (const std::exception& e) {
        LOG_ERROR("Petdex pet_list.json parse error: {}", e.what());
    }
}

void Sprite::LoadCurrentPet() {
    if (pet_list_.empty()) return;
    const auto& entry = pet_list_[current_pet_index_];
    pet_sprite_ = std::make_unique<Spritesheet>(*sprite_window_, entry.slug, PetdexSpritesDir());
    if (!pet_sprite_->IsValid()) {
        pet_sprite_.reset();
    }
}

void Sprite::NextPet() {
    if (pet_list_.empty()) return;
    current_pet_index_ = (current_pet_index_ + 1) % pet_list_.size();
    LoadCurrentPet();
}

void Sprite::PrevPet() {
    if (pet_list_.empty()) return;
    current_pet_index_ = (current_pet_index_ - 1 + static_cast<int>(pet_list_.size())) % pet_list_.size();
    LoadCurrentPet();
}

// ── State → Action ────────────────────────────────────────────────────

SpritesheetAction Sprite::StateToAction(AgentRuntimeState state) const {
    switch (state) {
        case AgentRuntimeState::IDLE:
            return SpritesheetAction::IDLE;
        case AgentRuntimeState::BEGINNING:
        case AgentRuntimeState::STREAM_THINKING_START:
        case AgentRuntimeState::STREAM_THINKING:
        case AgentRuntimeState::STREAM_THINKING_END:
            return SpritesheetAction::REVIEW;
        case AgentRuntimeState::EXECUTING_TOOL:
        case AgentRuntimeState::TOOL_USE:
            return SpritesheetAction::SPRINT;
        case AgentRuntimeState::WAITING_PERMISSION:
            return SpritesheetAction::WAIT;
        case AgentRuntimeState::STATE_ERROR:
            return SpritesheetAction::FAILED;
        case AgentRuntimeState::COMPLETE:
        case AgentRuntimeState::STREAM_MODE_COMPLETE:
            return SpritesheetAction::IDLE;
        case AgentRuntimeState::STREAM_CONTENT_TYPING:
        case AgentRuntimeState::STREAM_CONTENT_START:
        case AgentRuntimeState::STREAM_CONTENT_END:
            return SpritesheetAction::REVIEW;
    }
    return SpritesheetAction::IDLE;
}

void Sprite::UpdateAnimation(float delta_time) {
    animation_time_ += delta_time;

    // ── Auto-wander when IDLE-like, not dragging, not hovering ──
    if ((agent_state_ == AgentRuntimeState::IDLE ||
         agent_state_ == AgentRuntimeState::COMPLETE ||
         agent_state_ == AgentRuntimeState::STREAM_MODE_COMPLETE) &&
        !dragging_ && !hover_override_active_) {
        if (wandering_) {
            int wx, wy;
            sprite_window_->GetPosition(&wx, &wy);
            float speed = 80.0f;
            int dx = static_cast<int>(wander_left_ ? -speed * delta_time : speed * delta_time);
            int new_wander_x = wx + dx;
            {
                auto& mcref = media_engine::MediaCore::Instance();
                int disp_x, disp_w;
                if (mcref.GetDisplayBoundsForWindow(sprite_window_, &disp_x, nullptr, &disp_w, nullptr)) {
                    int win_w = sprite_window_->GetWidth();
                    float sprite_size = LayoutConfig{}.pet_sprite_size;
                    int half = static_cast<int>(sprite_size / 2);
                    int char_screen_x = new_wander_x + static_cast<int>(win_w / 2.0f - sprite_size / 2.0f);
                    int valid_char_x = std::clamp(char_screen_x, disp_x - half, disp_x + disp_w - half);
                    new_wander_x += (valid_char_x - char_screen_x);
                }
            }
            sprite_window_->SetPosition(new_wander_x, wy);
            wander_dist_ += std::abs(static_cast<float>(dx));
            if (wander_dist_ >= wander_max_dist_) {
                wandering_ = false;
                SetDragOverride(false, false);
                wander_timer_ = SteadyClock::Now();
            }
        } else {
            if (SteadyClock::ElapsedMillis(wander_timer_) > 3000) {
                wander_left_ = std::rand() % 2 == 0;
                wander_dist_ = 0.0f;
                wander_max_dist_ = 80.0f + static_cast<float>(std::rand() % 200);
                wandering_ = true;
                SetDragOverride(true, wander_left_);
            }
        }
    } else {
        if (wandering_) {
            wandering_ = false;
            SetDragOverride(false, false);
        }
        wander_timer_ = SteadyClock::Now();
    }
}

SpritesheetAction Sprite::GetEffectiveAction() const {
    if (drag_override_active_) {
        return drag_override_left_ ? SpritesheetAction::RUN_LEFT : SpritesheetAction::RUN_RIGHT;
    }
    if (hover_override_active_) {
        return SpritesheetAction::WAVE;
    }
    return StateToAction(agent_state_);
}

// ── State / override setters ──────────────────────────────────────────

void Sprite::SetAgentState(AgentRuntimeState state, const std::string& details) {
    agent_state_ = state;
    state_details_ = details;
}

void Sprite::SetDragOverride(bool active, bool left) {
    drag_override_active_ = active;
    drag_override_left_ = left;
}

void Sprite::SetHovering(bool active) {
    hover_override_active_ = active;
}

}  // namespace prosophor
