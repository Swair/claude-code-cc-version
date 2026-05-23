// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "managers/gpt_sovits_manager.h"
#include "common/log_wrapper.h"
#include "platform/platform.h"

#include <chrono>
#include <sstream>
#include <thread>

namespace prosophor {

GptSoVitsManager& GptSoVitsManager::GetInstance() {
    static GptSoVitsManager instance;
    return instance;
}

GptSoVitsManager::~GptSoVitsManager() {
    if (running_.load()) {
        Stop();
    }
}

bool GptSoVitsManager::Start(const std::string& install_dir, int port, int timeout_ms) {
    if (running_.load()) {
        LOG_DEBUG("GptSoVitsManager: already running on port {}", port_);
        return true;
    }

    port_ = port;

    // Locate python and script
    std::string python = install_dir + "/runtime/python.exe";
    std::string script = install_dir + "/api_v2.py";
    std::string config = install_dir + "/GPT_SoVITS/configs/tts_infer.yaml";

    if (!platform::PathExists(python)) {
        LOG_ERROR("GptSoVitsManager: python not found at {}", python);
        return false;
    }
    if (!platform::PathExists(script)) {
        LOG_ERROR("GptSoVitsManager: api_v2.py not found at {}", script);
        return false;
    }

    std::ostringstream cmd;
    cmd << "cd /d " << platform::ShellEscape(install_dir) << " && ";
    cmd << platform::ShellEscape(python) << " " << platform::ShellEscape(script);
    cmd << " -a 127.0.0.1 -p " << port;
    cmd << " -c " << platform::ShellEscape(config);

    LOG_INFO("GptSoVitsManager: starting api_v2.py on port {}...", port);

    int pid = platform::LaunchDetachedCommand(cmd.str());
    if (pid < 0) {
        LOG_ERROR("GptSoVitsManager: failed to start api_v2.py");
        return false;
    }

    pid_ = pid;
    running_.store(true);

    if (!WaitForPort(port, timeout_ms)) {
        LOG_ERROR("GptSoVitsManager: startup timeout (port {} not ready in {}ms)", port, timeout_ms);
        Stop();
        return false;
    }

    LOG_INFO("GptSoVitsManager: api_v2.py ready on port {} (PID: {})", port, pid_);
    return true;
}

void GptSoVitsManager::Stop() {
    if (!running_.load()) return;

    LOG_INFO("GptSoVitsManager: stopping api_v2.py (PID: {})", pid_);

    if (pid_ > 0) {
        if (!platform::KillProcess(pid_, false)) {
            LOG_WARN("GptSoVitsManager: force stopping...");
            platform::KillProcess(pid_, true);
        }
    }

    pid_ = -1;
    running_.store(false);
    LOG_INFO("GptSoVitsManager: stopped");
}

bool GptSoVitsManager::IsRunning() const {
    if (!running_.load()) return false;

    if (pid_ > 0 && !platform::IsProcessAlive(pid_)) {
        running_.store(false);
        return false;
    }

    if (pid_ > 0) return true;

    if (platform::CheckPortOpen(port_)) return true;

    running_.store(false);
    return false;
}

bool GptSoVitsManager::WaitForPort(int port, int timeout_ms) const {
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        if (platform::CheckPortOpen(port)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return false;
}

}  // namespace prosophor
