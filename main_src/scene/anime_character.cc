// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "scene/anime_character.h"
#include "scene/asset_define.h"
#include "media_engine/media_engine.h"
#include "common/log_wrapper.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace prosophor {

AnimeCharacterRenderer& AnimeCharacterRenderer::Instance() {
    static AnimeCharacterRenderer instance;
    return instance;
}

// ============================================================================
// 角色调色板
// ============================================================================
AnimeCharacterRenderer::Palette AnimeCharacterRenderer::GetPalette(
    AnimeCharacterType type, uint8_t a) const {
    switch (type) {
        case AnimeCharacterType::TEACHER:
            return {Color{90, 40, 120, a}, Color{120, 60, 150, a},
                    Color{80, 30, 130, a}, Color{20, 10, 40, a},
                    Color{50, 30, 70, a}};
        case AnimeCharacterType::STUDENT:
            return {Color{220, 190, 60, a}, Color{255, 220, 80, a},
                    Color{50, 180, 255, a}, Color{10, 40, 80, a},
                    Color{250, 250, 255, a}};
        case AnimeCharacterType::AI_ASSISTANT:
            return {Color{200, 210, 230, a}, Color{160, 180, 255, a},
                    Color{0, 200, 255, a}, Color{0, 100, 150, a},
                    Color{30, 30, 50, a}};
        case AnimeCharacterType::MAGICAL_GIRL:
            return {Color{255, 150, 200, a}, Color{255, 180, 220, a},
                    Color{255, 80, 120, a}, Color{80, 20, 40, a},
                    Color{255, 180, 200, a}};
        case AnimeCharacterType::COOL_SEMPAI:
            return {Color{50, 80, 180, a}, Color{30, 60, 150, a},
                    Color{60, 100, 180, a}, Color{20, 30, 60, a},
                    Color{100, 130, 180, a}};
    }
    return {};
}

// ============================================================================
// PNG 立绘路径
// ============================================================================
std::string AnimeCharacterRenderer::GetPortraitPath(AnimeCharacterType type) const {
    std::string base = PortraitDir();
    switch (type) {
        case AnimeCharacterType::TEACHER:     return base + "teacher.png";
        case AnimeCharacterType::STUDENT:     return base + "student.png";
        case AnimeCharacterType::AI_ASSISTANT: return base + "ai_assistant.png";
        case AnimeCharacterType::MAGICAL_GIRL: return base + "magical_girl.png";
        case AnimeCharacterType::COOL_SEMPAI:  return base + "cool_sempai.png";
    }
    return "";
}

void AnimeCharacterRenderer::LoadPortraitTexture(AnimeCharacterType type) {
    if (textures_.count(type)) return;  // already cached

    std::string path = GetPortraitPath(type);
    if (path.empty() || !std::filesystem::exists(path)) {
        textures_[type] = nullptr;  // cache as not-found
        return;
    }

    auto tex = std::make_unique<Texture>(path);
    if (tex->GetOriginWidth() <= 0 || tex->GetOriginHeight() <= 0) {
        textures_[type] = nullptr;
        return;
    }

    LOG_INFO("Loaded portrait: {}", path);
    textures_[type] = std::move(tex);
}

// ============================================================================
// PNG 立绘渲染
// ============================================================================
void AnimeCharacterRenderer::RenderWithTexture(
    AnimeCharacterType /*type*/, float cx, float cy,
    Texture& tex, AgentRuntimeState state,
    const Color& scarf_color, float render_w, float render_h,
    float alpha, bool is_blinking) {

    uint8_t a = static_cast<uint8_t>(alpha * 255);

    // 呼吸浮动
    float breathe_y = std::sin(/* shared animation time */ 0.0f) * 6.0f;
    // 简单脉冲（thinking 状态）
    float pulse = 1.0f;
    if (state == AgentRuntimeState::BEGINNING ||
        state == AgentRuntimeState::EXECUTING_TOOL) {
        pulse = 0.92f + 0.08f * alpha;
    }

    float final_w = render_w * pulse;
    float final_h = render_h * pulse;
    float x = cx - final_w / 2.0f;
    float y = cy - final_h / 2.0f + breathe_y;

    tex.RenderTexture(x, y, final_w, final_h);

    // 状态色调覆盖（薄层）
    Color tint(scarf_color.r, scarf_color.g, scarf_color.b,
               static_cast<uint8_t>(a * 0.08f));
    ::Drawer::Instance().DrawFillRect(x, y, final_w, final_h, tint);

    // Error 状态：红色 X 覆盖
    if (state == AgentRuntimeState::STATE_ERROR) {
        float ex = cx, ey = cy + breathe_y;
        float len = std::min(final_w, final_h) * 0.15f;
        Color red(255, 60, 60, a);
        ::Drawer::Instance().DrawLine(ex - len, ey - len, ex + len, ey + len, red);
        ::Drawer::Instance().DrawLine(ex + len, ey - len, ex - len, ey + len, red);
    }

    // 眨眼覆盖（黑色细线）
    if (is_blinking) {
        float eye_y = cy - final_h * 0.05f + breathe_y;
        float eye_spacing = final_w * 0.13f;
        Color line(30, 20, 40, a);
        for (float ex : {cx - eye_spacing, cx + eye_spacing}) {
            ::Drawer::Instance().DrawFillRect(
                ex - final_w * 0.04f, eye_y,
                final_w * 0.08f, final_h * 0.015f, line);
        }
    }
}

