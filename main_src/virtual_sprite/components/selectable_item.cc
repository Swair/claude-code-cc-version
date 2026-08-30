#include "virtual_sprite/components/selectable_item.h"
#include "virtual_sprite/layout_config.h"

namespace prosophor {

bool SelectableItem::Render(const char* id, float x, float y, float w, float h,
                            const char* text, bool selected, float sm)
{
    auto Lc = LayoutConfig{};
    media_engine::Layout::SetCursorScreenPos(x, y);
    bool clicked = media_engine::ImGuiWidget::InvisibleButton(id, w, h);
    bool hov = media_engine::ImGuiWidget::IsItemHovered();

    // Adjust SDL draw positions by the parent child window's scroll offset
    float scroll_y = media_engine::Scroll::GetY();
    float dy = y - scroll_y;

    float iw = w - 4.0f;
    constexpr float kPad = 4.0f;
    if (selected) {
        media_engine::DrawList::RoundRect(x + kPad, dy, iw - kPad, h, 4.0f,
            media_engine::Colors::OrangeLightest);
        media_engine::DrawList::RoundRect(x + kPad, dy, 3.0f, h, 4.0f,
            media_engine::Colors::Orange);
    } else if (hov) {
        media_engine::DrawList::RoundRect(x + kPad, dy, iw - kPad, h, 4.0f,
            media_engine::Colors::OrangePale);
    }

    float tx = x + (Lc.split_list_text_x - Lc.split_list_item_gap) * sm;
    float ty = dy + Lc.split_list_text_y * sm;
    media_engine::DrawList::Text(tx, ty,
        selected ? media_engine::Colors::OrangeDeep
                 : hov ? media_engine::Colors::Orange
                 : media_engine::Colors::Gray40,
        text);
    return clicked;
}

} // namespace prosophor
