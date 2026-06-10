// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <functional>

#include "media_engine/media_engine.h"
#include "ui_component/panel_container.h"

namespace prosophor {

using media_engine::Area;
using media_engine::PanelContainer;
using PanelFrame = PanelContainer;  // 兼容旧名 PanelFrame → PanelContainer

/// 统一面板文字行间距比例（font_scale × 1.5）
inline float Spacing() {
    return media_engine::Layout::GetFontScale() * 1.5f;
}

/// Save/Cancel 按钮栏（I18n 依赖，保持在 app 层）
void SaveCancelPanel(const Area& area, float btn_h,
                     std::function<void()> on_save,
                     std::function<void()> on_cancel = nullptr);

// ============================================================================
// PanelHelper — 存量辅助函数（逐步迁移到 PanelFrame）
// ============================================================================
struct PanelHelper {
    struct Area { float x, y, w, h; };
    static Area ContentArea(float cont_x, float cont_y, float cont_w, float cont_h);
    static void BeginPanel(float cx, float cy, float cw, float ch);
    static void PanelHeader(const char* title, float cx, float cy, float cw);
    static auto BeginScrollContent(float cx, float cy, float cw, float ch,
                                   float btn_h, float gap)
        -> media_engine::ScopedChild;
    static void SaveCancelBar(float cx, float cy, float cw, float ch,
                              float btn_h,
                              std::function<void()> on_save,
                              std::function<void()> on_cancel = nullptr);
    static void SectionCard(float cx, float cy, float cw, float h, const char* title);
    static float LabelRow(float cx, float iy, const char* label, float wx,
                          std::function<void()> widget_fn,
                          float spacing_scale = 1.0f);
    static float Spacing(float base, float scale);
};

// ============================================================================
// SplitView — 左右分割面板布局计算
//
// 统一 Config / Providers / Roles 的分割布局模式。
// 提供预计算的坐标值，调用方用独立 ScopedChild 控制生命周期。
//
// 用法:
//   auto sv = SplitView(f.a, left_w, f.btn_h, gap);
//   {
//     auto _l = media_engine::ScopedChild("left", sv.left_w, sv.inner_h, ...);
//     // 渲染左侧列表...
//   }
//   sv.DrawDivider();
//   {
//     auto _r = media_engine::ScopedChild("right", sv.right_w, sv.inner_h, ...);
//     // 渲染右侧内容...
//   }
// ============================================================================
struct SplitView {
    float left_x, left_y, left_w;       // 左侧 child 坐标
    float right_x, right_y, right_w;    // 右侧 child 坐标
    float inner_h;                      // child 高度
    float divider_x;                    // 分割线 X

    SplitView(const Area& content, float left_width,
              float btn_h = 0, float gap = 12.0f);
    void DrawDivider() const;           // 绘制分割线
};

// ============================================================================
// SectionForm — BeginScroll + SectionCard 表单容器
//
// 统一 TTS / LocalModels 及 Config/Roles 右侧详情的 SectionCard 表单模式。
// 用法:
//   SectionForm sf(f.a, title, card_h, f.btn_h);
//   float iy = sf.iy;
//   iy = PanelHelper::LabelRow(sf.cx, iy, label, sf.wx, widget_fn);
// ============================================================================
struct SectionForm {
    float cx;       // SectionCard 内容区 X
    float wx;       // 控件 X 位置 (label 右侧)
    float iy;       // 初始 Y 位置 (SectionCard 标题下方)
    media_engine::ScopedChild _child;

    SectionForm(const Area& area, const char* title, float card_h,
                float btn_h = 0, float gap = 12.0f);
};

// ============================================================================
// 通用组件 — CardBox / InfoRow / SeparatorLine / StatCard
// ============================================================================

/// 白色卡片容器 + CreamBorder 描边 + 标题（用于 Usage、Status、Debug 等面板内部分组）
/// 返回卡片底部 Y
float CardBox(float x, float y, float w, const char* title);

/// 信息行：label (Gray55) + value (Gray40) 同行显示
/// @return 下一行 Y
float InfoRow(float x, float iy, const char* label, const char* value, float label_w = 80.0f);

/// CreamBorder 水平分隔线
/// @return 下一行 Y（iy + 间距）
float SeparatorLine(float x, float iy, float w, float spacing = 8.0f);

/// 统计卡片（白底圆角 + 左侧彩色竖条），用于 Usage、Status 面板
void StatCard(float x, float y, float w, float h,
              const char* title, const char* value,
              const media_engine::Color& accent);

/// 占位面板：标题 + 信息行 + "Coming soon"，用于 memory/skills/security/scheduler/mcp
void PlaceholderView(int cont_x, int cont_y, int cont_w, int cont_h,
                     const char* view_title, const char* section_title,
                     std::initializer_list<const char*> lines);

} // namespace prosophor
