// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "common/noncopyable.h"
#include "core/agent_types.h"
#include "scene/anime_character.h"

#include <string>
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <vector>

namespace prosophor {

/// AgentStateObserver: Callback interface for agent state changes
class AgentStateObserver {
 public:
    virtual ~AgentStateObserver() = default;
    virtual void OnAgentStateChanged(const std::string& session_id,
                                     const std::string& role_id,
                                     AgentRuntimeState new_state,
                                     const std::string& details) = 0;
};

/// AgentStateNotifier: Singleton for broadcasting state changes
/// Thread-safe observer pattern for agent state updates
class AgentStateNotifier : public Noncopyable {
 public:
    static AgentStateNotifier& GetInstance();

    /// Register an observer
    void AddObserver(std::weak_ptr<AgentStateObserver> observer);

    /// Remove an observer
    void RemoveObserver(std::weak_ptr<AgentStateObserver> observer);

    /// Notify all observers of state change
    void NotifyStateChanged(const std::string& session_id,
                            const std::string& role_id,
                            AgentRuntimeState new_state,
                            const std::string& details = "");

    /// Get current state
    AgentRuntimeState GetCurrentState() const;

 private:
    AgentStateNotifier() = default;

    mutable std::mutex mutex_;
    std::vector<std::weak_ptr<AgentStateObserver>> observers_;
    AgentRuntimeState current_state_ = AgentRuntimeState::IDLE;
};

/// AgentStateVisualizer: Renders virtual human anime character + blackboard
/// Each role gets its own instance via GetOrCreate(role_id).
class AgentStateVisualizer : public Noncopyable {
 public:
    /// Default singleton (backward-compat for single-role modes)
    static AgentStateVisualizer& GetInstance();

    /// Per-role instance � creates on first call, returns existing on subsequent calls.
    /// role_id="" falls back to GetInstance().
    static AgentStateVisualizer& GetOrCreate(const std::string& role_id);

    /// Update all per-role instances (call from update loop)
    static void UpdateAll(float delta_time);

    /// Render all per-role instances (call from render loop)
    static void RenderAll();

    /// Initialize visual resources
    void Initialize();

    /// Render the state indicator
    void Render();

    /// Update internal state (call from update loop)
    void Update(float delta_time);

    /// Set the current agent state to display
    void SetAgentState(AgentRuntimeState state, const std::string& details = "");

    /// Toggle visibility
    void SetVisible(bool visible);
    bool IsVisible() const { return visible_; }

    /// Get current agent state
    AgentRuntimeState GetAgentState() const { return agent_state_; }

    /// Set character type (TEACHER/STUDENT/AI_ASSISTANT/MAGICAL_GIRL/COOL_SEMPAI)
    void SetCharacterType(AnimeCharacterType type);
    AnimeCharacterType GetCharacterType() const { return character_type_; }

 private:
    AgentStateVisualizer() = default;

    /// Draw blackboard background
    void DrawBlackboard();

    /// Load background texture from JPG
    void LoadBackground();

    /// Draw virtual human anime character with animations
    void DrawVirtualHumanCharacter();

    // State
    AgentRuntimeState agent_state_ = AgentRuntimeState::IDLE;
    AnimeCharacterType character_type_ = AnimeCharacterType::TEACHER;
    bool visible_ = true;
    float animation_time_ = 0.0f;
    float pulse_alpha_ = 1.0f;
    std::string state_details_;
    std::unique_ptr<Texture> bg_texture_;
};

}  // namespace prosophor
