#pragma once

#include <stdint.h>

namespace media_engine {

// ============================================================================
// 颜色定义
// ============================================================================
struct Color {
    uint8_t r, g, b, a;

    constexpr Color() : r(0), g(0), b(0), a(255) {}
    constexpr Color(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 255)
        : r(_r), g(_g), b(_b), a(_a) {}

    // ImGui 样式颜色槽索引 — 值匹配 ImGui 1.92.8
    enum Slot {
        Text = 0,
        WindowBg = 2,
        ChildBg = 3,
        PopupBg = 4,
        Border = 5,
        TitleBg = 8,
        TitleBgActive = 9,
        FrameBg = 10,
        Button = 18,
        ButtonHovered = 19,
        ButtonActive = 20,
    };
};

// 常用颜色枚举
namespace Colors {
    // ── 基础色 ──
    constexpr Color Transparent{0, 0, 0, 0};
    constexpr Color Black{0, 0, 0, 255};
    constexpr Color White{255, 255, 255, 255};
    constexpr Color Red{255, 0, 0, 255};
    constexpr Color Green{0, 255, 0, 255};
    constexpr Color Blue{0, 0, 255, 255};
    constexpr Color Yellow{255, 255, 0, 255};
    constexpr Color Cyan{0, 255, 255, 255};
    constexpr Color Magenta{255, 0, 255, 255};
    constexpr Color Gray{128, 128, 128, 255};

    // ── 灰色系（按亮度升序）──
    constexpr Color GrayBlack{10, 10, 10, 240};
    constexpr Color GrayNearBlack{35, 35, 35, 220};
    constexpr Color GrayDarkest{20, 20, 20, 200};
    constexpr Color Gray20{50, 50, 50, 255};
    constexpr Color Gray24{60, 60, 60, 255};
    constexpr Color DarkGray{64, 64, 64, 255};
    constexpr Color Gray31{80, 80, 80, 255};
    constexpr Color Gray35{90, 90, 90, 255};
    constexpr Color Gray40{100, 100, 100, 255};
    constexpr Color Gray47{120, 120, 120, 255};
    constexpr Color Gray51{130, 130, 130, 255};
    constexpr Color Gray55{140, 140, 140, 255};
    constexpr Color Gray63{160, 160, 160, 255};
    constexpr Color Gray70{180, 180, 180, 255};
    constexpr Color Gray78{200, 200, 200, 255};
    constexpr Color LightGray{192, 192, 192, 255};
    constexpr Color Silver{192, 192, 192, 255};
    constexpr Color Gray86{220, 220, 220, 255};
    constexpr Color Gray86a{220, 220, 215, 80};
    constexpr Color Gray86b{220, 220, 230, 200};
    constexpr Color Gray78a{200, 200, 200, 240};
    constexpr Color Gray70a{180, 180, 180, 230};
    constexpr Color Gray63a{160, 160, 160, 180};
    constexpr Color Gray55a{140, 140, 140, 130};
    constexpr Color Gray90{230, 230, 230, 255};
    constexpr Color Gray95{245, 245, 245, 235};
    constexpr Color Gray24a{60, 60, 60, 180};
    constexpr Color Gray24b{60, 60, 60, 200};

    // ── 暖白/奶油色系 ──
    constexpr Color Cream{250, 248, 245, 240};
    constexpr Color Cream70{250, 248, 245, 178};
    constexpr Color CreamTranslucent{250, 248, 245, 128};
    constexpr Color CreamOpaque90{250, 248, 245, 229};
    constexpr Color CreamLight{240, 238, 235, 255};
    constexpr Color CreamBorder{200, 195, 190, 255};
    constexpr Color CreamDark{180, 175, 170, 230};
    constexpr Color Beige{245, 245, 240, 255};
    constexpr Color Tan{210, 180, 140, 255};
    constexpr Color TanDark{160, 120, 80, 255};
    constexpr Color Taupe{180, 170, 160, 255};
    constexpr Color TaupeDark{150, 140, 130, 255};
    constexpr Color Peach{255, 220, 180, 255};

    // ── 红色系 ──
    constexpr Color DarkRed{139, 0, 0, 255};
    constexpr Color LightRed{255, 100, 100, 255};
    constexpr Color Crimson{220, 20, 60, 255};
    constexpr Color RedMid{220, 60, 60, 255};
    constexpr Color RedDark{200, 50, 50, 255};

