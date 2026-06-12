#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/layout_config.h"
#include "config/config.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace prosophor {

namespace {

static std::string s_sel;
static bool s_scan = true;
static std::vector<std::string> s_order;

void InitOrder(const std::unordered_map<std::string, ProviderConfig>& providers) {
    s_order.clear();
    const std::vector<std::string> preferred = {
        "anthropic", "openai", "ollama", "llamacpp"
    };
    for (auto& name : preferred)
        if (providers.find(name) != providers.end()) s_order.push_back(name);
    for (auto& [name, _] : providers)
        if (std::find(s_order.begin(), s_order.end(), name) == s_order.end())
            s_order.push_back(name);
    if (s_sel.empty() || providers.find(s_sel) == providers.end())
        s_sel = s_order.empty() ? "" : s_order[0];
}

} // anonymous namespace

void ChatWindow::RenderProvidersView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto Lc = LayoutConfig{};
    auto& L = I18n::Instance();
    float sm = Spacing();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, L.Get("view_providers").c_str(), 30.0f);

    auto& config = ProsophorConfig::GetInstance();

    if (s_scan) { InitOrder(config.llm_providers); s_scan = false; }

    float gap = 12.0f;
    float left_w = Lc.panel_left_list_w;

    auto sv = SplitView(f.a, left_w, f.btn_h, gap);

    // ── Left inner: provider list ──
    {
        media_engine::Layout::SetCursorScreenPos(sv.left_x, sv.left_y);
        auto _l = media_engine::ScopedChild("pv_list", sv.left_w, sv.inner_h, 0,
            media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);

        float cy = sv.left_y;
        for (auto& name : s_order) {
            bool act = (name == s_sel);
            float bh = Lc.split_list_item_h * sm;
            float iw = sv.left_w - 4.0f;
            media_engine::Layout::SetCursorScreenPos(sv.left_x, cy);
            if (media_engine::ImGuiWidget::InvisibleButton(
                    ("ps_" + name).c_str(), sv.left_w, bh))
                s_sel = name;
            bool hov = media_engine::ImGuiWidget::IsItemHovered();
            constexpr float kPad = 4.0f;
            if (act) {
                media_engine::DrawList::RoundRect(sv.left_x + kPad, cy, iw - kPad, bh, 4.0f,
                    media_engine::Colors::OrangeLightest);
                media_engine::DrawList::RoundRect(sv.left_x + kPad, cy, 3.0f, bh, 4.0f,
                    media_engine::Colors::Orange);
            } else if (hov) {
                media_engine::DrawList::RoundRect(sv.left_x + kPad, cy, iw - kPad, bh, 4.0f,
                    media_engine::Colors::OrangePale);
            }
            media_engine::DrawList::Text(sv.left_x + Lc.split_list_text_x * sm - Lc.split_list_item_gap * sm, cy + Lc.split_list_text_y * sm,
                act ? media_engine::Colors::OrangeDeep
                    : hov ? media_engine::Colors::Orange
                    : media_engine::Colors::Gray40,
                name.c_str());
            cy += bh + Lc.split_list_item_gap * sm;
        }
    }

    sv.DrawDivider();

    // ── Right inner: provider config ──
    {
        media_engine::Layout::SetCursorScreenPos(sv.right_x, sv.right_y);
        auto _r = media_engine::ScopedChild("pv_cfg", sv.right_w, sv.inner_h,
            0, media_engine::ImGuiWindowFlags_AlwaysVerticalScrollbar);

        auto it = config.llm_providers.find(s_sel);
        if (it == config.llm_providers.end()) {
            media_engine::Text::Colored(media_engine::Colors::Gray55, s_sel.empty()
                ? L.Get("panel_no_data").c_str()
                : (L.Get("panel_no_data") + " (" + s_sel + ")").c_str());
            return;
        }
        auto& prov = it->second;

        if (prov.entries.empty()) {
            media_engine::Text::Colored(media_engine::Colors::Gray55, "(no entries)");
            media_engine::Layout::Dummy(0, 4.0f * sm);
        }
        for (size_t ei = 0; ei < prov.entries.size(); ++ei) {
            auto& entry = prov.entries[ei];
            media_engine::Layout::Dummy(0, 6.0f * sm);

            auto _entry = media_engine::ScopedChild(
                ("##pv_entry_" + std::to_string(ei)).c_str(),
                sv.right_w - 8.0f, 0,
                media_engine::ImGuiChildFlags_Borders | media_engine::ImGuiChildFlags_AutoResizeY,
                media_engine::ImGuiWindowFlags_None);

            auto id = [&](const char* suf) {
                return "##pv_" + std::to_string(ei) + "_" + suf;
            };

            float cw = media_engine::Layout::GetContentRegionAvailWidth();
            float widget_w = (cw - 120.0f) * 2.0f / 3.0f;
            float entryFY;
            media_engine::Layout::GetCursorScreenPos(nullptr, &entryFY);
            float entryCX;
            media_engine::Layout::GetCursorScreenPos(&entryCX, nullptr);
            float entryLH = Lc.panel_widget_spacing * sm;

            auto field_row = [&](const char* label, std::function<void(float)> widget_fn) {
                media_engine::Layout::SetCursorScreenPos(entryCX + 8.0f, entryFY);
                media_engine::Text::Colored(media_engine::Colors::Gray55, label);
                media_engine::Layout::SetCursorScreenPos(entryCX + 120.0f, entryFY);
                auto _fw = media_engine::ScopedItemWidth(widget_w);
                widget_fn(widget_w);
                entryFY += entryLH;
            };

            char buf[1024];
            field_row("api_key", [&](float) {
                std::strncpy(buf, entry.api_key.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = 0;
                if (media_engine::ImGuiWidget::InputText(id("ak").c_str(), buf, sizeof(buf)))
                    entry.api_key = buf;
            });
            field_row("base_url", [&](float) {
                std::strncpy(buf, entry.base_url.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = 0;
                if (media_engine::ImGuiWidget::InputText(id("url").c_str(), buf, sizeof(buf)))
                    entry.base_url = buf;
            });
            field_row("timeout", [&](float) {
                media_engine::ImGuiWidget::InputInt(id("to").c_str(), &entry.timeout);
            });
            field_row("thinking", [&](float) {
                media_engine::ImGuiWidget::Checkbox(id("thk").c_str(), &entry.thinking);
            });

            media_engine::Layout::Dummy(0, 4.0f * sm);

            // ── Models section ──
            {
                float mCW = cw - 8.0f;
                static std::unordered_map<size_t, bool> s_models_open;
                if (s_models_open.find(ei) == s_models_open.end())
                    s_models_open[ei] = true;
                bool& m_open = s_models_open[ei];

                float sy;
                media_engine::Layout::GetCursorScreenPos(nullptr, &sy);
                float ty = sy + 2.0f * sm;
                media_engine::Layout::SetCursorScreenPos(entryCX + 8.0f, ty);
                std::string arrow_str = m_open ? " -" : " +";
                media_engine::Text::Colored(media_engine::Colors::OrangeDeep, ("models" + arrow_str).c_str());

                media_engine::Layout::SetCursorScreenPos(entryCX + 8.0f, sy);
                if (media_engine::ImGuiWidget::InvisibleButton(("##mdl_click_" + std::to_string(ei)).c_str(), cw - 8.0f, 30.0f * sm))
                    m_open = !m_open;

                if (m_open) {
                    float model_y;
                    media_engine::Layout::GetCursorScreenPos(nullptr, &model_y);
                    media_engine::Layout::SetCursorScreenPos(entryCX + 8.0f, model_y + 4.0f * sm);
                    for (auto& [mk, mv] : entry.models) {
                        auto pid = [&](const char* suf) {
                            return "##pvm_" + std::to_string(ei) + "_" + mk + "_" + suf;
                        };

                        auto _mc = media_engine::ScopedChild(
                            ("##mdl_card_" + std::to_string(ei) + "_" + mk).c_str(),
                            mCW, 0,
                            media_engine::ImGuiChildFlags_Borders | media_engine::ImGuiChildFlags_AutoResizeY,
                            media_engine::ImGuiWindowFlags_None);

                        media_engine::Layout::Dummy(0, 4.0f * sm);
                        auto _sw = media_engine::ScopedItemWidth((mCW - 50.0f) / 3.0f);

                        char mbuf[256];
                        std::strncpy(mbuf, mv.model.c_str(), sizeof(mbuf) - 1);
                        mbuf[sizeof(mbuf) - 1] = 0;
                        media_engine::Text::Colored(media_engine::Colors::Gray55, "model");
                        if (media_engine::ImGuiWidget::InputText(pid("mdl").c_str(), mbuf, sizeof(mbuf)))
                            mv.model = mbuf;

                        media_engine::Text::Colored(media_engine::Colors::Gray55, "temperature");
                        double temp = mv.temperature;
                        media_engine::ImGuiWidget::SliderFloat(pid("tmp").c_str(), &temp, 0.0f, 2.0f, "%.1f");
                        mv.temperature = static_cast<float>(temp);

                        media_engine::Text::Colored(media_engine::Colors::Gray55, "max_tokens");
                        media_engine::ImGuiWidget::InputInt(pid("mt").c_str(), &mv.max_tokens);

                        media_engine::Text::Colored(media_engine::Colors::Gray55, "context_window");
                        media_engine::ImGuiWidget::InputInt(pid("cw").c_str(), &mv.context_window);
                        media_engine::Layout::Dummy(0, 4.0f * sm);
                    }

                    float btn_y;
                    media_engine::Layout::GetCursorScreenPos(nullptr, &btn_y);
                    media_engine::Layout::SetCursorScreenPos(entryCX + cw - 140.0f - 8.0f, btn_y);
                    if (media_engine::ImGuiWidget::Button(("+ Add Model##" + std::to_string(ei)).c_str(), 140.0f, 0)) {
                        int n = (int)entry.models.size() + 1;
                        std::string new_key = "new-model-" + std::to_string(n);
                        while (entry.models.count(new_key)) { ++n; new_key = "new-model-" + std::to_string(n); }
                        ModelConfig mc;
                        mc.name = new_key;
                        mc.model = new_key;
                        entry.models[new_key] = mc;
                    }
                }
            }
        }

        media_engine::Layout::Dummy(0, 6.0f * sm);
        float add_entry_y;
        media_engine::Layout::GetCursorScreenPos(nullptr, &add_entry_y);
        media_engine::Layout::SetCursorScreenPos(sv.right_x + sv.right_w - 140.0f - 8.0f, add_entry_y);
        if (media_engine::ImGuiWidget::Button(("+ Add Entry##" + s_sel).c_str(), 140.0f, 0)) {
            ProviderEntryConfig ne;
            ne.timeout = 30;
            prov.entries.push_back(std::move(ne));
        }
    }

    SaveCancelPanel(f.a, f.btn_h,
        [&](){ config.SaveToFile(); },
        [&](){ config = ProsophorConfig::LoadFromFile(ProsophorConfig::DefaultConfigPath());
               s_scan = true; });
}

} // namespace prosophor
