// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace prosophor {

/// 全局布局配置 — 所有布局数值的单一事实来源
///
/// 使用方法：在需要布局数值的地方 instance 一个 struct 读字段即可。
/// 所有像素单位都集中在这里，不要在业务代码中硬编码重复值。
struct LayoutConfig {
    // ── Central chat window ──
    int chat_window_width = 1600;
    int chat_window_height = 800;

    // ── Chat panel (right-side percentage) ──
    float chat_panel_x_ratio = 0.65f;         // 左边界（宽度的比例 0.0~1.0）
    float chat_panel_width_ratio = 0.35f;      // 面板宽度（宽度的比例 0.0~1.0）

    // ── Bottom fixed-height areas (pixels) ──
    float input_area_height = 100.0f;          // 输入区高度
    float status_bar_height = 30.0f;          // 状态栏高度（保留字段，后续实现）

    // ── Close button (chat window) ──
    float close_btn_size = 22.0f;

    // ── Desktop pet (sprite window) ──
    int sprite_window_width = 280;
    int sprite_window_height = 380;
    float pet_sprite_size = 192.0f;           // 宠物精灵渲染尺寸
    float pet_ground_ratio = 0.70f;           // 宠物脚底位置 (win_h * ratio)

    // ── Navigation / status ──
    float tile_size = 32.0f;                  // 像素精灵地砖大小（保留）

    // ── SpeechBubble (sprite popup cloud) ──
    float bubble_min_width = 260.0f;
    float bubble_min_body_height = 200.0f;
    float bubble_radius = 14.0f;
    float bubble_padding = 12.0f;
    float bubble_tail_height = 20.0f;
    float bubble_title_height = 22.0f;
    float bubble_input_height = 34.0f;
    float bubble_btn_size = 22.0f;

    // ── Tray icon ──
    int tray_icon_size = 48;
    int tray_margin = 64;                     // 距屏幕右下角偏移
};

}  // namespace prosophor
