// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "media_engine/media_engine.h"
#include "ui_component/ui_panel.h"
#include "ui_types.h"
#include <functional>
#include <string>
#include <memory>

namespace prosophor {

/// 聊天面板组件 - 使用独立 ScrollWindow
class ChatPanel : public media_engine::Widget {
public:
    ChatPanel(float x, float y, float width, float height);
    ~ChatPanel();

    void OnResize() override;

    void SetSnapshot(const RenderSnapshot& snap);

    using media_engine::Widget::Render;

    void Render() const;
    void Render(const media_engine::RenderContext& ctx) override;
    void RenderContent(const RenderSnapshot& snapshot);

    /// Render messages into a pre-positioned rect (no outer container).
    /// Used by SpeechBubble for embedding inside its own ImGui window.
    void RenderContentInRect(float x, float y, float w, float h,
                             const RenderSnapshot& snapshot);

    void ScrollToBottom();
    bool IsScrolledToBottom() const;

    void SetVisible(bool visible) { media_engine::Widget::SetVisible(visible); if (panel_) panel_->SetVisible(visible); }

    void SetRoleFilter(const std::string& role) { role_filter_ = role; }
    void SetHideRoleLabels(bool hide) { hide_role_labels_ = hide; }

    /// Override the display name for "assistant" role (e.g., sprite name)
    void SetAssistantDisplayName(const std::string& name) { assistant_display_name_ = name; }

    // -- 外观 setter --
    void SetBackgroundColor(const media_engine::Color& color) { media_engine::Widget::SetBackgroundColor(color); if (panel_) panel_->SetBackgroundColor(color); }
    void SetBorderColor(const media_engine::Color& color)     { if (panel_) panel_->SetBorderColor(color); }
    void SetBorderWidth(float w)                               { if (panel_) panel_->SetBorderWidth(w); }
    void SetUserBgColor(const media_engine::Color& c)       { user_bg_color_ = c; }
    void SetAssistantBgColor(const media_engine::Color& c)  { assistant_bg_color_ = c; }

private:
    std::unique_ptr<media_engine::UIPanel> panel_;
    std::unique_ptr<media_engine::ScrollWindow> scroll_window_;
    std::string role_filter_;
    bool hide_role_labels_ = false;
    std::string assistant_display_name_;
    RenderSnapshot snapshot_;
    std::string last_session_id_;
    mutable std::vector<ChatMessage> display_messages_;
    size_t last_msg_count_ = 0;
    size_t last_streaming_len_ = 0;
    media_engine::Color user_bg_color_{media_engine::Colors::BluePale};
    media_engine::Color assistant_bg_color_{media_engine::Colors::GreenPale};

    void RenderMessage(const std::string& role, const std::string& content, size_t index);
    void RenderMessages(const RenderSnapshot& snapshot);
};

}  // namespace prosophor
