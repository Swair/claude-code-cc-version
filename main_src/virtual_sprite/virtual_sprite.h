// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "common/noncopyable.h"
#include "common/input_event.h"
#include <functional>
#include <string>

namespace prosophor {

/// VirtualSprite: SDL-based graphical interface entry point
class VirtualSprite : public Noncopyable {
 public:
    static VirtualSprite& GetInstance();

    int Run();
    void Stop();

    void HandleTextInput(const char* text);
    void HandleKeyDown(int key_code);
    void HandleMouseButtonDown(int x, int y);

    using InputCallback = std::function<void(const InputEvent&)>;
    void SetInputCallback(InputCallback callback);

 private:
    VirtualSprite();
    ~VirtualSprite();

    void Initialize();
    void Shutdown();

    InputCallback saved_callback_;
    InputCallback input_callback_;
};

}  // namespace prosophor