// ============================================================================
// 主渲染入口
// ============================================================================
void AnimeCharacterRenderer::Render(AnimeCharacterType type, float cx, float cy,
                                     AgentRuntimeState state, const Color& scarf_color,
                                     float scale, float alpha, bool is_blinking) {
    uint8_t a = static_cast<uint8_t>(alpha * 255);

    // ---- 尝试 PNG 立绘 ----
    auto it = textures_.find(type);
    if (it == textures_.end()) {
        // 首次访问，尝试加载
        LoadPortraitTexture(type);
        it = textures_.find(type);
    }

    if (it != textures_.end() && it->second && it->second->GetOriginWidth() > 0) {
        Texture& tex = *it->second;
        // 立绘尺寸：基于 scale，保持宽高比
        float base = scale * 45.0f;
        float tw = tex.GetOriginWidth();
        float th = tex.GetOriginHeight();
        float render_h = base;
        float render_w = base * (tw / th);
        if (render_w > base * 0.8f) {
            render_w = base * 0.8f;
            render_h = render_w * (th / tw);
        }
        // 垂直居中（cy 偏下，向上偏移）
        float portrait_y = cy - 48.0f * scale;
        RenderWithTexture(type, cx, portrait_y, tex, state, scarf_color,
                          render_w, render_h, alpha, is_blinking);
        return;
    }

    // ---- 无 PNG 立绘：Q 版胸像回退 ----
    float s = scale * 2.2f;
    float portrait_y = cy - 28.0f * scale;
    DrawFallbackPortrait(type, cx, portrait_y, s, state, scarf_color, a, is_blinking);
}

// ============================================================================
// Q 版胸像回退渲染
// ============================================================================
void AnimeCharacterRenderer::DrawFallbackPortrait(
    AnimeCharacterType type, float cx, float cy, float s,
    AgentRuntimeState state, const Color& scarf_color,
    uint8_t a, bool is_blinking) {

    auto pal = GetPalette(type, a);

    // ---- 肩膀/身体 ----
    ::Drawer::Instance().DrawFillTriangle(
        cx - 22 * s, cy + 14 * s, cx + 22 * s, cy + 14 * s,
        cx + 30 * s, cy + 38 * s, pal.body);
    ::Drawer::Instance().DrawFillTriangle(
        cx - 22 * s, cy + 14 * s, cx - 30 * s, cy + 38 * s,
        cx + 30 * s, cy + 38 * s, pal.body);

    // ---- 围巾/领结 ----
    ::Drawer::Instance().DrawFillEllipse(cx, cy + 2 * s, 12 * s, 5 * s, scarf_color);
    ::Drawer::Instance().DrawFillTriangle(
        cx, cy + 2 * s, cx - 8 * s, cy + 10 * s, cx + 8 * s, cy + 10 * s, scarf_color);

    // ---- 头发（在面部之前） ----
    DrawFallbackHair(type, cx, cy, s, a);

    // ---- 脸 ----
    DrawFallbackFace(cx, cy, s, a);

    // ---- 眼睛 ----
    DrawFallbackEyes(type, cx, cy, s, state, a, is_blinking);

    // ---- 嘴巴 ----
    DrawFallbackMouth(cx, cy, s, state, a);

    // ---- 脸红 ----
    DrawFallbackBlush(cx, cy, s, state, a);
}

