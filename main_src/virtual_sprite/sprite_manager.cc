// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/sprite_manager.h"
#include "virtual_sprite/virtual_sprite.h"
#include "virtual_sprite/sprite.h"
#include "virtual_sprite/layout_config.h"
#include "media_engine/media_engine.h"
#include "common/log_wrapper.h"

#include <random>

namespace prosophor {

SpriteManager& SpriteManager::GetInstance() {
    static SpriteManager instance;
    return instance;
}

SpriteManager::SpriteManager() = default;
SpriteManager::~SpriteManager() = default;

Sprite* SpriteManager::CreateSprite(const std::string& name, int width, int height,
                                     const std::string& role_id) {
    auto sprite = std::make_unique<Sprite>(name, width, height, role_id);
    if (!sprite->Create()) {
        return nullptr;
    }

    // Randomly place each sprite within its display bounds
    if (auto* win = sprite->GetWindow()) {
        int disp_x = 0, disp_y = 0, disp_w = 0, disp_h = 0;
        auto& mc = media_engine::MediaCore::Instance();
        if (mc.GetDisplayBoundsForWindow(win, &disp_x, &disp_y, &disp_w, &disp_h)) {
            static std::mt19937 rng{std::random_device{}()};
            // Margins to keep sprite fully visible (280x380 default)
            int margin_x = 40, margin_y = 40;
            int range_w = std::max(1, disp_w - width - margin_x * 2);
            int range_h = std::max(1, disp_h - height - margin_y * 2);
            std::uniform_int_distribution<int> dist_x(0, range_w);
            std::uniform_int_distribution<int> dist_y(0, range_h);
            win->SetPosition(disp_x + margin_x + dist_x(rng),
                             disp_y + margin_y + dist_y(rng));
        }
    }

    auto* ptr = sprite.get();
    sprites_.push_back(std::move(sprite));
    LOG_INFO("SpriteManager: sprite '{}' created (total={})", name, sprites_.size());
    return ptr;
}

void SpriteManager::UpdateAll(float dt) {
    ProcessPendingOps();
    for (auto& s : sprites_) {
        s->UpdateAnimation(dt);
    }
}

Sprite* SpriteManager::FindBySessionId(const std::string& session_id) {
    for (auto& s : sprites_) {
        if (s->GetSessionId() == session_id) {
            return s.get();
        }
    }
    return nullptr;
}

Sprite* SpriteManager::FindByWindow(media_engine::Window* win) {
    for (auto& s : sprites_) {
        if (s->GetWindow() == win) {
            return s.get();
        }
    }
    return nullptr;
}

std::string SpriteManager::GetFocusedSpriteName() const {
    if (focused_session_.empty()) return {};
    for (const auto& s : sprites_) {
        if (s->GetSessionId() == focused_session_) {
            return s->GetName();
        }
    }
    return {};
}

bool SpriteManager::RemoveSpriteByRoleId(const std::string& role_id) {
    pending_remove_roles_.push_back(role_id);
    return true;
}

void SpriteManager::QueueCreateSprite(const std::string& role_id) {
    pending_create_roles_.push_back(role_id);
}

void SpriteManager::ProcessPendingOps() {
    for (const auto& role_id : pending_create_roles_) {
        try {
            auto& vs = VirtualSprite::GetInstance();
            auto* sp = CreateSprite(role_id, LayoutConfig{}.sprite_window_width,
                LayoutConfig{}.sprite_window_height, role_id);
            if (sp) {
                sp->SetOnToggleCentralWindow([&vs]() {
                    vs.GetCentralWindow().SetVisible(!vs.GetCentralWindow().IsVisible());
                });
            }
        } catch (const std::exception& e) {
            LOG_ERROR("SpriteManager: failed to create sprite for role '{}': {}", role_id, e.what());
        }
    }
    pending_create_roles_.clear();
    for (const auto& role_id : pending_remove_roles_) {
        for (auto it = sprites_.begin(); it != sprites_.end(); ++it) {
            if ((*it)->GetRoleId() == role_id) {
                if ((*it)->GetSessionId() == focused_session_)
                    focused_session_.clear();
                if (auto* win = (*it)->GetWindow())
                    media_engine::MediaCore::Instance().DestroyMediaWindow(win);
                sprites_.erase(it);
                LOG_INFO("SpriteManager: sprite for role '{}' removed (total={})", role_id, sprites_.size());
                break;
            }
        }
    }
    pending_remove_roles_.clear();
}

void SpriteManager::Clear() {
    sprites_.clear();
}

}  // namespace prosophor
