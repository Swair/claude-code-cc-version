#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"

namespace prosophor {

void ChatWindow::RenderUsageView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    float sm = Spacing();
    PanelContainer f(cont_x, cont_y, cont_w, cont_h, L.Get("view_usage").c_str());

    float cx = f.a.x + 8.0f, cy = f.a.y + 8.0f, gap = 12.0f * sm, ch = 60.0f * sm;
    float cw = (f.a.w - 28.0f - gap * 2) / 3.0f;
    StatCard(cx, cy, cw, ch, L.Get("usage_total_tokens").c_str(), "1,234", media_engine::Colors::Orange);
    StatCard(cx + cw + gap, cy, cw, ch, L.Get("usage_input_tokens").c_str(), "890", media_engine::Colors::BlueMid);
    StatCard(cx + (cw + gap) * 2, cy, cw, ch, L.Get("usage_output_tokens").c_str(), "344", media_engine::Colors::Purple);
    cy += ch + gap;
    StatCard(cx, cy, cw, ch, L.Get("usage_est_cost").c_str(), "$0.0023", media_engine::Colors::GreenSuccess);

    cy += ch + 20.0f * sm;
    float tw = f.a.w - 24.0f, th = f.a.y + f.a.h - cy - 8.0f;
    WhiteCard(cx, cy, tw, th);
    media_engine::DrawList::Text(cx + 14.0f, cy + 10.0f, media_engine::Colors::OrangeDeep, L.Get("usage_session").c_str());
    media_engine::DrawList::Text(cx + 80.0f, cy + 10.0f, media_engine::Colors::Gray55, L.Get("usage_overall").c_str());
    float ty = cy + 34.0f * sm;
    media_engine::DrawList::Text(cx + 14.0f, ty, media_engine::Colors::Gray55, "Model");
    media_engine::DrawList::Text(cx + 120.0f, ty, media_engine::Colors::Gray55, "Input");
    media_engine::DrawList::Text(cx + 180.0f, ty, media_engine::Colors::Gray55, "Output");
    media_engine::DrawList::Text(cx + 250.0f, ty, media_engine::Colors::Gray55, "Cost");
    ty += 24.0f * sm;
    const char* rows[][4] = {{"deepseek-v4","580","210","$0.0012"},{"qwen3:8b","310","134","$0.0011"}};
    for (auto& row : rows) {
        media_engine::DrawList::Text(cx + 14.0f, ty, media_engine::Colors::Gray40, row[0]);
        media_engine::DrawList::Text(cx + 120.0f, ty, media_engine::Colors::Gray40, row[1]);
        media_engine::DrawList::Text(cx + 180.0f, ty, media_engine::Colors::Gray40, row[2]);
        media_engine::DrawList::Text(cx + 250.0f, ty, media_engine::Colors::Gray40, row[3]);
        ty += 20.0f * sm;
    }
}

} // namespace prosophor
