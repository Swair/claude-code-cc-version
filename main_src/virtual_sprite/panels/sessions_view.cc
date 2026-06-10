#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/sprite.h"
#include "virtual_sprite/sprite_manager.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"

namespace prosophor {

void ChatWindow::RenderSessionsView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, L.Get("view_sessions").c_str());
    float sm = Spacing();

    {
        auto _child = PanelFrame::BeginScroll(f.a, 0, 0);

        auto& sprites = SpriteManager::GetInstance().GetAll();
        if (sprites.empty()) { media_engine::Text::Colored(media_engine::Colors::Gray55, L.Get("panel_no_data").c_str()); return; }

        float card_w = f.a.w - 24.0f;
        std::string focused_sid = SpriteManager::GetInstance().GetFocusedSession();
        for (auto& s : sprites) {
            bool focused = (s->GetSessionId() == focused_sid);
            float cx2, cy2;
            media_engine::Layout::GetCursorScreenPos(&cx2, &cy2);
            float card_h = 72.0f * sm;
            media_engine::DrawList::RoundRect(cx2, cy2, card_w, card_h, 8.0f,
                focused ? media_engine::Colors::OrangeLightest : media_engine::Colors::White);
            if (focused) media_engine::DrawList::RoundRectOutline(cx2, cy2, card_w, card_h, 8.0f,
                media_engine::Colors::OrangeWarm, 1.5f);

            float iy = cy2 + 10.0f * sm;
            iy = InfoRow(cx2 + 14.0f, iy, L.Get("sess_id").c_str(), s->GetSessionId().c_str(), 76.0f * sm);
            iy = InfoRow(cx2 + 14.0f, iy, L.Get("sess_role").c_str(), s->GetRoleId().c_str(), 76.0f * sm);
            InfoRow(cx2 + 14.0f, iy, L.Get("sess_agent_state").c_str(), "idle", 76.0f * sm);

            if (media_engine::ImGuiWidget::IconButton(("focus_" + s->GetSessionId()).c_str(),
                    focused ? "\xE2\x9C\x93" : L.Get("btn_focus").c_str(),
                    cx2 + card_w - 66.0f, cy2 + 22.0f * sm, 56.0f,
                    focused ? media_engine::Colors::OrangeLight : media_engine::Colors::CreamBorder,
                    focused ? media_engine::Colors::OrangeDeep : media_engine::Colors::Gray55, 4.0f))
                SpriteManager::GetInstance().SetFocusedSession(s->GetSessionId());
            media_engine::Layout::Dummy(0, 6.0f * sm);
        }
    }
}

} // namespace prosophor
