// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/noncopyable.h"
#include "common/input_event.h"
#include "virtual_sprite/sprite_manager.h"
#include "virtual_sprite/chat_window.h"
#include <functional>
#include <string>
#include <memory>

namespace prosophor {

/// VirtualSprite: SDL application entry point.
/// Owns SpriteManager (all sprite instances) and the shared central chat window.
class VirtualSprite : public Noncopyable {
 public:
    static VirtualSprite& GetInstance();

    int Run();
    void Stop();

    ChatWindow& GetCentralWindow() { return central_window_; }

    /// External input forwarding (system tray, etc.)
    void HandleTextInput(const char* text);
    void HandleKeyDown(int key_code);
    void HandleMouseButtonDown(int x, int y);

    using InputCallback = std::function<void(const InputEvent&)>;
    void SetInputCallback(InputCallback callback);

 private:
    VirtualSprite();
    ~VirtualSprite();

    void GlobalInit();
    void Shutdown();

    bool shutdown_ = false;

    InputCallback saved_callback_;
    InputCallback input_callback_;

    ChatWindow central_window_;          // shared central chat window
};

}  // namespace prosophor
