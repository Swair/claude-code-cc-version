#include <cstring>
#include "virtual_sprite/panels/model_card.h"
#include "virtual_sprite/panels/panel_helpers.h"

namespace prosophor {

ModelCard::ModelCard(ModelConfig& config,
                     size_t entry_idx,
                     const std::string& model_key,
                     float container_width,
                     float sm)
    : config_(config)
    , entry_idx_(entry_idx)
    , model_key_(model_key)
    , container_width_(container_width)
    , sm_(sm) {}

std::string ModelCard::Id(const char* suf) const {
    return "##pvm_" + std::to_string(entry_idx_) + "_" + model_key_ + "_" + suf;
}

void ModelCard::Render() {
    static const auto bg = media_engine::Colors::BluePale;
    auto _pad = media_engine::ScopedStyleVar::WindowPadding(8.0f * sm_, 4.0f * sm_);
    BorderedContainer container(
        ("##mdl_card_" + std::to_string(entry_idx_) + "_" + model_key_).c_str(),
        container_width_, &bg);

    float widget_w = (container_width_ - 120.0f - 28.0f * sm_) * 0.5f;
    auto row = [&](const char* label, std::function<void(float)> widget_fn) {
        media_engine::Text::Colored(media_engine::Colors::Gray55, label);
        media_engine::Layout::SameLine(0, 12.0f * sm_);
        auto _w = media_engine::ScopedItemWidth(widget_w);
        if (widget_fn) widget_fn(widget_w);
    };

    char mbuf[256];
    std::strncpy(mbuf, config_.model.c_str(), sizeof(mbuf) - 1);
    mbuf[sizeof(mbuf) - 1] = 0;
    row("model", [&](float) {
        if (media_engine::ImGuiWidget::InputText(Id("mdl").c_str(), mbuf, sizeof(mbuf)))
            config_.model = mbuf;
    });

    double temp = config_.temperature;
    row("temperature", [&](float) {
        media_engine::ImGuiWidget::SliderFloat(Id("tmp").c_str(), &temp, 0.0f, 2.0f, "%.1f");
    });
    config_.temperature = static_cast<float>(temp);

    row("max_tokens", [&](float) {
        media_engine::ImGuiWidget::InputInt(Id("mt").c_str(), &config_.max_tokens);
    });

    row("context_window", [&](float) {
        media_engine::ImGuiWidget::InputInt(Id("cw").c_str(), &config_.context_window);
    });
}

} // namespace prosophor
