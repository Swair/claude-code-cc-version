#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/layout_config.h"
#include "config/config.h"
#include "config/role_config_manager.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include "common/file_utils.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstring>

namespace prosophor {

namespace {

static std::vector<std::string> all_ids, dnames, avail_models, descrs, prompts;
static std::vector<int> checked, model_idx, voice_idx, auto_confirms;
static std::vector<const char*> m_cstrs, v_cstrs;
static bool scan = true;
static int s_sel = 0;

void ScanRoles() {
    auto& config = ProsophorConfig::GetInstance();
    all_ids.clear(); dnames.clear(); descrs.clear(); prompts.clear();
    std::string rd = (ProsophorConfig::BaseDir() / "roles").string();
    if (DirExists(rd)) for (auto& e : std::filesystem::directory_iterator(rd))
        if (e.is_regular_file() && e.path().extension() == ".json") all_ids.push_back(e.path().stem().string());
    std::sort(all_ids.begin(), all_ids.end());
    dnames.resize(all_ids.size()); checked.assign(all_ids.size(), 0);
    descrs.resize(all_ids.size()); prompts.resize(all_ids.size());
    auto_confirms.assign(all_ids.size(), 0);
    for (size_t i = 0; i < all_ids.size(); ++i) {
        if (std::find(config.default_role.begin(), config.default_role.end(), all_ids[i]) != config.default_role.end()) checked[i] = 1;
        std::string fp = (ProsophorConfig::BaseDir() / "roles" / (all_ids[i] + ".json")).string();
        std::ifstream f(fp); if (f.is_open()) try { auto rj = nlohmann::json::parse(f);
            dnames[i] = rj.value("display_name", all_ids[i]);
            descrs[i] = rj.value("description", "");
            prompts[i] = rj.value("personality_prompt", "");
            auto_confirms[i] = rj["llm"].value("auto_confirm_tools", false) ? 1 : 0;
        } catch(...) { dnames[i] = all_ids[i]; } else dnames[i] = all_ids[i];
    }
    avail_models.clear();
    for (auto& [pn, pv] : config.llm_providers) for (auto& [mn, mc] : pv.model_configs) { std::string d = "[" + pn + "] " + mc.model; if (std::find(avail_models.begin(), avail_models.end(), d) == avail_models.end()) avail_models.push_back(d); }
    m_cstrs.clear(); for (auto& m : avail_models) m_cstrs.push_back(m.c_str());
    v_cstrs.clear(); for (auto& v : config.tts.voice_list) v_cstrs.push_back(v.c_str());
    model_idx.assign(all_ids.size(), 0); voice_idx.assign(all_ids.size(), 0);
    for (size_t i = 0; i < all_ids.size(); ++i) {
        std::string fp = (ProsophorConfig::BaseDir() / "roles" / (all_ids[i] + ".json")).string();
        std::ifstream f(fp); if (!f.is_open()) continue;
        try { auto rj = nlohmann::json::parse(f);
            std::string rm = rj["llm"]["model"];
            for (size_t ai = 0; ai < avail_models.size(); ++ai) { auto be = avail_models[ai].find("] "); if (be != std::string::npos && avail_models[ai].substr(be + 2) == rm) { model_idx[i] = (int)ai; break; } }
            if (rj.contains("tts") && rj["tts"].is_object()) { std::string v = rj["tts"].value("voice",""); for (size_t vi = 0; vi < config.tts.voice_list.size(); ++vi) { if (config.tts.voice_list[vi] == v) { voice_idx[i] = (int)vi; break; } } }
        } catch(...) {}
    }
    scan = false;
}

} // anonymous namespace

void ChatWindow::RenderRolesView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto Lc = LayoutConfig{};
    auto& L = I18n::Instance();
    PanelFrame pf(cont_x, cont_y, cont_w, cont_h, L.Get("view_roles").c_str());

    auto& config = ProsophorConfig::GetInstance();
    float s = Spacing();
    if (scan) ScanRoles();

    float btn_h = 30.0f, gap = 12.0f;
    int vc = (int)v_cstrs.size();
    auto Lc2 = Lc;
    Lc2.split_list_item_h = 30.0f * s;
    Lc2.split_list_text_y = 7.0f * s;
    Lc2.split_list_item_gap = 2.0f * s;
    Lc2.split_list_text_x = 12.0f * s;

    float left_w = 220.0f * s;

