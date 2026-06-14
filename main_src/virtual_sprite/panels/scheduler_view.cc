#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "components/panel_kit.h"
#include "common/i18n.h"
#include "prosophor_core/services/cron_scheduler.h"

namespace prosophor {

void ChatWindow::RenderSchedulerView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    PanelContainer f(cont_x, cont_y, cont_w, cont_h, L.Get("view_scheduler").c_str());
    float sm = Spacing();

    auto& cron = CronScheduler::GetInstance();
    auto tasks = cron.ListTasks();

    {
        auto _child = PanelContainer::BeginScroll(f.a, 0, 4.0f);
        float cx = f.a.x + 8.0f, cw = f.a.w - 24.0f;
        float y = f.a.y + 8.0f;

        if (tasks.empty()) {
            Card(cx, y, cw, L.Get("panel_no_data").c_str(), sm);
            return;
        }

        for (auto& t : tasks) {
            std::string title = t.description.empty() ? t.id : t.description;
            {
                Card card(cx, y, cw, title.c_str(), sm);
                y = card.Y();
                std::string info = t.cron_expression + " | " + (t.recurring ? "recurring" : "one-shot");
                media_engine::DrawList::Text(cx + 38.0f, y,
                    media_engine::Colors::Gray55, info.c_str());
                card.Advance(22.0f * sm);
                y = card.Y();
            }
            y += 4.0f * sm;
        }
    }
}

} // namespace prosophor
