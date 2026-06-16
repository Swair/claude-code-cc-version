#include "components/item_list.h"
#include "components/selectable_item.h"
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
                    bool* checked)
{
    float ck_w = 0;
    if (checked) {
        ck_w = 20.0f * sm_;
    }

    bool clicked = SelectableItem::Render(id, x_, current_y_,
        width_ - ck_w, item_h_, text, selected, sm_);

    if (checked) {
        media_engine::Layout::SetCursorScreenPos(
            x_ + width_ - ck_w, current_y_ + 5.0f * sm_);
        media_engine::ImGuiWidget::Checkbox(
            ("##ck_" + std::string(id)).c_str(), checked);
    }

    current_y_ += item_h_ + gap_;
    return clicked;
}

} // namespace prosophor