    auto sv = SplitView(pf.a, left_w, btn_h, gap);

    if (s_sel >= (int)all_ids.size()) s_sel = 0;

    // ── Left: role list ──
    {
        media_engine::Layout::SetCursorScreenPos(sv.left_x, sv.left_y);
        auto _l = media_engine::ScopedChild("rl_list", sv.left_w, sv.inner_h, 0,
            media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);

        float cy = sv.left_y;
        for (size_t i = 0; i < all_ids.size(); ++i) {
            bool act = ((int)i == s_sel);
            float bh = Lc2.split_list_item_h;
            float iw = sv.left_w - 4.0f;
            bool ck = (checked[i] != 0);
            media_engine::Layout::SetCursorScreenPos(sv.left_x, cy);
            if (media_engine::ImGuiWidget::InvisibleButton(
                    ("rl_" + all_ids[i]).c_str(), sv.left_w, bh))
                s_sel = (int)i;
            bool hov = media_engine::ImGuiWidget::IsItemHovered();
            if (act)
                media_engine::DrawList::Selection(sv.left_x, cy, iw, bh, 3.0f,
                    media_engine::Colors::Orange, media_engine::Colors::OrangeLightest, 4.0f);
            else if (hov)
                media_engine::DrawList::RoundRect(sv.left_x, cy, iw, bh, 4.0f,
                    media_engine::Colors::OrangePale);
            std::string lbl = all_ids[i];
            if (!dnames[i].empty() && dnames[i] != all_ids[i]) lbl += " - " + dnames[i];
            media_engine::DrawList::Text(sv.left_x + Lc2.split_list_text_x - Lc2.split_list_item_gap, cy + Lc2.split_list_text_y,
                act ? media_engine::Colors::OrangeDeep
                    : hov ? media_engine::Colors::Orange
                    : media_engine::Colors::Gray40,
                lbl.c_str());
            media_engine::Layout::SetCursorScreenPos(sv.left_x + sv.left_w - 20.0f * s, cy + 5.0f * s);
            media_engine::ImGuiWidget::Checkbox(("##rl_ck_" + all_ids[i]).c_str(), &ck);
            checked[i] = ck ? 1 : 0;
            cy += bh + Lc2.split_list_item_gap;
        }
    }

    sv.DrawDivider();

