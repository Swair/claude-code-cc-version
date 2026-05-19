// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/noncopyable.h"
#include <string>
#include <functional>
#include <memory>
#include <vector>

namespace media_engine { class Window; }

namespace prosophor {

class Sprite;

/// SpriteManager: owns all Sprite instances, handles creation and lookup.
class SpriteManager : public Noncopyable {
public:
    static SpriteManager& GetInstance();

    SpriteManager();
    ~SpriteManager();

    /// Create a new sprite window. Returns nullptr on failure.
    Sprite* CreateSprite(const std::string& name, int width, int height,
                         const std::string& role_id = "");

    /// Update all sprite animations.
    void UpdateAll(float dt);

    /// Find sprite by session ID (for routing agent state updates).
    Sprite* FindBySessionId(const std::string& session_id);

    /// Find sprite by window pointer (for context menu routing).
    Sprite* FindByWindow(media_engine::Window* win);

    /// Remove all sprites.
    void Clear();

    /// Callback for "+New" button (wired by VirtualSprite).
    using NewSpriteCallback = std::function<void()>;
    void SetOnNewSprite(NewSpriteCallback cb) { on_new_sprite_ = std::move(cb); }
    bool HasNewSprite() const { return !!on_new_sprite_; }
    void TriggerNewSprite() { if (on_new_sprite_) on_new_sprite_(); }

    /// Focused session tracking (for MainWindow cross-sprite chat display).
    void SetFocusedSession(const std::string& sid) { focused_session_ = sid; }
    std::string GetFocusedSession() const { return focused_session_; }

    /// Get the display name of the currently focused sprite.
    std::string GetFocusedSpriteName() const;

    /// Direct access to all sprites (for iteration in callbacks).
    std::vector<std::unique_ptr<Sprite>>& GetAll() { return sprites_; }
    size_t Count() const { return sprites_.size(); }

private:
    NewSpriteCallback on_new_sprite_;
    std::string focused_session_;
    std::vector<std::unique_ptr<Sprite>> sprites_;
};

}  // namespace prosophor
