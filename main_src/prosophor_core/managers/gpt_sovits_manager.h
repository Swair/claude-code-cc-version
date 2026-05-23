// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "common/noncopyable.h"

#include <atomic>
#include <string>

namespace prosophor {

/// Manages the GPT-SoVITS api_v2.py subprocess lifecycle
class GptSoVitsManager : public Noncopyable {
 public:
    static GptSoVitsManager& GetInstance();

    /// Start api_v2.py as background subprocess
    /// @param install_dir  GPT-SoVITS installation directory
    /// @param port         API server port
    /// @param timeout_ms   Max wait time for startup
    bool Start(const std::string& install_dir, int port = 9880, int timeout_ms = 60000);

    /// Stop the running api_v2.py process
    void Stop();

    /// Check if the API server is running
    bool IsRunning() const;

    /// Get the port the server is listening on
    int GetPort() const { return port_; }

 private:
    GptSoVitsManager() = default;
    ~GptSoVitsManager();

    /// Wait for the server to start listening on the given port
    bool WaitForPort(int port, int timeout_ms) const;

    int port_ = 9880;
    int pid_ = -1;
    mutable std::atomic<bool> running_{false};
};

}  // namespace prosophor
