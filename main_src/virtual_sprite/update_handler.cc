// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/update_handler.h"

#include "components/panel_kit.h"
#include "media_engine/media_engine.h"
#include "platform/platform.h"
#include "updater/update_checker.h"

namespace prosophor {

// ============================================================================
// Update checking & dialog
// ============================================================================

void UpdateHandler::CheckAndShowUpdate() {
    auto& updater = UpdateChecker::Instance();
    if (!updater.IsCheckDone()) {
        if (update_status_text_.empty()) {
            update_status_text_ = "正在检查更新...";
        }
        return; // 等待后台完成
    }

    // Only run once
    static bool checked = false;
    if (checked) return;
    checked = true;

    switch (updater.GetResult()) {
        case CheckResult::kNoUpdate:
            update_status_text_.clear();
            break;
        case CheckResult::kUpdateReady: {
            auto release = updater.GetLatestRelease();
            update_status_text_ = "发现新版本 " + release.tag_name;
            update_popup_open_ = true;
            break;
        }
        case CheckResult::kNoNetwork:
            update_status_text_.clear();
            break;
        case CheckResult::kCheckFailed:
            update_status_text_.clear();
            break;
    }
}

void UpdateHandler::Render() {
    if (!update_popup_open_) return;

    auto release = UpdateChecker::Instance().GetLatestRelease();

    media_engine::Popup::Open("update_modal");
    media_engine::ImGuiWindow::SetNextSize(480.0f, 360.0f, media_engine::ImGuiCond_Appearing);
    auto _modal = media_engine::ScopedModal(
        ("更新###update_modal"), &update_popup_open_,
        media_engine::ImGuiWindowFlags_NoSavedSettings);
    if (!_modal) return;

    float sm = Spacing();

    // Version info
    media_engine::Text::Colored(media_engine::Colors::OrangeDeep, update_status_text_.c_str());
    media_engine::Layout::Dummy(0, 8.0f * sm);

    // Release notes
    std::string notes = release.release_notes;
    if (!notes.empty()) {
        media_engine::Text::Wrapped(notes.c_str(),
            media_engine::ImGuiWindow::GetWidth() - 30.0f,
            media_engine::Colors::Gray40);
        media_engine::Layout::Dummy(0, 12.0f * sm);
    }

    // Download URL button
    float btn_w = 160.0f;
    float win_w = media_engine::ImGuiWindow::GetWidth();
    media_engine::Layout::SetCursorPosX((win_w - btn_w) * 0.5f);

    if (media_engine::ImGuiWidget::Button("下载更新", btn_w, 0)) {
        Platform::OpenWithDefault(release.download_url);
        update_popup_open_ = false;
    }
}

}  // namespace prosophor
