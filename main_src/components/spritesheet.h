// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "media_engine/media_engine.h"

/// Spritesheet spritesheet action index (row index in .webp).
enum class SpritesheetAction : uint8_t {
    IDLE = 0,
    RUN_RIGHT = 1,
    RUN_LEFT = 2,
    WAVE = 3,
    JUMP = 4,
    FAILED = 5,
    WAIT = 6,
    SPRINT = 7,
    REVIEW = 8,
    COUNT = 9
};

/// Spritesheet: load a 8×9 spritesheet and render individual frames by action.
class Spritesheet {
public:
    Spritesheet(media_engine::Window& window, const std::string& slug,
                const std::string& sprites_dir);
    ~Spritesheet();

    bool IsValid() const { return valid_; }

    int GetFrameCount(SpritesheetAction action) const;
    int GetActionFps(SpritesheetAction action) const;

    float GetFrameWidth() const { return frame_width_; }
    float GetFrameHeight() const { return frame_height_; }
    const std::string& GetSlug() const { return slug_; }
    const std::string& GetDisplayName() const { return display_name_; }
    const std::string& GetFilePath() const { return file_path_; }

    bool RenderFrame(SpritesheetAction action, int frame_index,
                     float dst_x, float dst_y, float dst_w, float dst_h) const;
    bool RenderStatic(float dst_x, float dst_y, float dst_w, float dst_h) const;

private:
    std::string slug_;
    std::string display_name_;
    std::string file_path_;
    float frame_width_ = 0;
    float frame_height_ = 0;
    bool valid_ = false;
    std::unique_ptr<media_engine::Texture> texture_;
};
