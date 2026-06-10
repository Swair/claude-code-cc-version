// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

namespace prosophor {

/// 全局布局配置 — 所有布局数值的单一事实来源
///
/// 仿 openakita/openclaw shell 布局:
/// ┌──────────┬─────────────────────────────────┐
/// │          │  TopBar (status + controls)      │
/// │ Sidebar  ├──────────────────┬──────────────┤
/// │ (nav)    │  Main Content    │ Right Panel  │
/// │          │  (view-based)    │ (role cards) │
/// │          ├──────────────────┤              │
/// │          │  Input Panel     │              │
/// └──────────┴──────────────────┴──────────────┘
///
/// 使用方法：在需要布局数值的地方 instance 一个 struct 读字段即可。
/// 所有像素单位都集中在这里，不要在业务代码中硬编码重复值。
struct LayoutConfig {
    // ════════════════════════════════════════════════════════════════════
    // 窗口默认尺寸
    // ════════════════════════════════════════════════════════════════════
    int   chat_window_width      = 1925;   // 主聊天窗口初始宽度 (px)
    int   chat_window_height     = 1335;   // 主聊天窗口初始高度 (px)
    int   chat_window_min_width  = 800;    // 窗口最小宽度
    int   chat_window_min_height = 600;    // 窗口最小高度

    // ════════════════════════════════════════════════════════════════════
    // Shell 骨架 — sidebar + topbar 构成主框架
    // ════════════════════════════════════════════════════════════════════
    int   sidebar_width_expanded  = 300;   // 侧边栏展开宽度 (px)
    int   sidebar_width_collapsed = 48;    // 侧边栏折叠宽度 (px)
    float topbar_height           = 32.0f; // 顶部状态栏高度 (px)

    // ── Sidebar 内部 ──
    int   sidebar_brand_height        = 96;    // Logo + 名称区域高度
    int   sidebar_footer_height       = 100;   // 底部链接 + 版本 + 语言 + 折叠按钮区域高度
    int   sidebar_nav_item_h          = 36;    // 每个导航项高度
    int   sidebar_group_label_h       = 36;    // 组标题高度（与导航项一致）
    float sb_icon_x                   = 8;     // 导航图标距左侧
    float sb_icon_w                   = 24;    // 图标占宽
    float sb_icon_text_gap            = 4;     // 图标与文字间距
    float sb_child_indent             = 12;    // 子项缩进
    float sb_group_child_gap          = 40;    // 组标题→首子项 y 进给
    float sb_group_tail_gap           = 8;     // 组尾额外间距
    float sb_text_h                   = 16;    // 文字高度（垂直居中用）
    float sb_sep_h                    = 5;     // 分隔线间距
    float sb_footer_left_pad          = 14;    // 底部链接/按钮左侧留白
    float sb_act_bar_w                = 3;     // 选中状态左侧橙色条宽度

    // ── TopBar 内部 ──
    float topbar_status_dot_r     = 4.0f;  // 状态圆点半径
    float topbar_btn_size         = 22.0f; // 右上角操作按钮尺寸

    // ════════════════════════════════════════════════════════════════════
    // 主内容区布局 (Sidebar 右侧，TopBar 下方)
    // ════════════════════════════════════════════════════════════════════
    float main_content_top_gap    = 4.0f;  // TopBar 下方的垂直间距
    float panel_inner_pad         = 8.0f;  // 面板容器内部缩进
    float panel_card_radius       = 6.0f;  // 面板卡片圆角
    float panel_btn_area_h        = 30.0f; // 底部按钮区域高度

    // ── Panel 内部间距 ──
    float panel_widget_spacing    = 26.0f; // 表单行间距
    float panel_label_w           = 140.0f;// 标签占宽 (SameLine 用)
    float panel_save_btn_w        = 70.0f; // Save 按钮宽度
    float panel_btn_gap           = 8.0f;  // 按钮间距
    float panel_btn_right_gap     = 40.0f; // 按钮组距右侧边距

    // ════════════════════════════════════════════════════════════════════
    // PanelFrame 布局常量 (ViewHeader + ContentArea + BeginScroll)
    // ════════════════════════════════════════════════════════════════════
    float view_title_x            = 12.0f; // 标题距左
    float view_title_y            = 12.0f; // 标题距顶
    float view_sep_y              = 32.0f; // 分隔线距顶
    float content_pad_x           = 12.0f; // 内容区距左
    float content_pad_y           = 44.0f; // 内容区距顶
    float content_extra_w         = 24.0f; // 内容区宽缩减
    float content_extra_h         = 56.0f; // 内容区高缩减
    float panel_radius            = 6.0f;  // 面板圆角
    float scroll_pad              = 8.0f;  // 滚动区内缩
    float scroll_w_extra          = 16.0f; // 滚动区宽缩减
    float section_card_pad        = 8.0f;  // SectionCard 内缩
    float section_card_w_extra    = 16.0f; // SectionCard 宽缩减
    float section_card_right_margin = 24.0f; // SectionCard 右侧边距
    float card_content_indent     = 16.0f; // SectionCard 内容区缩进 (icx = cx + 此值)
    float card_widget_offset      = 140.0f;// SectionCard 控件偏移标签 (iwx = icx + 此值)
    float section_title_gap       = 40.0f; // SectionCard 标题到内容行间距
    float split_list_item_h       = 30.0f; // 分割面板列表项高度
    float split_list_item_gap     = 2.0f;  // 分割面板列表项间距
    float split_list_text_x       = 12.0f; // 分割面板列表文字 X 偏移
    float split_list_text_y       = 7.0f;  // 分割面板列表文字 Y 偏移
    float split_left_gap          = 4.0f;  // 分割面板左右间距 (right_x = f.a.x + left_w + 此值)
    float split_right_child_pad   = 8.0f;  // 分割面板右侧 child 起始偏移
    float split_right_child_wextra = 8.0f; // 分割面板右侧 child 宽缩减 (与 BeginScroll 一致)
    float split_divider_w         = 1.0f;  // 分割栏线宽
    float label_row_pad           = 22.0f; // LabelRow 标签距左