    // ── 绿色系 ──
    constexpr Color DarkGreen{0, 100, 0, 255};
    constexpr Color LightGreen{144, 238, 144, 255};
    constexpr Color Lime{0, 255, 0, 255};
    constexpr Color Olive{128, 128, 0, 255};
    constexpr Color GreenMid{60, 160, 60, 255};
    constexpr Color GreenAssist{80, 160, 80, 255};
    constexpr Color GreenBright{100, 255, 100, 255};
    constexpr Color GreenLeafDark{50, 150, 50, 255};
    constexpr Color GreenLeafLight{70, 170, 70, 255};

    // ── 蓝色系 ──
    constexpr Color DarkBlue{0, 0, 139, 255};
    constexpr Color LightBlue{173, 216, 230, 255};
    constexpr Color Navy{0, 0, 128, 255};
    constexpr Color Teal{0, 128, 128, 255};
    constexpr Color NavyDark{20, 20, 40, 255};
    constexpr Color BlueMid{100, 140, 255, 230};
    constexpr Color BlueSoft{80, 120, 200, 255};
    constexpr Color BlueRoyal{65, 105, 225, 255};
    constexpr Color BlueSlate{50, 80, 150, 180};
    constexpr Color BlueLight{180, 220, 255, 255};
    constexpr Color BlueLightSoft{210, 240, 255, 210};
    constexpr Color BlueMedium{60, 100, 180, 255};
    constexpr Color BlueGray{70, 90, 120, 255};
    constexpr Color BlueGrayDark{50, 70, 90, 255};

    // ── 黄色/橙色系 ──
    constexpr Color Orange{255, 165, 0, 255};
    constexpr Color OrangeLight{255, 225, 195, 255};
    constexpr Color OrangeLightest{255, 240, 220, 255};
    constexpr Color OrangeWarm{200, 120, 50, 255};
    constexpr Color OrangeDeep{180, 100, 40, 255};
    constexpr Color Gold{255, 215, 0, 255};
    constexpr Color GoldDark{200, 180, 100, 255};
    constexpr Color GoldLight{255, 235, 185, 200};
    constexpr Color BeigeWarm{245, 245, 220, 255};

    // ── 紫色系 ──
    constexpr Color Purple{128, 0, 128, 255};
    constexpr Color Violet{238, 130, 238, 255};
    constexpr Color Indigo{75, 0, 130, 255};
    constexpr Color GrayLavender{180, 180, 200, 255};

    // ── 棕色系 ──
    constexpr Color Brown{165, 42, 42, 255};
    constexpr Color BrownDark{139, 90, 43, 255};
    constexpr Color BrownHair{60, 40, 20, 255};
    constexpr Color TerraCotta{180, 130, 80, 255};
    constexpr Color TerraCottaDark{150, 110, 60, 255};

    // ── 特殊效果色 ──
    constexpr Color Overlay{0, 0, 0, 180};
    constexpr Color OverlayDark{40, 40, 40, 180};
    constexpr Color WhiteTranslucent{255, 255, 255, 220};
    constexpr Color White70{255, 255, 255, 178};
    constexpr Color White80{255, 255, 255, 204};
    constexpr Color MilkyWhite{253, 251, 245, 255};       // 乳白色

    // ── 科技色系 ──
    constexpr Color CyanLight{100, 200, 255, 255};
    constexpr Color PinkBlush{255, 180, 180, 100};
    constexpr Color Pink{255, 180, 180, 255};

    // ── 聊天消息背景色 ──
    constexpr Color BluePale{235, 245, 255, 200};
    constexpr Color GreenPale{235, 245, 235, 200};
    constexpr Color Gray20a{50, 50, 50, 160};

    // ── 日志色 ──
    constexpr Color Amber{255, 200, 0, 255};
    constexpr Color RedBright{255, 50, 50, 255};
    constexpr Color GreenSuccess{50, 255, 100, 255};
}

// ============================================================================
// 颜色工具函数
// ============================================================================

// 将 32 位颜色 (ARGB) 转换为 Color
inline Color ColorFromUInt32(unsigned int color) {
    uint8_t a = (color >> 24) & 0xFF;
    uint8_t b = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t r = (color >> 0) & 0xFF;
    return Color(r, g, b, a);
}

// 将 Color 转换为 32 位 RGBA 值（IM_COL32 格式，用于 ImGui draw list）
inline constexpr unsigned int ColorToRGBA(const Color& color) {
    // IM_COL32 format: 0xAABBGGRR (as used by ImGui draw list)
    return (static_cast<unsigned int>(color.a) << 24) |
           (static_cast<unsigned int>(color.b) << 16) |
           (static_cast<unsigned int>(color.g) << 8) |
           (static_cast<unsigned int>(color.r));
}
} // namespace media_engine
