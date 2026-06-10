#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"

namespace prosophor {

void ChatWindow::RenderAboutView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, L.Get("about_title").c_str());

    float iy = f.a.y + 16.0f;
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::OrangeDeep,
        (L.Get("app_name") + " v" PROSOPHOR_VERSION).c_str()); iy += 24.0f;
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::Gray40,
        "AI Desktop Companion — Desktop Pet + LLM Chat"); iy += 24.0f;
    iy = SeparatorLine(f.a.x + 24.0f, iy, f.a.w - 48.0f);
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::Gray55, "System"); iy += 22.0f;
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::Gray40, "Version: " PROSOPHOR_VERSION); iy += 20.0f;
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::Gray40, "Renderer: SDL3 + ImGui"); iy += 24.0f;
    iy = SeparatorLine(f.a.x + 24.0f, iy, f.a.w - 48.0f);
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::Gray55, "Contact:"); iy += 22.0f;
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::Gray40, "Email: swair_fang@126.com"); iy += 20.0f;
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::Gray40, "GitHub: https://github.com/Swair"); iy += 24.0f;
    iy = SeparatorLine(f.a.x + 24.0f, iy, f.a.w - 48.0f);
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::Gray55, "License: Apache 2.0"); iy += 20.0f;
    media_engine::DrawList::Text(f.a.x + 24.0f, iy, media_engine::Colors::Gray55, "Copyright 2026 Prosophor Contributors");
}

} // namespace prosophor