// ============================================================================
// Q 版脸部
// ============================================================================
void AnimeCharacterRenderer::DrawFallbackFace(float cx, float cy, float s, uint8_t a) {
    Color skin(255, 224, 192, a);
    // 圆润脸型（大脸 = 可爱）
    ::Drawer::Instance().DrawFillEllipse(cx, cy - 4 * s, 30 * s, 28 * s, skin);
    // 柔和下巴
    ::Drawer::Instance().DrawFillTriangle(
        cx - 15 * s, cy + 10 * s, cx, cy + 18 * s, cx + 15 * s, cy + 10 * s, skin);
}

// ============================================================================
// Q 版头发
// ============================================================================
void AnimeCharacterRenderer::DrawFallbackHair(
    AnimeCharacterType type, float cx, float cy, float s, uint8_t a) {

    auto pal = GetPalette(type, a);

    // 主发球 - 角色通用
    ::Drawer::Instance().DrawFillEllipse(cx, cy - 20 * s, 32 * s, 26 * s, pal.hair_main);

    // 刘海
    ::Drawer::Instance().DrawFillEllipse(cx - 10 * s, cy - 8 * s, 12 * s, 8 * s, pal.hair_main);
    ::Drawer::Instance().DrawFillEllipse(cx + 10 * s, cy - 8 * s, 12 * s, 8 * s, pal.hair_main);
    ::Drawer::Instance().DrawFillEllipse(cx, cy - 10 * s, 10 * s, 8 * s, pal.hair_accent);

    // 侧发
    ::Drawer::Instance().DrawFillEllipse(cx - 24 * s, cy + 2 * s, 8 * s, 18 * s, pal.hair_main);
    ::Drawer::Instance().DrawFillEllipse(cx + 24 * s, cy + 2 * s, 8 * s, 18 * s, pal.hair_main);

    // 角色特定发饰
    switch (type) {
        case AnimeCharacterType::TEACHER: {
            // 发箍
            Color ribbon(200, 50, 50, a);
            ::Drawer::Instance().DrawFillEllipse(cx, cy - 22 * s, 22 * s, 4 * s, ribbon);
            ::Drawer::Instance().DrawFillCircle(cx, cy - 22 * s, 3 * s, Color(180, 40, 40, a));
            break;
        }
        case AnimeCharacterType::STUDENT: {
            // 发圈
            Color tie(220, 60, 60, a);
            ::Drawer::Instance().DrawFillCircle(cx - 26 * s, cy - 14 * s, 4 * s, tie);
            ::Drawer::Instance().DrawFillCircle(cx + 26 * s, cy - 14 * s, 4 * s, tie);
            break;
        }
        case AnimeCharacterType::AI_ASSISTANT: {
            // 头顶发光三角形
            Color glow(100, 150, 255, static_cast<uint8_t>(a * 0.6f));
            ::Drawer::Instance().DrawFillTriangle(
                cx - 6 * s, cy - 32 * s, cx, cy - 44 * s, cx + 6 * s, cy - 32 * s, glow);
            ::Drawer::Instance().DrawFillCircle(cx, cy - 38 * s, 2 * s, Color(200, 220, 255, a));
            break;
        }
        case AnimeCharacterType::MAGICAL_GIRL: {
            // 星星发饰
            Color star(255, 220, 50, a);
            ::Drawer::Instance().DrawFillTriangle(
                cx - 16 * s, cy - 30 * s, cx - 12 * s, cy - 24 * s,
                cx - 20 * s, cy - 24 * s, star);
            ::Drawer::Instance().DrawFillTriangle(
                cx + 16 * s, cy - 30 * s, cx + 12 * s, cy - 24 * s,
                cx + 20 * s, cy - 24 * s, star);
            break;
        }
        case AnimeCharacterType::COOL_SEMPAI:
            // 无额外发饰
            break;
    }
}

