#include "virtual_sprite/components/item_list.h"
#include "virtual_sprite/components/selectable_item.h"
#include "virtual_sprite/layout_config.h"
#include "media_engine/media_engine.h"

namespace prosophor {

ItemList::ItemList(float x, float y, float width, float sm)
    : x_(x), width_(width), current_y_(y), sm_(sm)
{
    auto Lc = LayoutConfig{};
    item_h_ = Lc.split_list_item_h * sm;
    gap_ = Lc.split_list_item_gap * sm;
}

bool ItemList::Item(const char* id, const char* text, bool selected,
                    bool* checked, bool disabled)
{
    float ck_w = 0;
    if (checked) {
        ck_w = 20.0f * sm_;
    }

    bool clicked = SelectableItem::Render(id, x_, current_y_,
        width_ - ck_w, item_h_, text, selected, sm_);

    if (checked) {
        float scroll_y = media_engine::Scroll::GetY();
        float cx = x_ + width_ - ck_w;
        float cy = current_y_ + 5.0f * sm_ - scroll_y;
        // Draw checkbox indicator with scroll-adjusted coordinates
        media_engine::DrawList::RoundRect(cx, cy, ck_w - 4.0f, ck_w - 4.0f, 3.0f,
            disabled ? media_engine::Colors::Gray86 : media_engine::Colors::White);
        media_engine::DrawList::RoundRectOutline(cx, cy, ck_w - 4.0f, ck_w - 4.0f, 3.0f,
            disabled ? media_engine::Colors::Gray70 : media_engine::Colors::Gray55, 1.0f);
        if (*checked) {
            media_engine::DrawList::Text(cx + 2.0f, cy + 1.0f,
                disabled ? media_engine::Colors::Gray55 : media_engine::Colors::OrangeDeep,
                "\u2713");
        }
        // Click toggle on the checkbox area (disabled = no interaction)
        if (!disabled) {
            media_engine::Layout::SetCursorScreenPos(cx, current_y_ + 5.0f * sm_);
            if (media_engine::ImGuiWidget::InvisibleButton(
                    ("##ck_" + std::string(id)).c_str(), ck_w, ck_w))
                *checked = !*checked;
        }
    }

    current_y_ += item_h_ + gap_;
    return clicked;
}

} // namespace prosophor