    // ── Right: selected role config ──
    {
        media_engine::Layout::SetCursorScreenPos(sv.right_x, sv.right_y);
        auto _r = media_engine::ScopedChild("rl_cfg", sv.right_w, sv.inner_h,
            0, media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);

        if (all_ids.empty()) {
            media_engine::Text::Colored(media_engine::Colors::Gray55, L.Get("panel_no_data").c_str());
            goto buttons;
        }

        size_t i = (size_t)s_sel;

        // ── SectionCard (matching Config style, dynamic height) ──
        float cx = sv.right_x;
        float cw = sv.right_w - Lc.section_card_right_margin + Lc.split_right_child_wextra;
        float wx = cx + 200.0f;

        float cX, cY;
        media_engine::Layout::GetCursorScreenPos(&cX, &cY);
        float scx = cx + Lc.section_card_pad;
        float scw = cw - Lc.section_card_w_extra;
        media_engine::DrawList::RoundRect(scx, cY, scw, sv.inner_h, Lc.panel_radius,
            media_engine::Colors::Beige);
        media_engine::DrawList::Text(cx + Lc.label_row_pad, cY + 10.0f * s,
            media_engine::Colors::OrangeDeep, all_ids[i].c_str());

        float iy = cY + 40.0f * s;

        descrs[i].resize(1024);
        media_engine::Layout::SetCursorScreenPos(cx + Lc.label_row_pad, iy);
        media_engine::Text::Colored(media_engine::Colors::Gray55, L.Get("description").c_str());
        iy += 22.0f * s;
        media_engine::Layout::SetCursorScreenPos(cx + Lc.label_row_pad, iy);
        media_engine::ImGuiWidget::InputTextMultiline(("##desc_" + all_ids[i]).c_str(), descrs[i].data(), descrs[i].size(), scw - Lc.label_row_pad - 16.0f, 50.0f * s, false);
        descrs[i].resize(std::strlen(descrs[i].data()));
        iy += 60.0f * s;

        prompts[i].resize(4096);
        media_engine::Layout::SetCursorScreenPos(cx + Lc.label_row_pad, iy);
        media_engine::Text::Colored(media_engine::Colors::Gray55, L.Get("personality_prompt").c_str());
        iy += 22.0f * s;
        media_engine::Layout::SetCursorScreenPos(cx + Lc.label_row_pad, iy);
        media_engine::ImGuiWidget::InputTextMultiline(("##prompt_" + all_ids[i]).c_str(), prompts[i].data(), prompts[i].size(), scw - Lc.label_row_pad - 16.0f, 70.0f * s, false);
        prompts[i].resize(std::strlen(prompts[i].data()));
        iy += 80.0f * s;

        bool ac = (auto_confirms[i] != 0);
        iy = PanelHelper::LabelRow(cx, iy, "auto-confirm", wx,
            [&](){ media_engine::ImGuiWidget::Checkbox(("##autoconf_" + all_ids[i]).c_str(), &ac); }, s);
        auto_confirms[i] = ac ? 1 : 0;

        iy = PanelHelper::LabelRow(cx, iy, L.Get("model").c_str(), wx, [&](){
            float cw_half = ((cx + cw) - wx) * 0.5f;
            auto _w = media_engine::ScopedItemWidth(cw_half);
            media_engine::ImGuiWidget::Combo(("##mdl_" + all_ids[i]).c_str(), &model_idx[i], m_cstrs.data(), (int)m_cstrs.size()); }, s);

        if (vc > 0) {
            iy = PanelHelper::LabelRow(cx, iy, L.Get("voice").c_str(), wx, [&](){
                float cw_half = ((cx + cw) - wx) * 0.5f;
                auto _w = media_engine::ScopedItemWidth(cw_half);
                media_engine::ImGuiWidget::Combo(("##voi_" + all_ids[i]).c_str(), &voice_idx[i], v_cstrs.data(), vc); }, s);
        }

        float cEX, cEY;
        media_engine::Layout::GetCursorScreenPos(&cEX, &cEY);
        float cH = cEY - cY + 4.0f;
        media_engine::DrawList::RoundRectOutline(scx, cY, scw, cH, Lc.panel_radius,
            media_engine::Colors::CreamBorder, 1.0f);
    }

buttons:
    float by = pf.a.y + pf.a.h - btn_h - 4.0f;
    float btn_w = Lc.panel_save_btn_w, btn_d = Lc.panel_btn_gap;
    float bx = pf.a.x + pf.a.w - (btn_w * 2 + btn_d) - Lc.panel_btn_right_gap;
    media_engine::Layout::SetCursorScreenPos(bx, by);
    if (media_engine::ImGuiWidget::Button(L.Get("btn_save").c_str(), btn_w, 0)) {
        std::vector<std::string> nr;
        for (size_t i = 0; i < all_ids.size(); ++i) if (checked[i]) {
            nr.push_back(all_ids[i]);
            auto d = avail_models[model_idx[i]]; auto be = d.find("] ");
            if (be != std::string::npos) {
                std::string p = d.substr(1, be - 1), m = d.substr(be + 2);
                RoleConfigManager::SaveModel(all_ids[i], p, m); RoleConfigManager::HotSwitch(all_ids[i], p, m);
            }
            if (vc > 0 && voice_idx[i] >= 0 && voice_idx[i] < vc) {
                std::string v = config.tts.voice_list[voice_idx[i]];
                RoleConfigManager::SaveTtsVoice(all_ids[i], v, config.tts.backend);
                RoleConfigManager::HotSwitchTtsVoice(all_ids[i], v, config.tts.backend);
            }
            RoleConfigManager::SaveField(all_ids[i], "description", descrs[i]);
            RoleConfigManager::SaveField(all_ids[i], "personality_prompt", prompts[i]);
            RoleConfigManager::SaveFieldBool(all_ids[i], "llm.auto_confirm_tools", auto_confirms[i] != 0);
        }
        config.default_role = nr; config.SaveToFile(); scan = true;
        for (size_t i = 0; i < all_ids.size(); ++i) if (checked[i]) {
            RoleConfigManager::HotReload(all_ids[i]);
        }
    }
    media_engine::Layout::SameLine(); media_engine::Layout::Dummy(btn_d, 0); media_engine::Layout::SameLine();
    if (media_engine::ImGuiWidget::Button(L.Get("nav_refresh").c_str(), btn_w, 0)) scan = true;
}

} // namespace prosophor
