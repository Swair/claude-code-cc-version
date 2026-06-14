#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"

namespace prosophor {

void ChatWindow::RenderLogsView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    float sm = Spacing();
    PanelContainer f(cont_x, cont_y, cont_w, cont_h, L.Get("view_logs").c_str());

    float fy = f.a.y + 8.0f * sm;
    const char* levels[] = {L.Get("log_all").c_str(), L.Get("log_debug").c_str(), L.Get("log_info").c_str(),
                            L.Get("log_warn").c_str(), L.Get("log_error").c_str()};
    media_engine::Layout::SetCursorScreenPos(f.a.x + 14.0f, fy);
    for (int i = 0; i < 5; ++i) {
        if (i > 0) media_engine::Layout::SameLine(0, 4.0f * sm);
        float bx, by; media_engine::Layout::GetCursorScreenPos(&bx, &by);
        if (media_engine::ImGuiWidget::Button((std::string(levels[i]) + "##lf" + std::to_string(i)).c_str(), 0, 22.0f * sm))
            log_filter_ = i;
        if (log_filter_ == i) {
            float iw, ih; media_engine::ImGuiWidget::GetItemRectSize(&iw, &ih);
            media_engine::DrawList::RoundRectOutline(bx, by, iw, ih, 4.0f, media_engine::Colors::OrangeWarm, 1.5f);
        }
    }
    media_engine::Layout::SameLine(0, 8.0f * sm);
    media_engine::ImGuiWidget::Button(L.Get("log_clear").c_str(), 0, 22.0f * sm);

    float ly = fy + 28.0f * sm, lw = f.a.w - 24.0f, lh = f.a.y + f.a.h - ly - 8.0f;
    auto _dark = media_engine::ScopedColors(media_engine::Color::Slot::ChildBg, media_engine::Colors::GrayNearBlack)
        .Then(media_engine::Color::Slot::Text, media_engine::Colors::Gray86);
    media_engine::Layout::SetCursorScreenPos(f.a.x + 12.0f, ly);
    auto _log = media_engine::ScopedChild("log_area", lw, lh, 0, media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);
    if (!_log) return;

    struct Entry { const char* t; const char* l; const char* m; media_engine::Color c; };
    Entry entries[] = {
        {"12:00:01","\xe2\x84\xb9\xef\xb8\x8f INFO","Agent initialized",media_engine::Colors::Gray86},          // ℹ️
        {"12:00:02","\xe2\x84\xb9\xef\xb8\x8f INFO","Provider ready",media_engine::Colors::Gray86},
        {"12:00:05","\xe2\x9a\xa0\xef\xb8\x8f WARN","Rate limit approaching",media_engine::Colors::Amber},       // ⚠️
        {"12:01:00","\xe2\x9d\x8c ERROR","Connection timeout",media_engine::Colors::RedBright},                   // ❌
        {"12:01:05","\xe2\x84\xb9\xef\xb8\x8f INFO","Reconnected",media_engine::Colors::GreenSuccess},
        {"12:02:30","\xe2\x84\xb9\xef\xb8\x8f INFO","Message processed",media_engine::Colors::Gray86},
    };
    for (auto& e : entries) { media_engine::Text::Fmt("%s  [%s]  %s", e.t, e.l, e.m); media_engine::Layout::Dummy(0, 2.0f * sm); }
}

} // namespace prosophor
