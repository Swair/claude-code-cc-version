// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "components/chat_panel.h"
#include "common/log_wrapper.h"

namespace prosophor {

ChatPanel::ChatPanel(float x, float y, float width, float height) {
    container_ = std::make_unique<UIContainer>(x, y, width, height, PanelStyle::ChatPanel());
    scroll_window_ = std::make_unique<imgui_widget::ScrollWindow>(0, 0, 0, 0);
}

ChatPanel::~ChatPanel() = default;

void ChatPanel::SetPosition(float x, float y) {
    container_->SetPosition(x, y);
}

void ChatPanel::SetSize(float width, float height) {
    container_->SetSize(width, height);
}

void ChatPanel::Render() const {
    if (!visible_) return;
    container_->Render();
}

void ChatPanel::RenderContent(const RenderSnapshot& snapshot) {
    if (!visible_) return;

    container_->SetContentCallback([this, &snapshot](float /*cx*/, float /*cy*/,
                                                      float content_width, float content_height) {
        scroll_window_->SetPosition(container_->GetContentX(), container_->GetContentY());
        scroll_window_->SetSize(content_width, content_height);
        scroll_window_->Begin("______________________________", &Colors::LightBlue);

        size_t index = 0;
        for (const auto& msg : snapshot.messages) {
            bool has_thinking = false;
            for (const auto& b : msg.content) {
                if (b.type == "thinking") { has_thinking = true; break; }
            }
            if (has_thinking) {
                for (const auto& b : msg.content) {
                    if (b.type == "thinking") {
                        RenderMessage("thinking", b.text, index++);
                    } else if (b.type == "text") {
                        RenderMessage("assistant", b.text, index++);
                    }
                }
            } else {
                RenderMessage(msg.role, msg.text(), index++);
            }
        }
        if (!snapshot.streaming_thinking.empty()) {
            RenderMessage("thinking", snapshot.streaming_thinking, index++);
        }
        if (!snapshot.streaming_text.empty()) {
            RenderMessage("assistant", snapshot.streaming_text, index);
        }

        scroll_window_->End();
    });

    container_->RenderContent("ChatPanel");
}

void ChatPanel::RenderMessage(const std::string& role, const std::string& content, size_t index) {
    using namespace imgui_widget;

    if (index > 0) {
        Dummy(0, 8);
    }

    ImGuiTextColored(Colors::Orange, role.c_str());
    Dummy(0, 5);
    ImGuiTextWrapped(content.c_str(), scroll_window_->GetWidth(), Colors::Yellow);
    Dummy(0, 5);
}

void ChatPanel::ScrollToBottom() {
    scroll_window_->ScrollToBottom();
}

bool ChatPanel::IsScrolledToBottom() const {
    return scroll_window_->IsScrolledToBottom();
}

}  // namespace prosophor
