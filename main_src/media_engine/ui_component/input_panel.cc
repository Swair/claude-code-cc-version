// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "ui_component/input_panel.h"
#include "imgui_widget.h"

namespace media_engine {

InputPanel::InputPanel(float x, float y, float width, float height)
    : Widget(x, y, width, height) {
    bg_color_ = Colors::OrangeLight;
    input_text_ = std::make_unique<InputText>("##Input", "", 1024,
        [this](const std::string& /*text*/) {});
    input_text_->SetEnterReturnsTrue(true);

    send_button_ = std::make_unique<Button>(" > ", [this]() {
        std::string msg(input_text_->GetText());
        if (!msg.empty()) {
            input_text_->SetText("");
            if (on_submit_) { on_submit_(msg); }
        }
    });
    send_button_->SetBgColor(Colors::Gray70a);
    send_button_->SetHoveredColor(Colors::Gray78a);
    send_button_->SetActiveColor(Colors::Gray63);

    mic_button_ = std::make_unique<Button>("M", [this]() {
        mic_on_ = !mic_on_;
        if (mic_on_) {
            mic_button_->SetLabel("\xe2\x97\x8f");
            mic_button_->SetBgColor(Colors::RedMid);
            mic_button_->SetHoveredColor(Colors::RedDark);
            mic_button_->SetActiveColor(Colors::DarkRed);
        } else {
            mic_button_->SetLabel("M");
            mic_button_->SetBgColor(Colors::Gray70a);
            mic_button_->SetHoveredColor(Colors::Gray78a);
            mic_button_->SetActiveColor(Colors::Gray63);
        }
        if (on_mic_toggle_) on_mic_toggle_(mic_on_);
    });
    mic_button_->SetBgColor(Colors::Gray70a);
    mic_button_->SetHoveredColor(Colors::Gray78a);
    mic_button_->SetActiveColor(Colors::Gray63);
}

InputPanel::~InputPanel() = default;

void InputPanel::RenderContent() {
    if (!visible_) return;

    DrawList::Panel(x_, y_, width_, height_, corner_radius_, bg_color_, border_color_, border_width_);

    float cx = x_ + padding_;
    float input_text_h = 20.0f;
    float cy = y_ + (height_ - input_text_h) / 2.0f;

    constexpr float kBtnW = 30.0f;
    constexpr float kBtnSpacing = 4.0f;
    float input_w = width_ - padding_ * 2.0f - kBtnW * 2.0f - kBtnSpacing * 2.0f;

    ScopedItemWidth _(input_w);
    input_text_->Render(cx, cy);

    if (input_text_->IsEnterPressed()) {
        std::string msg(input_text_->GetText());
        if (!msg.empty()) {
            input_text_->SetText("");
            if (on_submit_) { on_submit_(msg); }
        }
    }

    // V/M mode toggle label
    Layout::SameLine();
    mic_button_->Render();
    Layout::SameLine();
    send_button_->Render();
}



void InputPanel::SetInputRatio(float ratio) {
    input_ratio_ = (ratio < 0.0f) ? 0.0f : (ratio > 1.0f) ? 1.0f : ratio;
}

std::string InputPanel::GetText() const {
    return input_text_->GetText();
}

void InputPanel::SetText(const std::string& text) {
    input_text_->SetText(text);
}

void InputPanel::SetOnSubmit(SubmitCallback cb) {
    on_submit_ = std::move(cb);
}

void InputPanel::SetSendButtonColor(const Color& color) {
    send_button_->SetBgColor(color);
    auto blend = [](uint8_t a, uint8_t b, float t) -> uint8_t {
        return static_cast<uint8_t>(a + (b - a) * t);
    };
    send_button_->SetHoveredColor(
        {blend(color.r, 255, 0.4f), blend(color.g, 255, 0.4f),
         blend(color.b, 255, 0.4f), color.a});
    send_button_->SetActiveColor(
        {blend(color.r, 0, 0.4f), blend(color.g, 0, 0.4f),
         blend(color.b, 0, 0.4f), color.a});
}

void InputPanel::Render(const RenderContext& /*ctx*/) {
    RenderContent();
}

}  // namespace media_engine
