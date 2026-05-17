// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/sprite_manager.h"
#include "virtual_sprite/sprite.h"
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

void SpriteManager::Clear() {
    sprites_.clear();
}

}  // namespace prosophor
