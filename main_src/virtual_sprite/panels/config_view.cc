#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/layout_config.h"
#include "config/config.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include "platform/platform.h"
#include <cstring>
#include <string>
#include <vector>

namespace prosophor {

namespace {

struct ConfigTab {
    const char* key;
    const char* label;
};

static std::vector<ConfigTab> s_tabs = {
    {"general",  "General"},
    {"security", "Security"},
};

static int s_sel = 0;

} // anonymous namespace

void ChatWindow::RenderConfigView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto Lc = LayoutConfig{};
    auto& L = I18n::Instance();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, L.Get("view_config").c_str(), Lc.panel_btn_area_h);

    auto& config = ProsophorConfig::GetInstance();
    float s = Spacing();
    float ls = s;
    auto Lc2 = Lc;
    Lc2.panel_widget_spacing *= ls;
    Lc2.split_list_item_h *= ls;
    Lc2.split_list_text_y *= ls;
    Lc2.split_list_item_gap *= ls;
    Lc2.section_title_gap *= ls;

    float gap = Lc2.panel_widget_spacing;
    float left_w = Lc2.panel_label_w;

    auto sv = SplitView(f.a, left_w, f.btn_h, gap);

    // ── Left: category list ──
    {
        media_engine::Layout::SetCursorScreenPos(sv.left_x, sv.left_y);
        auto _l = media_engine::ScopedChild("cfg_list", sv.left_w, sv.inner_h, 0,
            media_engine::ImGuiWindowFlags_None);

        float cy = sv.left_y;
        for (int i = 0; i < (int)s_tabs.size(); ++i) {
            bool act = (i == s_sel);
            float bh = Lc2.split_list_item_h;
            float iw = sv.left_w - 4.0f;
            media_engine::Layout::SetCursorScreenPos(sv.left_x, cy);
            if (media_engine::ImGuiWidget::InvisibleButton(
                    ("cfg_tab_" + std::to_string(i)).c_str(), sv.left_w, bh))
                s_sel = i;
            bool hov = media_engine::ImGuiWidget::IsItemHovered();
            if (act)
                media_engine::DrawList::Selection(sv.left_x, cy, iw, bh, 3.0f,
                    media_engine::Colors::Orange, media_engine::Colors::OrangeLightest, 4.0f);
            else if (hov)
                media_engine::DrawList::RoundRect(sv.left_x, cy, iw, bh, 4.0f,
                    media_engine::Colors::OrangePale);
            media_engine::DrawList::Text(sv.left_x + Lc2.split_list_text_x - Lc2.split_list_item_gap, cy + Lc2.split_list_text_y,
                act ? media_engine::Colors::OrangeDeep
                    : hov ? media_engine::Colors::Orange
                    : media_engine::Colors::Gray40,
                L.Get(s_tabs[i].label).c_str());
            cy += bh + Lc2.split_list_item_gap;
        }
    }

    sv.DrawDivider();

    // ── Right: selected category config ──
    {
        media_engine::Layout::SetCursorScreenPos(sv.right_x, sv.right_y);
        auto _r = media_engine::ScopedChild("cfg_content", sv.right_w, sv.inner_h,
            0, media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);

        float cx = sv.right_x;
        float cw = sv.right_w - Lc2.section_card_right_margin + Lc2.split_right_child_wextra;
        float iy = sv.right_y + Lc2.split_list_text_y;

        if (s_tabs[s_sel].key == std::string("general")) {
            PanelHelper::SectionCard(cx, iy, cw, 180.0f * ls, L.Get("tab_general").c_str());
            iy += Lc2.section_title_gap;
            float icx = cx + Lc2.card_content_indent;
            float iwx = icx + Lc2.card_widget_offset;

            const char* levels[] = {"trace","debug","info","warn","error"};
            int ll = 0; for (int i = 0; i < 5; ++i) { if (config.log_level == levels[i]) { ll = i; break; } }
            iy = PanelHelper::LabelRow(icx, iy, L.Get("general_log_level").c_str(), iwx, [&](){
                float cw_half = ((cx + cw) - iwx) * 0.5f;
                auto _w = media_engine::ScopedItemWidth(cw_half);
                if (media_engine::ImGuiWidget::Combo("##cfg_ll", &ll, levels, 5)) config.log_level = levels[ll]; }, ls);

            int font_sel = (config.font_scale < ProsophorConfig::kFontScaleSwitch) ? 0 : 1;
            constexpr float kFontVals[] = {ProsophorConfig::kFontScaleSmall, ProsophorConfig::kFontScaleLarge};
            const char* font_labels[] = {L.Get("font_small").c_str(), L.Get("font_large").c_str()};
            iy = PanelHelper::LabelRow(icx, iy, L.Get("font_size").c_str(), iwx, [&](){
                for (int fi = 0; fi < 2; ++fi) {
                    if (fi > 0) media_engine::Layout::SameLine();
                    if (media_engine::ImGuiWidget::Button(font_labels[fi], 0, 0)) {
                        font_sel = fi;
                        float new_val = kFontVals[fi];
                        if (std::abs(config.font_scale - new_val) > 0.001f) {
                            config.font_scale = new_val;
                            media_engine::MediaCore::SetGlobalFontScale(config.font_scale);
                        }
                    }
                }
            }, ls);

            iy = PanelHelper::LabelRow(icx, iy, L.Get("general_enable_summary").c_str(), iwx,
                [&](){ media_engine::ImGuiWidget::Checkbox("##cfg_sum", &config.enable_summary); }, ls);

            iy = PanelHelper::LabelRow(icx, iy, L.Get("general_sprite_assets_dir").c_str(), iwx, [&](){
                char buf[512]; std::strncpy(buf, config.sprite_assets_dir.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1] = 0;
                float input_w = std::max(60.0f, ((cx + cw) - iwx - 36.0f - 4.0f) * 0.5f);
                { auto _w = media_engine::ScopedItemWidth(input_w);
                  media_engine::ImGuiWidget::InputText("##cfg_sd", buf, sizeof(buf)); }
                media_engine::Layout::SameLine();
                if (media_engine::ImGuiWidget::Button("...##b", 36.0f, 0)) { auto sel = Platform::BrowseForDirectory(); if (!sel.empty()) config.sprite_assets_dir = sel; }
            }, ls);
        }

        if (s_tabs[s_sel].key == std::string("security")) {
            PanelHelper::SectionCard(cx, iy, cw, 100.0f * ls, L.Get("tab_security").c_str());
            iy += Lc2.section_title_gap;
            float icx = cx + Lc2.card_content_indent;
            float iwx = icx + Lc2.card_widget_offset;

            const char* pls[] = {"auto","allow","deny"};
            int pl = 0; for (int i = 0; i < 3; ++i) { if (config.security.permission_level == pls[i]) { pl = i; break; } }
            iy = PanelHelper::LabelRow(icx, iy, L.Get("security_permission_level").c_str(), iwx, [&](){
                float cw_half = ((cx + cw) - iwx) * 0.5f;
                auto _w = media_engine::ScopedItemWidth(cw_half);
                if (media_engine::ImGuiWidget::Combo("##cfg_pl", &pl, pls, 3)) config.security.permission_level = pls[pl]; }, ls);
            iy = PanelHelper::LabelRow(icx, iy, L.Get("security_allow_local_exec").c_str(), iwx,
                [&](){ media_engine::ImGuiWidget::Checkbox("##cfg_le", &config.security.allow_local_execute); }, ls);
        }
    }

    SaveCancelPanel(f.a, f.btn_h,
        [&](){ config.SaveToFile(); },
        [&](){ config = ProsophorConfig::LoadFromFile(ProsophorConfig::DefaultConfigPath()); });
}

} // namespace prosophor
