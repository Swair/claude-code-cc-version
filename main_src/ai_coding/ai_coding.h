// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <string>
#include "common/noncopyable.h"
#include "common/input_event.h"

namespace prosophor {

/// AiCoding: Terminal UI frontend.
/// Owns the terminal event loop; delegates business logic to AgentEngine.
class AiCoding : public Noncopyable {
 public:
    static AiCoding& GetInstance();

    int Run();
    void Stop();

 private:
    AiCoding() = default;
    ~AiCoding() = default;

    void RegisterCallbacks();
    void HandleInputEvent(const InputEvent& event);

    std::string session_id_;
    std::atomic<bool> interrupted_{false};
};

}  // namespace prosophor
