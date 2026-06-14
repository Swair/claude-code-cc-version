#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/layout_config.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include <vector>
#include <string>
#include <cstring>

namespace prosophor {

namespace {

struct KbEntry {
    std::string title;
    std::string content;
    bool expanded = false;
};

struct KbCategory {
    std::string name;
    bool expanded = false;
    std::vector<KbEntry> entries;
};

static std::vector<KbCategory> s_categories;
static bool s_scan = true;

void ScanKnowledgeBase() {
    s_categories.clear();
    s_categories.push_back({"系统提示词", false, {
        {"默认角色提示词", "你是一位全能型 AI 助手，具备所有可用的技能。"},
        {"代码审查提示词", "请对以下代码进行审查，关注安全性、性能和可维护性。"},
    }});
    s_categories.push_back({"常用命令", false, {
        {"Git 提交规范", "feat: 新功能\\nfix: 修复\\ndocs: 文档\\nrefactor: 重构"},
        {"Docker 常用命令", "docker ps\\ndocker compose up -d\\ndocker logs -f"},
    }});
    s_categories.push_back({"配置参考", false, {
        {"LLM 提供商配置", "支持 Anthropic、OpenAI、Ollama、llama.cpp"},
        {"权限级别说明", "auto: 自动允许\\nallow: 手动确认\\ndeny: 拒绝执行"},
    }});
    s_scan = false;
}

} // anonymous namespace

void ChatWindow::RenderKnowledgeView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto Lc = LayoutConfig{};
    auto& L = I18n::Instance();
    PanelContainer f(cont_x, cont_y, cont_w, cont_h, L.Get("view_knowledge").c_str());

    if (s_scan) ScanKnowledgeBase();

    float btn_h = 30.0f, gap = 12.0f;
    {
        auto _child = PanelContainer::BeginScroll(f.a, btn_h, gap);
        float cx = f.a.x + 8.0f;
        float iy = f.a.y + 8.0f;

        for (auto& cat : s_categories) {
            media_engine::DrawList::RoundRect(cx, iy, f.a.w - 16.0f, 32.0f, Lc.rounding_small,
                media_engine::Colors::Beige);
            media_engine::DrawList::RoundRectOutline(cx, iy, f.a.w - 16.0f, 32.0f, Lc.rounding_small,
                media_engine::Colors::Gray63, 1.0f);
            media_engine::DrawList::Text(cx + 12.0f, iy + 8.0f,
                media_engine::Colors::OrangeDeep, cat.expanded ? "- " : "+ ");
            media_engine::DrawList::Text(cx + 28.0f, iy + 8.0f,
                media_engine::Colors::Gray40, cat.name.c_str());

            media_engine::Layout::SetCursorScreenPos(cx, iy);
            auto cat_id = "##kb_cat_" + cat.name;
            if (media_engine::ImGuiWidget::InvisibleButton(cat_id.c_str(), f.a.w - 16.0f, 32.0f)) {
                cat.expanded = !cat.expanded;
            }
            iy += 36.0f;

            if (cat.expanded) {
                for (auto& entry : cat.entries) {
                    float entry_x = cx + 16.0f;
                    float entry_w = f.a.w - 32.0f;

                    media_engine::DrawList::RoundRect(entry_x, iy, entry_w, 28.0f, Lc.rounding_small,
                        media_engine::Colors::White);
                    media_engine::DrawList::RoundRectOutline(entry_x, iy, entry_w, 28.0f, Lc.rounding_small,
                        media_engine::Colors::Gray63, 1.0f);
                    media_engine::DrawList::Text(entry_x + 10.0f, iy + 6.0f,
                        media_engine::Colors::Gray55, entry.title.c_str());
                    iy += 32.0f;

                    if (entry.expanded) {
                        char buf[4096];
                        std::strncpy(buf, entry.content.c_str(), sizeof(buf) - 1);
                        buf[sizeof(buf) - 1] = 0;

                        media_engine::Layout::SetCursorScreenPos(entry_x + 4.0f, iy);
                        auto _w = media_engine::ScopedItemWidth(entry_w - 8.0f);
                        media_engine::ImGuiWidget::InputTextMultiline(
                            ("##kb_cont_" + entry.title).c_str(), buf, sizeof(buf),
                            entry_w - 8.0f, 80.0f, false);
                        entry.content = buf;
                        iy += 90.0f;
                    }

                    media_engine::Layout::SetCursorScreenPos(entry_x, iy - 32.0f);
                    auto eid = "##kb_ent_" + cat.name + "_" + entry.title;
                    if (media_engine::ImGuiWidget::InvisibleButton(eid.c_str(), entry_w, 28.0f)) {
                        entry.expanded = !entry.expanded;
                    }
                }
            }
        }
    }

    float by = f.a.y + f.a.h - btn_h - 4.0f;
    media_engine::Layout::SetCursorScreenPos(f.a.x + f.a.w - 90.0f, by);
    if (media_engine::ImGuiWidget::Button(L.Get("nav_refresh").c_str(), Lc.panel_save_btn_w, 0)) {
        s_scan = true;
    }
}

} // namespace prosophor
