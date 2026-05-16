// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/sprite_manager.h"
#include "virtual_sprite/sprite.h"
#include "media_engine/media_engine.h"
#include "common/log_wrapper.h"

namespace prosophor {

SpriteManager& SpriteManager::GetInstance() {
    static SpriteManager instance;
    return instance;
}

SpriteManager::SpriteManager() = default;
SpriteManager::~SpriteManager() = default;

Sprite* SpriteManager::CreateSprite(const std::string& name, int width, int height) {
    auto sprite = std::make_unique<Sprite>(name, width, height);
    if (!sprite->Create()) {
        return nullptr;
    }

    // Place each sprite at bottom-right of its display, cascading inward
    int idx = static_cast<int>(sprites_.size());
    if (auto* win = sprite->GetWindow()) {
        int disp_x = 0, disp_y = 0, disp_w = 0, disp_h = 0;
        auto& mc = media_engine::MediaCore::Instance();
        if (mc.GetDisplayBoundsForWindow(win, &disp_x, &disp_y, &disp_w, &disp_h)) {
            win->SetPosition(disp_x + disp_w - width - idx * 40,
                             disp_y + disp_h - height - idx * 30);
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
