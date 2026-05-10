// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "media_engine/media_engine.h"
#include "components/ui_types.h"

#include <string>
#include <memory>
#include <unordered_map>

namespace prosophor {

/// 角色类型
enum class AnimeCharacterType {
    TEACHER,
    STUDENT,
    AI_ASSISTANT,
    MAGICAL_GIRL,
    COOL_SEMPAI,
};

/// 从角色 ID 字符串映射到立绘类型
inline AnimeCharacterType AnimeCharacterTypeFromRoleId(const std::string& role_id) {
    if (role_id == "teacher") return AnimeCharacterType::TEACHER;
    if (role_id == "student") return AnimeCharacterType::STUDENT;
    if (role_id == "ai_assistant") return AnimeCharacterType::AI_ASSISTANT;
    if (role_id == "magical_girl") return AnimeCharacterType::MAGICAL_GIRL;
    if (role_id == "cool_sempai") return AnimeCharacterType::COOL_SEMPAI;
    return AnimeCharacterType::TEACHER;
}

/// 二次元角色渲染器 - 支持 PNG 立绘 + 原始图形回退
class AnimeCharacterRenderer {
public:
    static AnimeCharacterRenderer& Instance();

    /// 渲染角色
    /// type: 角色类型
    /// cx, cy: 角色中心坐标
    /// state: AgentRuntimeState，决定表情/装饰
    /// scarf_color: 领结/围巾颜色（随状态变化）
    /// scale: 整体缩放
    /// alpha: 整体透明度 (0-1)
    /// is_blinking: 是否眨眼帧
    void Render(AnimeCharacterType type, float cx, float cy, AgentRuntimeState state,
                const Color& scarf_color, float scale, float alpha, bool is_blinking);

private:
    AnimeCharacterRenderer() = default;

    /// 加载指定角色的 PNG 立绘（失败时静默处理）
    void LoadPortraitTexture(AnimeCharacterType type);

    /// 获取角色对应的立绘文件路径
    std::string GetPortraitPath(AnimeCharacterType type) const;

    // ========== PNG 立绘模式 ==========
    /// 用 PNG 纹理渲染 + 覆盖物
    void RenderWithTexture(AnimeCharacterType /*type*/, float cx, float cy,
                           Texture& tex, AgentRuntimeState state,
                           const Color& scarf_color, float render_w, float render_h,
                           float alpha, bool is_blinking);

    // ========== 原始图形回退（Q版胸像） ==========
    void DrawFallbackPortrait(AnimeCharacterType type, float cx, float cy, float s,
                              AgentRuntimeState state, const Color& scarf_color,
                              uint8_t a, bool is_blinking);

    // 回退绘制子方法
    void DrawFallbackHair(AnimeCharacterType type, float cx, float cy, float s, uint8_t a);
    void DrawFallbackFace(float cx, float cy, float s, uint8_t a);
    void DrawFallbackEyes(AnimeCharacterType type, float cx, float cy, float s,
                          AgentRuntimeState state, uint8_t a, bool is_blinking);
    void DrawFallbackMouth(float cx, float cy, float s, AgentRuntimeState state, uint8_t a);
    void DrawFallbackBlush(float cx, float cy, float s, AgentRuntimeState state, uint8_t a);

    // 角色配色
    struct Palette {
        Color hair_main;
        Color hair_accent;
        Color eye_iris;
        Color eye_pupil;
        Color body;
    };
    Palette GetPalette(AnimeCharacterType type, uint8_t a) const;

    // PNG 立绘纹理缓存
    std::unordered_map<AnimeCharacterType, std::unique_ptr<Texture>> textures_;
};

}  // namespace prosophor
