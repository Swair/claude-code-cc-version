// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

// ============================================================================
// media_engine unified entry point
// External modules (virtual_sprite/, core/) should include this header
// instead of individual media_engine/xxx.h files.
// ============================================================================

#include "ui_component/widget.h"
#include "media/texture/colors.h"
#include "media/media_core.h"
#include "media/texture/drawer.h"
#include "media/texture/texture.h"
#include "media/audio/audior.h"
#include "media/audio/audio_streamer.h"
#include "media/audio/audio_capture.h"
#include "media/audio/voice_channel.h"
#include "media/texture/imgui_widget.h"
#include "media/texture/window.h"
#include "ui_component/ui_panel.h"
#include "ui_component/header_bar.h"
#include "ui_component/input_panel.h"
#include "ui_component/nav_bar.h"
#include "ui_component/label.h"
