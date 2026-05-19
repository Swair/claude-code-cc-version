// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/noncopyable.h"
#include "common/time_wrapper.h"
#include "core/agent_types.h"
#include "components/spritesheet.h"
#include "media_engine/media/imgui_widget.h"
#include "media_engine/ui_component/widget.h"
#include "media_engine/ui_component/label.h"

#include <string>
#include <memory>
#include <optional>
#include <cstdint>
#include <functional>
#include <vector>


namespace prosophor {


class SpeechBubble;

/// Sprite: one sprite window (pet + cloud bubble + inline input + drag + animation).
/// Each Sprite has its own session, pet renderer, and UI components.
///
/// Layout hierarchy (Widget tree for coordinate management only):
///   root_widget_ (0,0,100,100) [full window, transparent]
///   ├── name_label_            [top-center percentage]
///   ├── nav_anchor_             [bottom strip percentage, renders via NavBar::Render]
///   └── [pet sprite drawn by PetCanvas at computed position]
///
/// All sizing now comes from LayoutConfig; window dimensions are set at creation
/// via the caller (which reads LayoutConfig::sprite_window_width/height).
class Sprite : public Noncopyable {
 public:
    Sprite(const std::string& name, int width, int height, const std::string& role_id = "");
    ~Sprite();

    bool Create();

    media_engine::Window* GetWindow() const { return sprite_window_; }
    const std::string& GetSessionId() const { return session_id_; }
    const std::string& GetName() const { return name_; }

    /// Agent state → pet animation
    void SetAgentState(AgentRuntimeState state, const std::string& details = "");
    void UpdateAnimation(float delta_time);

    /// Drag/hover overrides
    void SetDragOverride(bool active, bool left);
    void SetHovering(bool active);
    struct SpriteBounds {
        float x = 0, y = 0, width = 0, height = 0;
        bool Contains(float px, float py) const {
            return px >= x && px <= x + width && py >= y && py <= y + height;
        }
    };
    const SpriteBounds& GetSpriteBounds() const { return sprite_bounds_; }

    /// Pet navigation
    void NextPet();
    void PrevPet();
    int GetPetCount() const { return static_cast<int>(pet_list_.size()); }
    const std::string& GetCurrentPetSlug() const;
    const std::string& GetCurrentPetName() const;
    std::string GetSpritesheetPath() const;

    /// Callback fired on double-click (wired by VirtualSprite to toggle central window)
    using ToggleCentralCallback = std::function<void()>;
    void SetOnToggleCentralWindow(ToggleCentralCallback cb) { on_toggle_central_ = std::move(cb); }

    /// Toggle the per-sprite speech bubble (used by context menu)
    void ToggleSpeechBubble();

    // Widget tree root — exposed so render handler calls root_widget_.Render(ctx)
    media_engine::Widget& GetRootWidget() { return root_widget_; }

 private:
    // ── Pet loading ──
    struct PetEntry {
        std::string slug;
        std::string name;
    };
    struct SpriteBinding {
        std::string sprite_id;
        std::string assets_dir;    // 非空时优先，直接从该目录加载 sprite
        std::string spritesheet_file; // 相对 sprite_assets_dir 的纹理文件名
        std::string display_name;  // display name from role JSON
    };
    void LoadPetList();
    void LoadCurrentPet();
    void LoadPetBySpriteId(const std::string& sprite_id);
    void LoadPetFromDir(const std::string& assets_dir);
    void LoadBackground();
    SpriteBinding LoadSpriteBindingFromRole(const std::string& role_id);

    // ── Root widget: draws pet bg + pet sprite + name text + nav bar ──
    class PetCanvas : public media_engine::Widget {
    public:
        explicit PetCanvas(Sprite& owner) : owner_(owner) {}
        void Render(const media_engine::RenderContext& ctx) override;
    private:
        Sprite& owner_;
    };

    // ── Core state ──
    std::string name_;
    std::string role_id_;
    int width_ = 280;
    int height_ = 380;
    std::string session_id_;
    media_engine::Window* sprite_window_ = nullptr;

    // ── Widget tree ──
    PetCanvas root_widget_{*this};
    media_engine::Label name_label_;     // display sprite name at top center
    media_engine::Widget nav_anchor_;      // position ref for nav bar

    // ── Pet / animation ──
    std::vector<PetEntry> pet_list_;
    int current_pet_index_ = 0;
    std::unique_ptr<Spritesheet> pet_sprite_;
    std::unique_ptr<media_engine::Texture> bg_texture_;
    float animation_time_ = 0.0f;
    AgentRuntimeState agent_state_ = AgentRuntimeState::IDLE;
    std::string state_details_;
    SpriteBounds sprite_bounds_;

    // ── Nav bar ──
    std::unique_ptr<media_engine::NavBar> nav_bar_;

    // ── Rendering ──
    SpritesheetAction GetEffectiveAction() const;

    // ── Mouse event handlers ──
    void DispatchClickAction(const media_engine::MouseEvent& me);
    void ConstrainSpriteOnScreen(const media_engine::MouseEvent& me);
    void UpdateHoverState(const media_engine::MouseEvent& me);
    void EndDrag();

    // ── Drag state ──
    bool dragging_ = false;
    int drag_off_x_ = 0, drag_off_y_ = 0;
    std::optional<SteadyClock::TimePoint> first_click_at_;
    SteadyClock::TimePoint last_motion_time_;
    bool drag_override_active_ = false;
    bool drag_override_left_ = false;
    bool hover_override_active_ = false;

    // ── Auto-wander (IDLE时自动左右走) ──
    bool wandering_ = false;
    bool wander_left_ = false;
    float wander_dist_ = 0.0f;
    float wander_max_dist_ = 0.0f;
    SteadyClock::TimePoint wander_timer_;

    // ── Callbacks ──
    ToggleCentralCallback on_toggle_central_;

    // ── Per-sprite UI ──
    std::unique_ptr<SpeechBubble> speech_bubble_;
};

}  // namespace prosophor
