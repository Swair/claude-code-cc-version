// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "components/chat_panel.h"
#include "common/log_wrapper.h"
#include "common/time_wrapper.h"
#include <vector>

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
    media_engine::ImGuiWindow::GetPos(&win_x, &win_y);
    scroll_window_->SetPosition(win_x + panel_->GetContentX(), win_y + panel_->GetContentY());
    scroll_window_->SetSize(panel_->GetContentWidth(), panel_->GetContentHeight());

    auto _scroll = media_engine::ScopedStyleVar::ScrollbarSize(3.0f);
    auto _msgs = media_engine::ScopedGuard(
        scroll_window_->Begin("______________________________", &media_engine::Colors::CreamLight),
        [this]{ scroll_window_->End(); },
        true);  // BeginChild: always call End
    RenderMessages(snapshot_);

    // Auto-scroll to bottom when new content arrives.
    // Between content updates, user can freely drag the scrollbar.
    size_t msg_count = 0;
    for (const auto& msg : snapshot_.messages) msg_count += msg.content.size();
    if (!snapshot_.streaming_text.empty()) ++msg_count;
    if (!snapshot_.streaming_thinking.empty()) ++msg_count;

    size_t streaming_len = snapshot_.streaming_text.size() + snapshot_.streaming_thinking.size();
    if (msg_count != last_msg_count_ || streaming_len != last_streaming_len_) {
        media_engine::Scroll::SetY(media_engine::Scroll::GetMaxY());
    }
    last_msg_count_ = msg_count;
    last_streaming_len_ = streaming_len;
}

void ChatPanel::SetSnapshot(const RenderSnapshot& snap) {
    if (snap.session_id != last_session_id_) {
        last_session_id_ = snap.session_id;
        display_messages_.clear();
    }
    snapshot_ = snap;
}

void ChatPanel::RenderContent(const RenderSnapshot& snapshot) {
    if (snapshot.session_id != last_session_id_) {
        last_session_id_ = snapshot.session_id;
        display_messages_.clear();
    }
    snapshot_ = snapshot;
    Render(media_engine::RenderContext{});
}

void ChatPanel::RenderContentInRect(float x, float y, float w, float h,
                                     const RenderSnapshot& snapshot) {
    if (!visible_) return;
    scroll_window_->SetPosition(x, y);
    scroll_window_->SetSize(w, h);
    auto _scroll = media_engine::ScopedStyleVar::ScrollbarSize(3.0f);
    auto _msgs = media_engine::ScopedGuard(
        scroll_window_->Begin("______________________________", &media_engine::Colors::CreamLight),
        [this]{ scroll_window_->End(); },
        true);  // BeginChild: always call End
    RenderMessages(snapshot);
    // Auto-scroll to bottom when new content arrives.
    // Between content updates, user can freely drag the scrollbar.
    size_t msg_count = 0;
    for (const auto& msg : snapshot.messages) msg_count += msg.content.size();
    if (!snapshot.streaming_text.empty()) ++msg_count;
    if (!snapshot.streaming_thinking.empty()) ++msg_count;

    size_t streaming_len = snapshot.streaming_text.size() + snapshot.streaming_thinking.size();
    if (msg_count != last_msg_count_ || streaming_len != last_streaming_len_) {
        media_engine::Scroll::SetY(media_engine::Scroll::GetMaxY());
    }
    last_msg_count_ = msg_count;
    last_streaming_len_ = streaming_len;
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
            std::string combined_thinking, combined_text;
            for (const auto& b : msg.content) {
                if (b.type == "thinking") combined_thinking = b.text;
                else if (b.type == "text") combined_text = b.text;
            }
            std::string combined;
            if (!combined_thinking.empty())
                combined = "【thinking】\n" + combined_thinking;
            if (!combined_text.empty()) {
                if (!combined.empty()) combined += "\n\n";
                combined += combined_text;
            }
            if (!combined.empty())
                RenderMessage("assistant", combined, index++);
        } else {
            RenderMessage(msg.role, msg.text(), index++);
        }
    }
    // Streaming entries: during thinking→content transition, combine into
    // a single assistant entry matching the final post-completion structure,
    // so the display doesn't jump from two entries to one.
    if (!snapshot.streaming_thinking.empty() && !snapshot.streaming_text.empty() &&
        (role_filter_.empty() || role_filter_ == "assistant")) {
        std::string combined = "【thinking】\n" + snapshot.streaming_thinking
                             + "\n\n" + snapshot.streaming_text;
        RenderMessage("assistant", combined, index++);
    } else {
        if (!snapshot.streaming_thinking.empty() &&
            (role_filter_.empty() || role_filter_ == "thinking")) {
            RenderMessage("thinking", snapshot.streaming_thinking, index++);
        }
        if (!snapshot.streaming_text.empty() &&
            (role_filter_.empty() || role_filter_ == "assistant")) {
            RenderMessage("assistant", snapshot.streaming_text, index);
        }
    }
}

