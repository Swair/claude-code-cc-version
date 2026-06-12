#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/layout_config.h"
#include "config/config.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include "voice/voice_engine.h"

namespace prosophor {

void ChatWindow::RenderTtsView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, L.Get("view_tts").c_str(), 30.0f);

    auto Lc = LayoutConfig{};
    auto& config = ProsophorConfig::GetInstance();
    float s = Spacing();
    float iy;
    {
        auto _child = PanelFrame::BeginScroll(f.a, f.btn_h);
        float cx = f.a.x, cw = f.a.w - Lc.section_card_right_margin, wx = cx + Lc.card_content_indent + Lc.card_widget_offset;

        // ── TTS ──
        PanelHelper::SectionCard(cx, f.a.y + 8.0f, cw, 200.0f * s, L.Get("tab_tts").c_str());
        iy = f.a.y + 50.0f;
        iy = PanelHelper::LabelRow(cx, iy, L.Get("tts_enabled").c_str(), wx,
            [&](){ media_engine::ImGuiWidget::Checkbox("##tts_en", &config.tts.enabled); }, s);
        iy = PanelHelper::LabelRow(cx, iy, L.Get("tts_backend").c_str(), wx, [&](){
            float cw_half = ((cx + cw) - wx) * 0.5f;
            auto _w = media_engine::ScopedItemWidth(cw_half);
            const char* be[] = {"edge-tts"}; int bi = 0; media_engine::ImGuiWidget::Combo("##tts_be", &bi, be, 1); }, s);
        iy = PanelHelper::LabelRow(cx, iy, "Voice", wx, [&](){
            float cw_half = ((cx + cw) - wx) * 0.5f;
            auto _w = media_engine::ScopedItemWidth(cw_half);
            auto& vl = config.tts.voice_list;
            std::vector<const char*> vc; for (auto& v : vl) vc.push_back(v.c_str());
            int vi = 0; for (int i = 0; i < (int)vl.size(); ++i) { if (config.tts.voice == vl[i]) { vi = i; break; } }
            if (media_engine::ImGuiWidget::Combo("##tts_v", &vi, vc.data(), (int)vc.size()))
                if (vi >= 0 && vi < (int)vl.size()) config.tts.voice = vl[vi];
        }, s);
        iy += 8.0f * s;
        media_engine::Layout::SetCursorScreenPos(wx, iy);
        static char test_text[256] = "";
        { float input_w = std::max(60.0f, ((cx + cw) - wx - 52.0f - 4.0f) * 0.5f);
          auto _w = media_engine::ScopedItemWidth(input_w);
          media_engine::ImGuiWidget::InputText("##tts_test", test_text, sizeof(test_text)); }
        media_engine::Layout::SameLine();
        if (media_engine::ImGuiWidget::Button("Speak", 0, 0))
            VoiceEngine::GetInstance().Speak(test_text, config.tts.backend, config.tts.voice);

        // ── ASR ──
        iy += 30.0f * s;
        PanelHelper::SectionCard(cx, iy, cw, 120.0f * s, L.Get("tab_asr").c_str());
        iy += 40.0f;
        iy = PanelHelper::LabelRow(cx, iy, "enabled", wx,
            [&](){ media_engine::Layout::Dummy(8.0f * s, 0); media_engine::Layout::SameLine(); media_engine::ImGuiWidget::Checkbox("##asr_en", &config.asr.enabled); }, s);
        iy = PanelHelper::LabelRow(cx, iy, "push_to_talk", wx,
            [&](){ media_engine::Layout::Dummy(8.0f * s, 0); media_engine::Layout::SameLine(); media_engine::ImGuiWidget::Checkbox("##asr_ptt", &config.asr.push_to_talk); }, s);
        iy = PanelHelper::LabelRow(cx, iy, "server_url", wx, [&](){
            char buf[512]; std::strncpy(buf, config.asr.server_url.c_str(), sizeof(buf)-1); buf[sizeof(buf)-1] = 0;
            if (media_engine::ImGuiWidget::InputText("##asr_url", buf, sizeof(buf))) config.asr.server_url = buf;
        }, s);
    }

    SaveCancelPanel(f.a, f.btn_h,
        [&](){ config.SaveToFile(); },
        [&](){ config = ProsophorConfig::LoadFromFile(ProsophorConfig::DefaultConfigPath()); });
}

} // namespace prosophor
