// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "components/spritesheet.h"
#include "media_engine/media/texture.h"
#include "common/log_wrapper.h"
#include "common/file_utils.h"
#include <nlohmann/json.hpp>

Spritesheet::~Spritesheet() = default;

// 各动作的帧数（按行索引 0-8）
static constexpr int kActionFrameCounts[] = {6, 8, 8, 4, 5, 8, 6, 6, 6};
static constexpr int kMaxColumns = 8;   // 行中最大帧数
static constexpr int kTotalRows = 9;     // 总动作行数

/// Search base_dir + subdirectories for a file named {slug}{ext}, return full path or empty.
static std::string FindSpriteFile(const std::string& base_dir,
                                   const std::string& slug,
                                   const std::string& ext) {
    // Try direct path first
    std::string direct = base_dir + "/" + slug + ext;
    if (prosophor::FileExists(direct)) return direct;

    // Search one level deep (by-collection/*/slug.ext)
    return prosophor::FindFileInDirs(base_dir, slug + ext);
}

Spritesheet::Spritesheet(media_engine::Window& window, const std::string& slug, const std::string& sprites_dir)
    : slug_(slug) {
    // 1. 加载 JSON 元数据
    auto json_path = FindSpriteFile(sprites_dir, slug, ".json");
    if (!json_path.empty()) {
        auto json_content = prosophor::ReadFile(json_path);
        if (json_content) {
            try {
                auto j = nlohmann::json::parse(*json_content);
                display_name_ = j.value("displayName", slug);
            } catch (const std::exception& e) {
                LOG_WARN("Spritesheet JSON parse failed for {}: {}", slug, e.what());
                display_name_ = slug;
            }
        } else {
            display_name_ = slug;
        }
    } else {
        display_name_ = slug;
    }

    // 2. 加载纹理（优先 WebP，回退 PNG）
    auto try_load = [&](const std::string& ext) -> std::unique_ptr<media_engine::Texture> {
        auto tex_path = FindSpriteFile(sprites_dir, slug, ext);
        if (tex_path.empty()) return nullptr;
        auto tex = std::make_unique<media_engine::Texture>(window, tex_path);
        if (tex->GetOriginWidth() > 0 && tex->GetOriginHeight() > 0) {
            return tex;
        }
        return nullptr;
    };

    texture_ = try_load(".webp");
    if (!texture_) {
        texture_ = try_load(".png");
    }
    if (!texture_) {
        LOG_WARN("Spritesheet not found: {} (.webp/.png)", slug);
        return;
    }

    // 3. 计算帧尺寸
    float total_w = texture_->GetOriginWidth();
    float total_h = texture_->GetOriginHeight();
    frame_height_ = total_h / static_cast<float>(kTotalRows);
    frame_width_ = total_w / static_cast<float>(kMaxColumns);

    valid_ = true;
    LOG_INFO("Loaded spritesheet: {} ({}x{}, frame {}x{})",
             slug, static_cast<int>(total_w), static_cast<int>(total_h),
             static_cast<int>(frame_width_), static_cast<int>(frame_height_));
}

int Spritesheet::GetFrameCount(SpritesheetAction action) const {
    int idx = static_cast<int>(action);
    if (idx < 0 || idx >= kTotalRows) return 0;
    return kActionFrameCounts[idx];
}

bool Spritesheet::RenderFrame(SpritesheetAction action, int frame_index,
                               float dst_x, float dst_y, float dst_w, float dst_h) const {
    if (!valid_) return false;

    int idx = static_cast<int>(action);
    if (idx < 0 || idx >= kTotalRows) return false;

    int frame_count = kActionFrameCounts[idx];
    if (frame_index < 0 || frame_index >= frame_count) {
        frame_index = frame_index % frame_count;  // 循环
    }

    float src_x = static_cast<float>(frame_index) * frame_width_;
    float src_y = static_cast<float>(idx) * frame_height_;

    return texture_->RenderTexture(src_x, src_y, frame_width_, frame_height_,
                                    dst_x, dst_y, dst_w, dst_h,
                                    false, false);
}

bool Spritesheet::RenderStatic(float dst_x, float dst_y, float dst_w, float dst_h) const {
    if (!valid_) return false;
    return texture_->RenderTexture(dst_x, dst_y, dst_w, dst_h);
}
