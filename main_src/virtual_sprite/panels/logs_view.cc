// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include "common/log_wrapper.h"

namespace prosophor {

static media_engine::Color LevelColor(const std::string& level) {
    if (level == "ERROR" || level == "CRITICAL") return media_engine::Colors::RedBright;
    if (level == "WARN")                         return media_engine::Colors::Amber;
    if (level == "DEBUG" || level == "TRACE")     return media_engine::Colors::Gray70;
    return media_engine::Colors::White;
}

static int LevelPriority(const std::string& level) {
    if (level == "TRACE")    return 0;
    if (level == "DEBUG")    return 1;
    if (level == "INFO")     return 2;
    if (level == "WARN")     return 3;
    if (level == "ERROR")    return 4;
    if (level == "CRITICAL") return 5;
    return 2;
}

static std::string NormalizeLevel(const std::string& raw) {
    if (raw == "warning")  return "WARN";
    if (raw == "info")     return "INFO";
    if (raw == "error")    return "ERROR";
    if (raw == "debug")    return "DEBUG";
    if (raw == "critical") return "CRITICAL";
    if (raw == "trace")    return "TRACE";
    return raw;
}

static std::string ExtractLevel(const std::string& line) {
    auto br1 = line.find(']');
    if (br1 == std::string::npos) return "INFO";
    auto br2 = line.find(']', br1 + 1);
    if (br2 == std::string::npos) return "INFO";
    auto br3 = line.find('[', br2 + 1);
    if (br3 == std::string::npos) return "INFO";
    auto br4 = line.find(']', br3 + 1);
    if (br4 == std::string::npos) return "INFO";
    return NormalizeLevel(line.substr(br3 + 1, br4 - br3 - 1));
}

void ChatWindow::RenderLogsView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    float sm = Spacing();
    PanelContainer f(cont_x, cont_y, cont_w, cont_h, L.Get("view_logs").c_str());

    float fy = f.a.y + 8.0f * sm;
    const char* level_names[] = {L.Get("log_all").c_str(), L.Get("log_debug").c_str(), L.Get("log_info").c_str(),
                                 L.Get("log_warn").c_str(), L.Get("log_error").c_str()};
    const int filter_target[] = {0, 1, 2, 3, 4};

    media_engine::Layout::SetCursorScreenPos(f.a.x + 14.0f, fy);
    for (int i = 0; i < 5; ++i) {
        if (i > 0) media_engine::Layout::SameLine(0, 4.0f * sm);
        float bx, by; media_engine::Layout::GetCursorScreenPos(&bx, &by);
        if (media_engine::ImGuiWidget::Button((std::string(level_names[i]) + "##lf" + std::to_string(i)).c_str(), 0, 22.0f * sm))
            log_filter_ = i;
        if (log_filter_ == i) {
            float iw, ih; media_engine::ImGuiWidget::GetItemRectSize(&iw, &ih);
            media_engine::DrawList::RoundRectOutline(bx, by, iw, ih, 4.0f, media_engine::Colors::OrangeWarm, 1.5f);
        }
    }
    media_engine::Layout::SameLine(0, 8.0f * sm);
    if (media_engine::ImGuiWidget::Button(L.Get("log_copy").c_str(), 0, 22.0f * sm)) {
        auto ring = GetLogRingSink();
        auto formatted = ring->last_formatted();
        int target = filter_target[log_filter_];
        std::string copy_text;
        for (auto& line : formatted) {
            if (target > 0 && LevelPriority(ExtractLevel(line)) != target) continue;
            copy_text += line;
        }
        media_engine::ImGuiWidget::SetClipboardText(copy_text.c_str());
    }
    media_engine::Layout::SameLine(0, 4.0f * sm);
    if (media_engine::ImGuiWidget::Button(L.Get("log_clear").c_str(), 0, 22.0f * sm)) {
        log_clear_anchor_ = GetLogRingSink()->last_formatted().size();
    }

    // ---- Read from ring buffer ----
    auto ring = GetLogRingSink();
    auto formatted = ring->last_formatted();

    size_t start_idx = 0;
    if (log_clear_anchor_ > 0) {
        if (log_clear_anchor_ < formatted.size())
            start_idx = formatted.size() - log_clear_anchor_;
        else
            start_idx = formatted.size();
    }

    // ---- Render scrollable log area with per-line colored Selectable ----
    float ly = fy + 28.0f * sm;
    float lw = f.a.w;
    float lh = f.a.y + f.a.h - ly - 8.0f;

    int target = filter_target[log_filter_];

    // Check if any entries pass the filter
    bool any_shown = false;
    for (size_t i = start_idx; i < formatted.size(); ++i) {
        if (target > 0 && LevelPriority(ExtractLevel(formatted[i])) != target) continue;
        any_shown = true;
        break;
    }

    if (any_shown) {
        media_engine::Area scroll_area = {f.a.x, ly, lw, lh};
        auto _dark = media_engine::ScopedColors(
            media_engine::Color::Slot::ChildBg, media_engine::Colors::GrayNearBlack)
            .Then(media_engine::Color::Slot::Text, media_engine::Colors::White);
        auto _scrollbar = media_engine::ScopedStyleVar::ScrollbarSize(8.0f);
        auto _log = PanelContainer::BeginScroll(scroll_area, 0, 0, true);
        if (_log) {
            for (size_t i = start_idx; i < formatted.size(); ++i) {
                auto& line = formatted[i];
                std::string level = ExtractLevel(line);
                if (target > 0 && LevelPriority(level) != target) continue;

                auto _c = media_engine::ScopedColors(
                    media_engine::Color::Slot::Text, LevelColor(level));
                media_engine::ID::Push(("l" + std::to_string(i)).c_str());
                media_engine::ImGuiWidget::Selectable(line.c_str());
                media_engine::ID::Pop();
            }

            // Auto-scroll
            float sy = media_engine::Scroll::GetY();
            float smax = media_engine::Scroll::GetMaxY();
            if (smax > 0 && sy >= smax - 10.0f)
                media_engine::Scroll::SetY(smax);
        }
    } else {
        media_engine::Layout::SetCursorScreenPos(f.a.x, ly);
        auto _empty = media_engine::ScopedColors(
            media_engine::Color::Slot::Text, media_engine::Colors::Gray70);
        media_engine::Text::Fmt("  %s", L.Get("log_no_entries").c_str());
    }
}

} // namespace prosophor
