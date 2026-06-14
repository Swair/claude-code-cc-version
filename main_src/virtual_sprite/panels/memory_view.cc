// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "components/panel_kit.h"
#include "components/item_list.h"
#include "virtual_sprite/layout_config.h"
#include "common/i18n.h"
#include "prosophor_core/config/config.h"
#include "media_engine/media_engine.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <sstream>

namespace fs = std::filesystem;
namespace prosophor {

namespace {

struct MemEntry {
    std::string title;      // display name
    std::string date;
    std::string source;     // "daily" / "consolidation" / "skill"
    std::string role;       // role name (for role memories)
    std::string path;
    std::string preview;
    std::string content;    // full content (loaded on demand)
};

enum MemTab { TAB_DAILY = 0, TAB_CONSOLIDATION, TAB_SKILLS, TAB_COUNT };

static std::vector<MemEntry> s_entries;
static int s_sel = -1;
static MemTab s_tab = TAB_DAILY;
static bool s_scan = true;
static std::string s_detail_content;
static std::string s_detail_title;

const char* kTabLabels[] = {"tab_daily_memory", "tab_role_memory", "tab_skills"};

void ScanDailyMemory() {
    s_entries.clear();
    auto mem_dir = fs::path(ProsophorConfig::DefaultConfigPath()).parent_path() / "memory";
    if (!fs::exists(mem_dir)) return;

    std::vector<fs::directory_entry> files;
    for (auto& e : fs::directory_iterator(mem_dir))
        if (e.is_regular_file() && e.path().extension() == ".md")
            files.push_back(e);
    std::sort(files.begin(), files.end(),
        [](auto& a, auto& b) { return a.last_write_time() > b.last_write_time(); });

    for (auto& e : files) {
        auto fname = e.path().filename().string();
        auto date = fname.substr(0, fname.size() - 3);  // strip .md

        // Read first line as preview
        std::string preview;
        std::ifstream f(e.path());
        std::getline(f, preview);
        if (preview.size() > 80) preview = preview.substr(0, 80) + "...";

        s_entries.push_back({fname, date, "daily", "", e.path().string(), preview, ""});
    }
}

void ScanRoleConsolidation() {
    s_entries.clear();
    auto base_dir = fs::path(ProsophorConfig::DefaultConfigPath()).parent_path() / "roles";
    if (!fs::exists(base_dir)) return;

    for (auto& role_dir : fs::directory_iterator(base_dir)) {
        if (!role_dir.is_directory()) continue;
        auto cons_dir = role_dir.path() / "consolidation";
        if (!fs::exists(cons_dir)) continue;

        std::string role_name = role_dir.path().filename().string();
        for (auto& e : fs::directory_iterator(cons_dir)) {
            if (!e.is_regular_file() || e.path().extension() != ".md") continue;

            std::string preview;
            std::ifstream f(e.path());
            std::getline(f, preview);
            if (preview.size() > 80) preview = preview.substr(0, 80) + "...";

            auto fname = e.path().filename().string();
            auto date = fname.substr(0, fname.size() - 3);
            s_entries.push_back({fname, date, "consolidation", role_name,
                                 e.path().string(), preview, ""});
        }
    }

    // Sort by date descending (newest first)
    std::sort(s_entries.begin(), s_entries.end(),
        [](auto& a, auto& b) { return a.date > b.date; });
}

void ScanSkills() {
    s_entries.clear();
    auto skills_dir = fs::path(ProsophorConfig::DefaultConfigPath()).parent_path() / "skills";
    if (!fs::exists(skills_dir)) return;

    for (auto& e : fs::directory_iterator(skills_dir)) {
        if (!e.is_regular_file() || e.path().extension() != ".md") continue;

        std::string preview;
        std::ifstream f(e.path());
        std::getline(f, preview);
        if (preview.size() > 80) preview = preview.substr(0, 80) + "...";

        auto fname = e.path().filename().string();
        s_entries.push_back({fname, "", "skill", "", e.path().string(), preview, ""});
    }
}

void LoadContent(int idx) {
    if (idx < 0 || idx >= (int)s_entries.size()) return;
    auto& entry = s_entries[idx];
    if (entry.content.empty()) {
        std::ifstream f(entry.path);
        entry.content.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    s_detail_content = entry.content;
    s_detail_title = entry.title;
}

} // anonymous namespace

void ChatWindow::RenderMemoryView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    auto Lc = LayoutConfig{};
    PanelContainer f(cont_x, cont_y, cont_w, cont_h, L.Get("view_memory").c_str(), 30.0f);
    float sm = Spacing();

    if (s_scan) {
        switch (s_tab) {
            case TAB_DAILY:         ScanDailyMemory(); break;
            case TAB_CONSOLIDATION: ScanRoleConsolidation(); break;
            case TAB_SKILLS:        ScanSkills(); break;
            default: break;
        }
        s_sel = -1;
        s_detail_content.clear();
        s_scan = false;
    }

