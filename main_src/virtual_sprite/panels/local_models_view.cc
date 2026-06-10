#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/layout_config.h"
#include "config/config.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include "platform/platform.h"
#include <cstring>

namespace prosophor {

void ChatWindow::RenderLocalModelsView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, L.Get("view_local_models").c_str(), 30.0f);

    auto& config = ProsophorConfig::GetInstance();

    if (config.llamacpp_models.empty()) {
        media_engine::DrawList::Text(f.a.x + 14.0f, f.a.y + 22.0f, media_engine::Colors::Gray55, L.Get("panel_no_data").c_str());
        return;
    }

    auto& lm = config.llamacpp_models[0];
    float s = Spacing();
    float iy;
    {
        SectionForm sf(f.a, L.Get("nav_local_models").c_str(), 140.0f * s, f.btn_h);
        iy = sf.iy;
        iy = PanelHelper::LabelRow(sf.cx, iy, L.Get("local_model_model_path").c_str(), sf.wx, [&](){
            char buf[512]; std::strncpy(buf, lm.model_path.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1] = 0;
            float card_right = sf.cx + f.a.w - LayoutConfig{}.section_card_right_margin;
            float input_w = std::max(60.0f, (card_right - sf.wx - 36.0f - 4.0f) * 0.5f);
            { auto _w = media_engine::ScopedItemWidth(input_w);
              media_engine::ImGuiWidget::InputText("##lm_p", buf, sizeof(buf)); }
            media_engine::Layout::SameLine();
            if (media_engine::ImGuiWidget::Button("...##b", 36.0f, 0)) {
                auto p = Platform::BrowseForFile("GGUF Model (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0");
                if (!p.empty()) lm.model_path = p;
            }
        }, s);
        iy = PanelHelper::LabelRow(sf.cx, iy, L.Get("local_model_auto_start").c_str(), sf.wx,
            [&](){ media_engine::ImGuiWidget::Checkbox("##lm_as", &lm.auto_start); }, s);
        iy = PanelHelper::LabelRow(sf.cx, iy, L.Get("local_model_start_timeout").c_str(), sf.wx,
            [&](){ media_engine::ImGuiWidget::InputInt("##lm_to", &lm.start_timeout_ms); }, s);
    }

    SaveCancelPanel(f.a, f.btn_h,
        [&](){ config.SaveToFile(); },
        [&](){ config = ProsophorConfig::LoadFromFile(ProsophorConfig::DefaultConfigPath()); });
}

} // namespace prosophor
