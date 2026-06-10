#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "common/i18n.h"

namespace prosophor {

void ChatWindow::RenderSchedulerView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    PlaceholderView(cont_x, cont_y, cont_w, cont_h, L.Get("view_scheduler").c_str(), "", {
        "No scheduled tasks configured.",
    });
}

} // namespace prosophor
