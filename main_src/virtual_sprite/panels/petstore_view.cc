#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include "platform/platform.h"

namespace prosophor {

void ChatWindow::RenderPetStoreView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    float sm = Spacing();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, L.Get("view_petstore").c_str());
    media_engine::DrawList::Text(f.a.x + 14.0f, f.a.y + 10.0f, media_engine::Colors::OrangeDeep, "Petdex");

    float iy = f.a.y + 40.0f * sm;
    media_engine::DrawList::Text(f.a.x + 14.0f, iy, media_engine::Colors::Gray40, "Browse and download desktop pets"); iy += 24.0f * sm;
    media_engine::DrawList::Text(f.a.x + 14.0f, iy, media_engine::Colors::Gray55, "https://petdex.dev/zh"); iy += 30.0f * sm;
    media_engine::Layout::SetCursorScreenPos(f.a.x + 14.0f, iy);
    if (media_engine::ImGuiWidget::Button("Open in Browser", 0, 0))
        Platform::OpenWithDefault("https://petdex.dev/zh");
}

} // namespace prosophor
