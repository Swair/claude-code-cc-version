// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <memory>
#include "media_engine/media_engine.h"

/// 9 种动作对应 spritesheet 的 9 行
enum class SpritesheetAction {
    IDLE = 0,       // row 0, 6 frames
    RUN_RIGHT = 1,  // row 1, 8 frames
    RUN_LEFT = 2,   // row 2, 8 frames
    WAVE = 3,       // row 3, 4 frames
    JUMP = 4,       // row 4, 5 frames
    FAILED = 5,     // row 5, 8 frames
    WAIT = 6,       // row 6, 6 frames
    SPRINT = 7,     // row 7, 6 frames
    REVIEW = 8,     // row 8, 6 frames
};

/// Spritesheet: load and render individual frames from a petdex spritesheet.
class Spritesheet {
public:
    /// @param window 所属 Window（用于纹理创建）
    /// @param slug  宠物标识（文件名不含扩展名）
    /// @param sprites_dir  spritesheet 所在目录（末尾带 /）
    Spritesheet(media_engine::Window& window, const std::string& slug, const std::string& sprites_dir);
    ~Spritesheet();

    bool IsValid() const { return valid_; }

    int GetFrameCount(SpritesheetAction action) const;
    float GetFrameWidth() const { return frame_width_; }
    float GetFrameHeight() const { return frame_height_; }
    const std::string& GetSlug() const { return slug_; }
    const std::string& GetDisplayName() const { return display_name_; }

    /// 渲染指定动作的某一帧
    bool RenderFrame(SpritesheetAction action, int frame_index,
                     float dst_x, float dst_y, float dst_w, float dst_h) const;

    /// 渲染完整 spritesheet（调试用）
    bool RenderStatic(float dst_x, float dst_y, float dst_w, float dst_h) const;

private:
    std::string slug_;
    std::string display_name_;
    float frame_width_ = 0;
    float frame_height_ = 0;
    bool valid_ = false;
    std::unique_ptr<media_engine::Texture> texture_;
};