    // ── 面板通用元素尺寸（跨面板复用）──
    float rounding_small          = 4.0f;  // 小圆角（项目列表、标签、徽章、筛选按钮）
    float content_pad             = 14.0f; // 内容区内缩（info 类型面板文字距左）
    float info_row_h              = 20.0f; // InfoRow 行高（debug/sessions/about），函数内乘以 fs*1.5

    // ════════════════════════════════════════════════════════════════════
    // Chat panel (消息历史 + 右侧角色卡片的分割)
    // ════════════════════════════════════════════════════════════════════
    float chat_panel_x_ratio      = 0.65f; // 消息面板左边界 (占主内容宽度比例)
    float chat_panel_width_ratio  = 0.35f; // 消息面板宽度 (占主内容宽度比例)

    // ════════════════════════════════════════════════════════════════════
    // 输入区域
    // ════════════════════════════════════════════════════════════════════
    float input_area_height       = 100.0f;// 底部输入区高度 (px)

    // ════════════════════════════════════════════════════════════════════
    // Right Panel (角色卡片列表)
    // ════════════════════════════════════════════════════════════════════
    float right_panel_ratio       = 0.20f; // 右侧角色面板宽度 (占全窗口比例)
    float right_panel_card_h      = 64.0f; // 角色卡片高度 (px)
    float right_panel_thumb_w     = 48.0f; // 角色缩略图宽度 (px)
    float right_panel_thumb_h     = 48.0f; // 角色缩略图高度 (px)
    float right_panel_header_h    = 44.0f; // 面板标题栏高度 (px)
    bool  right_panel_collapsible = true;  // 是否可折叠

    // ════════════════════════════════════════════════════════════════════
    // 日志面板 (Logs View)
    // ════════════════════════════════════════════════════════════════════
    float log_filter_btn_w        = 48.0f; // 级别过滤按钮宽度
    float log_filter_btn_h        = 22.0f; // 级别过滤按钮高度
    float log_filter_gap          = 4.0f;  // 过滤按钮间距

    // ════════════════════════════════════════════════════════════════════
    // 用量面板 (Usage View)
    // ════════════════════════════════════════════════════════════════════
    float usage_card_h            = 60.0f; // 统计卡片高度
    int   usage_card_cols         = 3;     // 统计卡片列数
    float usage_card_gap          = 12.0f; // 卡片间距

    // ════════════════════════════════════════════════════════════════════
    // 会话面板 (Sessions View)
    // ════════════════════════════════════════════════════════════════════
    float session_card_h          = 72.0f; // 会话卡片高度
    float session_card_radius     = 8.0f;  // 会话卡片圆角
    float session_focus_btn_w     = 56.0f; // 聚焦按钮宽度

    // ════════════════════════════════════════════════════════════════════
    // Desktop pet (sprite 窗口)
    // ════════════════════════════════════════════════════════════════════
    int   sprite_window_width     = 192;   // 精灵窗口宽度 (px)
    int   sprite_window_height    = 384;   // 精灵窗口高度 (px)
    float pet_sprite_size         = 192.0f;// 宠物精灵渲染尺寸 (px)
    float pet_ground_ratio        = 0.75f; // 宠物脚底位置 (win_h * 此比例)

    // ════════════════════════════════════════════════════════════════════
    // SpeechBubble (Sprite 弹出对话框)
    // ════════════════════════════════════════════════════════════════════
    float bubble_min_width        = 180.0f;// 气泡最小宽度
    float bubble_min_body_height  = 160.0f;// 气泡最小主体高度 (不含尾巴)
    float bubble_radius           = 10.0f; // 气泡圆角半径
    float bubble_padding          = 8.0f;  // 内容区四周留白
    float bubble_tail_height      = 12.0f; // 三角尾巴高度
    float bubble_title_height     = 18.0f; // 标题栏高度
    float bubble_input_height     = 30.0f; // 气泡内输入框高度
    float bubble_btn_size         = 18.0f; // 标题栏按钮尺寸

    // ════════════════════════════════════════════════════════════════════
    // Tray icon (系统托盘图标)
    // ════════════════════════════════════════════════════════════════════
    int   tray_icon_size          = 48;    // 托盘图标尺寸 (px)
    int   tray_margin             = 64;    // 距屏幕右下角偏移 (px)

    // ════════════════════════════════════════════════════════════════════
    // Token speed badge (tok/s 浮标)
    // ════════════════════════════════════════════════════════════════════
    float token_badge_pad         = 6.0f;  // 徽章内边距
    float token_badge_char_w      = 7.5f;  // 字符宽度估算
    float token_badge_h           = 16.0f; // 徽章文字高度
};

}  // namespace prosophor
