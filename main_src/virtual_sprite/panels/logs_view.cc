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
    return media_engine::Colors::Gray86;
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

/// Extract log level from a formatted spdlog line:
///   [2026-06-09 22:02:06.723] [prosophor] [info] message
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
    if (media_engine::ImGuiWidget::Button(L.Get("log_clear").c_str(), 0, 22.0f * sm)) {
        log_cache_ = LogCache{};
    }

    // ---- Read from ring buffer (real-time, no file I/O) ----
    auto ring = GetLogRingSink();
    auto formatted = ring->last_formatted();

    // Rebuild cache if snapshot changed
    if (!formatted.empty()) {
        if (log_cache_.last_raw_payload != formatted.back()) {
            log_cache_.last_raw_payload = formatted.back();
            log_cache_.entries.clear();
            log_cache_.entries.reserve(formatted.size());
            for (auto& line : formatted) {
                log_cache_.entries.push_back({
                    line,
                    ExtractLevel(line),
                    LevelColor(ExtractLevel(line))
                });
            }
        }
    }

    // ---- Render with level filter + selectable text ----
    float ly = fy + 28.0f * sm;
    float lw = f.a.w - 24.0f;
    float lh = f.a.y + f.a.h - ly - 8.0f;

    int target = filter_target[log_filter_];
    bool any_shown = false;
    for (auto& e : log_cache_.entries) {
        if (target > 0 && LevelPriority(e.level) != target) continue;
        any_shown = true;
        break;
    }

    if (any_shown) {
        media_engine::Area scroll_area = {f.a.x + 12.0f, ly, lw, lh};
        auto _dark = media_engine::ScopedColors(
            media_engine::Color::Slot::ChildBg, media_engine::Colors::GrayNearBlack)
            .Then(media_engine::Color::Slot::Text, media_engine::Colors::Gray86);
        auto _scrollbar = media_engine::ScopedStyleVar::ScrollbarSize(8.0f);
        auto _log = PanelContainer::BeginScroll(scroll_area, 0, 0, true);
        if (_log) {
            for (auto& e : log_cache_.entries) {
                if (target > 0 && LevelPriority(e.level) != target) continue;
                auto _c = media_engine::ScopedColors(
                    media_engine::Color::Slot::Text, e.color);
                media_engine::Text::Fmt("%s", e.raw.c_str());
                media_engine::Layout::Dummy(0, 2.0f * sm);
            }
        }
    } else {
        media_engine::Layout::SetCursorScreenPos(f.a.x + 12.0f, ly);
        auto _empty = media_engine::ScopedColors(
            media_engine::Color::Slot::Text, media_engine::Colors::Gray70);
        media_engine::Text::Fmt("  %s", L.Get("log_no_entries").c_str());
    }
}

} // namespace prosophor
