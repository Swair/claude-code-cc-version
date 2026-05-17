// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "components/chat_panel.h"
#include "common/log_wrapper.h"

namespace prosophor {

ChatPanel::ChatPanel(float x, float y, float width, float height)
    : media_engine::Widget(x, y, width, height) {
    panel_ = std::make_unique<media_engine::UIPanel>(0, 0, 100, 100, media_engine::PanelStyle::ChatPanel());
    AddChild(panel_.get());
    scroll_window_ = std::make_unique<media_engine::ScrollWindow>(0, 0, 0, 0);
}

ChatPanel::~ChatPanel() = default;

void ChatPanel::OnResize() {
    Widget::OnResize();  // 级联到 panel_ 填满自身
}

void ChatPanel::Render() const {
    if (!visible_) return;
    panel_->Render();
}

void ChatPanel::Render(const media_engine::RenderContext& ctx) {
    if (!visible_) return;
    // Recurse children (panel_ draws background)
    for (auto* child : children_) {
        child->Render(ctx);
    }

    // Messages inside scroll window, positioned at panel's content area
    float win_x = 0.0f, win_y = 0.0f;
    media_engine::ImGuiGetWindowPos(&win_x, &win_y);
    scroll_window_->SetPosition(win_x + panel_->GetContentX(), win_y + panel_->GetContentY());
    scroll_window_->SetSize(panel_->GetContentWidth(), panel_->GetContentHeight());

    scroll_window_->Begin("______________________________", &media_engine::Colors::CreamLight);
    RenderMessages(snapshot_);

    // Auto-scroll to bottom when already at bottom (follow new messages)
    float scroll_max = media_engine::GetScrollMaxY();
    float scroll_y = media_engine::GetScrollY();
    if (scroll_y >= scroll_max - 10.0f || scroll_max < 1.0f) {
        media_engine::SetScrollY(scroll_max);
    }

    scroll_window_->End();
}

void ChatPanel::RenderContent(const RenderSnapshot& snapshot) {
    snapshot_ = snapshot;
    Render(media_engine::RenderContext{});
}

void ChatPanel::RenderContentInRect(float x, float y, float w, float h,
                                     const RenderSnapshot& snapshot) {
    if (!visible_) return;
    scroll_window_->SetPosition(x, y);
    scroll_window_->SetSize(w, h);
    scroll_window_->Begin("______________________________", &media_engine::Colors::CreamLight);
    RenderMessages(snapshot);
    // Always scroll to bottom for speech bubble (compact overlay)
    media_engine::SetScrollY(media_engine::GetScrollMaxY());
    scroll_window_->End();
}

void ChatPanel::RenderMessages(const RenderSnapshot& snapshot) {
    size_t index = 0;
    for (const auto& msg : snapshot.messages) {
        if (!role_filter_.empty() && msg.role != role_filter_) continue;
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
    if (!snapshot.streaming_thinking.empty() &&
        (role_filter_.empty() || role_filter_ == "thinking")) {
        RenderMessage("thinking", snapshot.streaming_thinking, index++);
    }
    if (!snapshot.streaming_text.empty() &&
        (role_filter_.empty() || role_filter_ == "assistant")) {
        RenderMessage("assistant", snapshot.streaming_text, index);
    }
}

void ChatPanel::RenderMessage(const std::string& role, const std::string& content, size_t index) {
    if (index > 0) {
        media_engine::Dummy(0, 4);
    }

    // Light theme (cream background):
    //   user     → dark gray
    //   thinking → medium gray
    //   assistant→ dark green
    //   tool     → teal
    //   error    → dark red
    media_engine::Color role_color = media_engine::Colors::Gray40;
    media_engine::Color text_color = media_engine::Colors::Gray40;

    if (role == "thinking") {
        role_color = media_engine::Colors::Gray55;
        text_color = media_engine::Colors::Gray47;
    } else if (role == "assistant") {
        role_color = media_engine::Colors::GreenLeafDark;
        text_color = media_engine::Colors::Gray40;
    } else if (role == "tool" || role == "function") {
        role_color = media_engine::Colors::Teal;
        text_color = media_engine::Colors::Teal;
    } else if (role == "error") {
        role_color = media_engine::Colors::RedDark;
        text_color = media_engine::Colors::RedDark;
    }

    if (!hide_role_labels_) {
        media_engine::ImGuiTextColored(role_color, role.c_str());
        media_engine::Dummy(0, 2);
    }
    media_engine::ImGuiTextWrapped(content.c_str(), scroll_window_->GetWidth(), text_color);
    media_engine::Dummy(0, 2);
}

void ChatPanel::ScrollToBottom() {
    scroll_window_->ScrollToBottom();
}

bool ChatPanel::IsScrolledToBottom() const {
    return scroll_window_->IsScrolledToBottom();
}

}  // namespace prosophor
