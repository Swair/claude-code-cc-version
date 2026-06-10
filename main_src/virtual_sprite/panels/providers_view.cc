#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/layout_config.h"
#include "config/config.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include <cstring>
#include <string>
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
    float left_w = 140.0f;

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
            if (act)
                media_engine::DrawList::Selection(sv.left_x, cy, iw, bh, 3.0f,
                    media_engine::Colors::Orange, media_engine::Colors::OrangeLightest, 4.0f);
            else if (hov)
                media_engine::DrawList::RoundRect(sv.left_x, cy, iw, bh, 4.0f,
                    media_engine::Colors::OrangePale);
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
            auto id = [&](const char* suf) {
                return "##pv_" + std::to_string(ei) + "_" + suf;
            };

            media_engine::Layout::Dummy(0, 4.0f * sm);
            media_engine::ImGuiWidget::Separator();

            media_engine::Text::Colored(media_engine::Colors::Gray55, "API Key");
            char buf[1024];
            std::strncpy(buf, entry.api_key.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = 0;
            if (media_engine::ImGuiWidget::InputText(id("ak").c_str(), buf, sizeof(buf)))
                entry.api_key = buf;

            media_engine::Text::Colored(media_engine::Colors::Gray55, "Base URL");
            std::strncpy(buf, entry.base_url.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = 0;
            if (media_engine::ImGuiWidget::InputText(id("url").c_str(), buf, sizeof(buf)))
                entry.base_url = buf;

            media_engine::Text::Colored(media_engine::Colors::Gray55, "Timeout (s)");
            media_engine::ImGuiWidget::InputInt(id("to").c_str(), &entry.timeout);

            media_engine::Text::Colored(media_engine::Colors::Gray55, "Thinking");
            media_engine::ImGuiWidget::Checkbox(id("thk").c_str(), &entry.thinking);

            // ── Models (gray card container, matching Config style) ──
            if (!entry.models.empty()) {

                float mCX2, mCY2;
                media_engine::Layout::GetCursorScreenPos(&mCX2, &mCY2);
                float mCW = sv.right_w - Lc.section_card_right_margin + Lc.split_right_child_wextra;
                float mCardX = sv.right_x;
                media_engine::DrawList::RoundRect(mCardX, mCY2, mCW, 200.0f, Lc.panel_radius,
                    media_engine::Colors::Beige);
                media_engine::DrawList::Text(mCardX + 14.0f, mCY2 + 10.0f,
                    media_engine::Colors::OrangeDeep, "Models");

                media_engine::Layout::SetCursorScreenPos(mCardX + 14.0f, mCY2 + 36.0f);
                for (auto& [mk, mv] : entry.models) {
                    auto pid = [&](const char* suf) {
                        return "##pvm_" + std::to_string(ei) + "_" + mk + "_" + suf;
                    };

                    media_engine::Text::Colored(media_engine::Colors::Gray40, mk.c_str());

                    auto _sw = media_engine::ScopedItemWidth((mCW - 28.0f) * 0.5f);
                    media_engine::Text::Colored(media_engine::Colors::Gray55, "Temperature");
                    double temp = mv.temperature;
                    media_engine::ImGuiWidget::SliderFloat(pid("tmp").c_str(), &temp, 0.0f, 2.0f, "%.1f");
                    mv.temperature = static_cast<float>(temp);

                    media_engine::Text::Colored(media_engine::Colors::Gray55, "Max Tokens");
                    media_engine::ImGuiWidget::InputInt(pid("mt").c_str(), &mv.max_tokens);

                    media_engine::Text::Colored(media_engine::Colors::Gray55, "Context Window");
                    media_engine::ImGuiWidget::InputInt(pid("cw").c_str(), &mv.context_window);

                    media_engine::Layout::Dummy(0, 4.0f * sm);
                }

                float mEX2, mEY2;
                media_engine::Layout::GetCursorScreenPos(&mEX2, &mEY2);
                float mCH = mEY2 - mCY2 + 4.0f;
                media_engine::DrawList::RoundRectOutline(mCardX, mCY2, mCW, mCH, Lc.panel_radius,
                    media_engine::Colors::Gray63, 1.0f);
            }
        }

        media_engine::Layout::Dummy(0, 4.0f * sm);
        if (media_engine::ImGuiWidget::Button(("+ Add Entry##" + s_sel).c_str(), 0, 0)) {
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