    // ── Tab bar ──
    float tab_h = 28.0f * sm;
    float tab_y = f.a.y;
    float tab_x = f.a.x + 8.0f;
    for (int i = 0; i < TAB_COUNT; ++i) {
        bool act = (s_tab == i);
        auto bg = act ? media_engine::Colors::OrangeLightest : media_engine::Colors::White;
        auto brd = act ? media_engine::Colors::Orange : media_engine::Colors::CreamBorder;
        auto tc = act ? media_engine::Colors::OrangeDeep : media_engine::Colors::Gray55;
        float tw = 110.0f * sm;
        media_engine::DrawList::RoundRect(tab_x, tab_y, tw, tab_h, 4.0f, bg);
        media_engine::DrawList::RoundRectOutline(tab_x, tab_y, tw, tab_h, 4.0f, brd, 1.0f);
        media_engine::DrawList::Text(tab_x + 8.0f, tab_y + 6.0f, tc, L.Get(kTabLabels[i]).c_str());
        media_engine::Layout::SetCursorScreenPos(tab_x, tab_y);
        if (media_engine::ImGuiWidget::InvisibleButton(("mt" + std::to_string(i)).c_str(), tw, tab_h)) {
            if (s_tab != i) { s_tab = (MemTab)i; s_scan = true; }
        }
        tab_x += tw + 6.0f * sm;
    }

    // ── Split: list (left) + detail (right) ──
    float gap = 8.0f;
    float content_top = f.a.y + tab_h + 8.0f * sm;
    float content_h = f.a.h - (content_top - f.a.y) - 8.0f;
    float left_w = Lc.panel_left_list_w;
    float right_x = f.a.x + left_w + gap;
    float right_w = f.a.w - left_w - gap - 8.0f;

    // ── Left: entry list ──
    {
        media_engine::Layout::SetCursorScreenPos(f.a.x, content_top);
        auto _l = media_engine::ScopedChild("mem_list", left_w, content_h, 0,
            media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);

        ItemList list(f.a.x, content_top, left_w);
        for (int i = 0; i < (int)s_entries.size(); ++i) {
            auto& e = s_entries[i];
            std::string label;
            if (s_tab == TAB_CONSOLIDATION)
                label = "[" + e.role + "] " + e.date;
            else
                label = e.date.empty() ? e.title : e.date;

            if (!e.preview.empty())
                label += "\n" + e.preview;

            if (list.Item(("me" + std::to_string(i)).c_str(), label.c_str(), i == s_sel))
                { s_sel = i; LoadContent(i); }
        }
    }

    // ── Right: detail view ──
    {
        media_engine::Layout::SetCursorScreenPos(right_x, content_top);
        auto _r = media_engine::ScopedChild("mem_detail", right_w, content_h, 0,
            media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);

        if (s_sel < 0 || s_sel >= (int)s_entries.size()) {
            media_engine::Text::Colored(media_engine::Colors::Gray55,
                s_entries.empty() ? L.Get("panel_no_data").c_str() : "Select an entry");
        } else {
            auto& e = s_entries[s_sel];

            // Meta info card
            float x = right_x;
            float w = right_w - 8.0f;
            float meta_h = 100.0f;

            media_engine::DrawList::RoundRect(x + 4.0f, content_top + 4.0f, w, meta_h, 6.0f,
                media_engine::Colors::Beige);
            media_engine::DrawList::RoundRectOutline(x + 4.0f, content_top + 4.0f, w, meta_h,
                6.0f, media_engine::Colors::Gray63, 1.0f);

            float my = content_top + 10.0f;
            media_engine::DrawList::Text(x + 12.0f, my, media_engine::Colors::OrangeDeep,
                e.title.c_str());
            my += 22.0f;
            if (!e.date.empty()) {
                media_engine::DrawList::Text(x + 12.0f, my, media_engine::Colors::Gray55,
                    ("Date: " + e.date).c_str());
                my += 20.0f;
            }
            if (!e.role.empty()) {
                media_engine::DrawList::Text(x + 12.0f, my, media_engine::Colors::Gray55,
                    ("Role: " + e.role).c_str());
                my += 20.0f;
            }
            media_engine::DrawList::Text(x + 12.0f, my, media_engine::Colors::Gray55,
                ("Path: " + e.path).c_str());

            // Full content
            float content_y = content_top + meta_h + 12.0f;
            float content_area_h = content_h - meta_h - 20.0f;
            if (content_area_h > 60.0f) {
                media_engine::DrawList::RoundRect(x + 4.0f, content_y, w, content_area_h, 6.0f,
                    media_engine::Colors::White);
                media_engine::DrawList::RoundRectOutline(x + 4.0f, content_y, w, content_area_h,
                    6.0f, media_engine::Colors::CreamBorder, 1.0f);

                float text_x = x + 12.0f;
                float text_y = content_y + 8.0f;

                // Render content with wrapping
                media_engine::DrawList::Text(text_x, text_y, media_engine::Colors::Gray40,
                    s_detail_content.c_str());
            }
        }
    }
}

} // namespace prosophor