void ChatPanel::RenderMessage(const std::string& role, const std::string& content, size_t index) {
    float fs = media_engine::Layout::GetFontScale();

    if (index > 0) {
        media_engine::Layout::Dummy(0, 4.0f * fs);
    }

    // Store ChatMessage with timestamp on first render (freeze at message-received time)
    if (index >= display_messages_.size()) {
        display_messages_.resize(index + 1);
        display_messages_[index] = ChatMessage{
            role, "", content,
            SystemClock::GetCurrentEpochSeconds()
        };
    }

    // Use content region width (accounts for child window padding + scrollbar)
    // with zeroed FramePadding so the text area = content_region_width,
    // matching CalcWrappedHeight exactly.
    float content_w = media_engine::Layout::GetContentRegionAvailWidth();
    if (content_w < 50.0f) content_w = 50.0f;  // guard against degenerate layout

    // Measure exact message height for background rect
    float h_pad = 8.0f * fs;
    float v_pad = 6.0f * fs;
    float label_h = hide_role_labels_ ? 0 : 20.0f * fs;
    float text_h = media_engine::Text::CalcWrappedHeight(content.c_str(), content_w - h_pad * 2.0f) + v_pad * 2.0f;
    float total_h = (index > 0 ? 4.0f * fs : 0) + label_h + text_h + 2.0f * fs;

    // Choose background color by role
    media_engine::Color bg_color = user_bg_color_;
    if (role == "assistant") {
        bg_color = assistant_bg_color_;
    } else if (role == "thinking") {
        bg_color = media_engine::Colors::Gray20a;
    }

    float start_x, start_y;
    media_engine::Layout::GetCursorScreenPos(&start_x, &start_y);
    media_engine::DrawList::RoundRect(start_x, start_y - 2.0f * fs, content_w, total_h, 6.0f * fs, bg_color);

    // Light theme (cream background):
    //   user     → black (text) / dark gray (label)
    //   thinking → black (text) / medium gray (label)
    //   assistant→ black (text) / dark green (label)
    //   tool     → black (text) / teal (label)
    //   error    → dark red
    media_engine::Color role_color = media_engine::Colors::Gray40;
    media_engine::Color text_color = media_engine::Colors::Black;

    if (role == "thinking") {
        role_color = media_engine::Colors::Gray55;
        text_color = media_engine::Colors::Black;
    } else if (role == "assistant") {
        role_color = media_engine::Colors::GreenLeafDark;
        text_color = media_engine::Colors::Black;
    } else if (role == "tool" || role == "function") {
        role_color = media_engine::Colors::Teal;
        text_color = media_engine::Colors::Black;
    } else if (role == "error") {
        role_color = media_engine::Colors::RedDark;
        text_color = media_engine::Colors::RedDark;
    }

    if (!hide_role_labels_) {
        std::string label = (role == "assistant" && !assistant_role_name_.empty())
                            ? assistant_role_name_ : role;
        label += " [" + SystemClock::FormatTimestamp(
            static_cast<std::time_t>(display_messages_[index].timestamp), "%H:%M:%S") + "]";
        media_engine::Text::Colored(role_color, label.c_str());
        media_engine::Layout::Dummy(0, 2.0f * fs);
    }
    // Use read-only multiline input for mouse-selectable text
    std::vector<char> text_buf(content.begin(), content.end());
    text_buf.push_back('\0');
    float input_w = content_w;
    std::string input_id = "##txt" + std::to_string(index);
    media_engine::Style::PushColor(media_engine::Color::Slot::Text, text_color);
    auto _fp = media_engine::ScopedStyleVar::FramePadding(h_pad, v_pad);
    media_engine::ImGuiWidget::InputTextMultiline(
        input_id.c_str(), text_buf.data(), text_buf.size(), input_w, text_h, true);
    media_engine::Style::PopColor();
    media_engine::Layout::Dummy(0, 2.0f * fs);
}

void ChatPanel::ScrollToBottom() {
    scroll_window_->ScrollToBottom();
}

bool ChatPanel::IsScrolledToBottom() const {
    return scroll_window_->IsScrolledToBottom();
}

}  // namespace prosophor