// ============================================================================
// Q 版眼睛（大眼 = 可爱核心）
// ============================================================================
void AnimeCharacterRenderer::DrawFallbackEyes(
    AnimeCharacterType type, float cx, float cy, float s,
    AgentRuntimeState state, uint8_t a, bool is_blinking) {

    auto pal = GetPalette(type, a);
    float eye_spacing = 11 * s;
    float eye_y = cy - 8 * s;

    if (is_blinking) {
        Color line(40, 20, 60, a);
        for (float ex : {cx - eye_spacing, cx + eye_spacing}) {
            ::Drawer::Instance().DrawFillRect(ex - 6 * s, eye_y, 12 * s, 2.5f * s, line);
        }
        return;
    }

    // ERROR 表情：> < 眼睛
    if (state == AgentRuntimeState::STATE_ERROR) {
        for (float ex : {cx - eye_spacing, cx + eye_spacing}) {
            Color frown(40, 20, 60, a);
            ::Drawer::Instance().DrawFillEllipse(ex, eye_y, 10 * s, 4 * s, frown);
        }
        return;
    }

    // 正常/开心：大眼白 + 瞳孔 + 高光
    for (float ex : {cx - eye_spacing, cx + eye_spacing}) {
        // 眼白
        ::Drawer::Instance().DrawFillEllipse(ex, eye_y, 11 * s, 13 * s,
                                             Color(255, 255, 255, a));
        // 虹膜
        ::Drawer::Instance().DrawFillEllipse(ex, eye_y + 1 * s, 7 * s, 9 * s, pal.eye_iris);
        // 瞳孔
        ::Drawer::Instance().DrawFillCircle(ex, eye_y + 1 * s, 3.5f * s, pal.eye_pupil);
        // 主高光（左上）
        ::Drawer::Instance().DrawFillCircle(ex - 2.5f * s, eye_y - 3 * s,
                                            2 * s, Color(255, 255, 255, a));
        // 副高光（右下小点）
        ::Drawer::Instance().DrawFillCircle(ex + 2.5f * s, eye_y + 4 * s,
                                            1.2f * s, Color(255, 255, 255, a));
    }

    // 眉毛
    Color brow(80, 60, 80, a);
    float brow_y = eye_y - 8 * s;
    if (state == AgentRuntimeState::STATE_ERROR) {
        ::Drawer::Instance().DrawFillTriangle(
            cx - eye_spacing - 7 * s, brow_y - 2 * s,
            cx - eye_spacing + 7 * s, brow_y + 2 * s,
            cx - eye_spacing + 7 * s, brow_y + 3 * s, brow);
        ::Drawer::Instance().DrawFillTriangle(
            cx + eye_spacing - 7 * s, brow_y + 2 * s,
            cx + eye_spacing + 7 * s, brow_y - 2 * s,
            cx + eye_spacing - 7 * s, brow_y + 3 * s, brow);
    } else {
        ::Drawer::Instance().DrawFillEllipse(cx - eye_spacing, brow_y, 7 * s, 2 * s, brow);
        ::Drawer::Instance().DrawFillEllipse(cx + eye_spacing, brow_y, 7 * s, 2 * s, brow);
    }
}

// ============================================================================
// Q 版嘴巴
// ============================================================================
void AnimeCharacterRenderer::DrawFallbackMouth(
    float cx, float cy, float s, AgentRuntimeState state, uint8_t a) {

    Color mouth(200, 100, 100, a);
    float my = cy + 5 * s;

    switch (state) {
        case AgentRuntimeState::COMPLETE:
            // 开心张嘴
            ::Drawer::Instance().DrawFillEllipse(cx, my, 8 * s, 5 * s, mouth);
            break;
        case AgentRuntimeState::STREAM_CONTENT_TYPING:
            // 说话 O 型
            ::Drawer::Instance().DrawFillEllipse(cx, my, 4 * s, 6 * s, mouth);
            break;
        case AgentRuntimeState::STATE_ERROR:
            // 撇嘴
            ::Drawer::Instance().DrawFillTriangle(
                cx - 4 * s, my, cx + 4 * s, my, cx + 1 * s, my + 3 * s, mouth);
            break;
        case AgentRuntimeState::BEGINNING:
            // 微笑
            ::Drawer::Instance().DrawFillRect(cx - 5 * s, my, 10 * s, 2.5f * s, mouth);
            break;
        default:
            // 普通微笑
            ::Drawer::Instance().DrawFillEllipse(cx, my + 1 * s, 6 * s, 2.5f * s, mouth);
            break;
    }
}

// ============================================================================
// Q 版脸红
// ============================================================================
void AnimeCharacterRenderer::DrawFallbackBlush(
    float cx, float cy, float s, AgentRuntimeState state, uint8_t a) {

    if (state == AgentRuntimeState::COMPLETE ||
        state == AgentRuntimeState::BEGINNING) {
        Color blush(255, 150, 150, static_cast<uint8_t>(a * 0.25f));
        float by = cy - 2 * s;
        ::Drawer::Instance().DrawFillEllipse(cx - 16 * s, by, 7 * s, 3.5f * s, blush);
        ::Drawer::Instance().DrawFillEllipse(cx + 16 * s, by, 7 * s, 3.5f * s, blush);
    }
}

}  // namespace prosophor
