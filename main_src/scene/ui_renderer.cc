// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "scene/ui_renderer.h"
#include "components/chat_panel.h"
#include "ui_component/input_panel.h"
#include "components/status_bar.h"
#include "media_engine/media_engine.h"
#include "platform/platform.h"
#include "common/log_wrapper.h"

namespace prosophor {

UIRenderer& UIRenderer::Instance() {
    static UIRenderer instance;
    return instance;
}

UIRenderer::UIRenderer() = default;
UIRenderer::~UIRenderer() = default;

void UIRenderer::UpdateLayout() {
    float window_width = static_cast<float>(MediaCore::Instance().GetWindowWidth());
    float window_height = static_cast<float>(MediaCore::Instance().GetWindowHeight());

    float chat_panel_x = LayoutConfig::GetChatPanelX(window_width);
    float chat_panel_width = LayoutConfig::GetChatPanelWidth(window_width);
    float chat_panel_y = LayoutConfig::GetChatPanelY();
    float chat_panel_height = LayoutConfig::GetChatPanelHeight(window_height, input_area_height_, bottom_status_height_);

    float input_panel_x = chat_panel_x;
    float input_panel_y = chat_panel_height;
    float input_panel_width = chat_panel_width;

    float status_bar_x = 0.0f;
    float status_bar_y = window_height - bottom_status_height_ - 10.0f;
    float status_bar_width = window_width;

    if (chat_panel_) {
        chat_panel_->SetPosition(chat_panel_x, chat_panel_y);
        chat_panel_->SetSize(chat_panel_width, chat_panel_height);
    }
    if (input_panel_) {
        input_panel_->SetPosition(input_panel_x, input_panel_y);
        input_panel_->SetSize(input_panel_width, input_area_height_);
    }
    if (status_bar_) {
        status_bar_->SetPosition(status_bar_x, status_bar_y);
        status_bar_->SetSize(status_bar_width, bottom_status_height_);
    }
}

void UIRenderer::Initialize() {
    chat_panel_ = std::make_unique<ChatPanel>(0, 0, 100, 100);
    input_panel_ = std::make_unique<InputPanel>(0, 0, 100, input_area_height_);
    status_bar_ = std::make_unique<StatusBar>(0, 0, 100, bottom_status_height_);

    UpdateLayout();

    input_panel_->SetOnSubmit([this](const std::string& msg) {
        if (on_message_submit_) {
            on_message_submit_(msg);
        }
    });

    LOG_INFO("UIRenderer initialized.");
}

void UIRenderer::SetOnMessageSubmit(MessageSubmitCallback cb) {
    on_message_submit_ = cb;
}

void UIRenderer::SetStatePropsGetter(StatePropsGetter getter) {
    state_props_getter_ = getter;
    if (status_bar_) {
        status_bar_->SetStatePropsGetter(getter);
    }
}

void UIRenderer::SetSnapshotGetter(SnapshotGetter getter) {
    snapshot_getter_ = getter;
}

void UIRenderer::Render() {
    if (!visible_) return;

    UpdateLayout();

    if (chat_panel_) chat_panel_->Render();
    if (input_panel_) input_panel_->Render();
    if (status_bar_) {
        AgentRuntimeState state = AgentRuntimeState::IDLE;
        if (snapshot_getter_) {
            if (auto snap = snapshot_getter_()) {
                state = snap->state;
            }
        }
        status_bar_->Render();
        status_bar_->RenderContent(status_text_, state);
    }
}

void UIRenderer::RenderImGui() {
    if (!visible_) return;

    if (chat_panel_ && snapshot_getter_) {
        if (auto snap = snapshot_getter_()) {
            chat_panel_->RenderContent(*snap);
        } else {
            RenderSnapshot empty;
            chat_panel_->RenderContent(empty);
        }
    }
    if (input_panel_) input_panel_->RenderContent();
}

bool UIRenderer::ProcessInput(std::string& out_message) {
    if (input_panel_) {
        std::string input = input_panel_->GetText();
        if (!input.empty()) {
            input_panel_->SetText("");
            out_message = input;
            return true;
        }
    }
    return false;
}

void UIRenderer::SetStatusText(const std::string& status) {
    status_text_ = status;
}

void UIRenderer::SetVisible(bool visible) {
    visible_ = visible;
    if (chat_panel_) chat_panel_->SetVisible(visible);
    if (input_panel_) input_panel_->SetVisible(visible);
    if (status_bar_) status_bar_->SetVisible(visible);
}

void UIRenderer::RenderFloatingText(const std::string& text, float x, float y,
                                     uint8_t r, uint8_t g, uint8_t b, float alpha) {
    MediaUtil::DrawTextRect(text, x, y, 300, 14,
                             Color(r, g, b, static_cast<uint8_t>(alpha * 255)),
                             platform::kDefaultFontPath);
}

}  // namespace prosophor
