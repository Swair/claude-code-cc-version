#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "config/config.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"

namespace prosophor {

void ChatWindow::RenderStatusView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, L.Get("view_status").c_str());

    float cx = f.a.x + 8.0f, cy = f.a.y + 8.0f;
    float cw = (f.a.w - 28.0f) / 2.0f, ch = 60.0f;
    StatCard(cx, cy, cw, ch, "Service", "Running", media_engine::Colors::GreenSuccess);
    StatCard(cx + cw + 12.0f, cy, cw, ch, "Sessions", "1 active", media_engine::Colors::Orange);
    cy += ch + 12.0f;
    StatCard(cx, cy, cw, ch, "Config", "OK", media_engine::Colors::BlueMid);
    StatCard(cx + cw + 12.0f, cy, cw, ch, "Memory", "OK", media_engine::Colors::Purple);

    cy += ch + 20.0f;
    float list_w = f.a.w - 24.0f, list_h = f.a.y + f.a.h - cy - 12.0f;
    media_engine::DrawList::RoundRect(cx, cy, list_w, list_h, 6.0f, media_engine::Colors::White);
    media_engine::DrawList::RoundRectOutline(cx, cy, list_w, list_h, 6.0f, media_engine::Colors::CreamBorder, 1.0f);
    media_engine::DrawList::Text(cx + 14.0f, cy + 10.0f, media_engine::Colors::Gray55, "Configured Providers");

    auto& config = ProsophorConfig::GetInstance();
    float py = cy + 34.0f;
    for (auto& [pname, prov] : config.llm_providers) {
        media_engine::DrawList::CircleFilled(cx + 16.0f, py + 6.0f, 4.0f, media_engine::Colors::GreenSuccess);
        media_engine::DrawList::Text(cx + 28.0f, py, media_engine::Colors::Gray40,
            (pname + " (" + std::to_string(prov.entries.size()) + " endpoints)").c_str());
        py += 22.0f;
    }
}

} // namespace prosophor
