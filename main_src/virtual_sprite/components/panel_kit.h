#pragma once

#include <string>
#include <functional>
#include <initializer_list>
#include "media_engine/media_engine.h"
#include "ui_component/panel_container.h"

namespace prosophor {

using media_engine::Area;
using media_engine::PanelContainer;

// ── Spacing ──

inline float Spacing() {
    return media_engine::Layout::GetFontScale() * 1.5f;
}

// ── ActionBar ──

void ActionBar(const Area& area, float btn_h,
               std::function<void()> on_save,
               std::function<void()> on_cancel = nullptr);

// ── SplitPanel (左右分割面板布局) ──

struct SplitPanel {
    float left_x, left_y, left_w;
    float right_x, right_y, right_w;
    float inner_h;
    float divider_x;

    SplitPanel(const Area& content, float left_width,
               float btn_h = 0, float gap = 12.0f);
    void DrawDivider() const;
};

// ── PanelHelper (legacy) ──

struct PanelHelper {
    struct HelperArea { float x, y, w, h; };
    static HelperArea ContentArea(float cont_x, float cont_y, float cont_w, float cont_h);
    static void BeginPanel(float cx, float cy, float cw, float ch);
    static void PanelHeader(const char* title, float cx, float cy, float cw);
    static auto BeginScrollContent(float cx, float cy, float cw, float ch,
                                   float btn_h, float gap)
        -> media_engine::ScopedChild;
    static void SaveCancelBar(float cx, float cy, float cw, float ch,
                              float btn_h,
                              std::function<void()> on_save,
                              std::function<void()> on_cancel = nullptr);
    static float LabelRow(float cx, float iy, const char* label, float wx,
                          std::function<void()> widget_fn,
                          float spacing_scale = 1.0f);
    static float Spacing(float base, float scale);
};

// ── FocusCard (OrangeLightest 填充 + OrangeWarm 描边) ──

class FocusCard {
public:
    FocusCard(float x, float y, float w, float h,
              bool focused, bool hover = false, float radius = 6.0f);
};

// ── WhiteCard (白底 + CreamBorder 描边) ──

class WhiteCard {
public:
    /// Draw White roundrect + CreamBorder outline.
    /// @param title  optional title (rendered OrangeDeep at top)
    WhiteCard(float x, float y, float w, float h,
              const char* title = nullptr);
};

// ── Card — 通用卡片（背景 + 边框 + 标题 + 字段行 + 自适应高度） ──
//
// 边框在析构时根据内容末端高度绘制，自动适配。
// 用法:
//   {
//     Card card(x, y, w, "Title", sm);
//     card.Field("label1", [&]{ ... });
//     card.Field("label2", [&]{ ... });
//   } // 边框在此处绘制

class Card {
public:
    /// @param fixed_h      0=自适应高度，>0=固定高度
    /// @param title_above  标题在边框上方（默认 false=标题在边框内）
    Card(float x, float y, float w, const char* title, float sm = 1.0f,
         const media_engine::Color& bg_color = media_engine::Colors::Beige,
         const media_engine::Color& border_color = media_engine::Colors::Gray63,
         const media_engine::Color& title_color = media_engine::Colors::OrangeDeep,
         float radius = 6.0f, float fixed_h = 0,
         bool title_above = true);

    ~Card();

    /// 添加一个字段行（label + widget），返回下一行 Y
    float Field(const char* label, std::function<void()> widget_fn);

    float ContentX() const { return content_x_; }
    float WidgetX() const { return widget_x_; }
    float Y() const { return next_y_; }
    float BottomY() const { return next_y_; }

    void Advance(float dy) { next_y_ += dy; }
    void SetY(float y) { next_y_ = y; }

private:
    float x_, y_, w_, fixed_h_;
    std::string title_;
    media_engine::Color bg_color_;
    media_engine::Color border_color_;
    media_engine::Color title_color_;
    float radius_, sm_;
    float content_x_, widget_x_, next_y_;
    bool title_above_;
};

// ── SectionCard (Beige 圆角大卡) ──

class SectionCard {
public:
    /// Draw Beige rounded rect + Gray63 outline + OrangeDeep title.
    /// Precomputes ContentX / WidgetX / ContentY for subsequent content layout.
    SectionCard(float x, float y, float w, float card_h, const char* title);

    float ContentX = 0;  // content label X (x + card_content_indent)
    float WidgetX = 0;   // widget X (ContentX + card_widget_offset)
    float ContentY = 0;  // first content Y (y + section_title_gap)
};

// ── StatCard (白底 + 色条) ──

class StatCard {
public:
    StatCard(float x, float y, float w, float h,
             const char* title, const char* value,
             const media_engine::Color& accent);
};

// ── 通用组件 helpers ──

float CardBox(float x, float y, float w, const char* title);
float InfoRow(float x, float iy, const char* label, const char* value, float label_w = 80.0f);
float SeparatorLine(float x, float iy, float w, float spacing = 8.0f);
void PlaceholderPage(int cont_x, int cont_y, int cont_w, int cont_h,
                     const char* view_title, const char* section_title,
                     std::initializer_list<const char*> lines);

} // namespace prosophor
