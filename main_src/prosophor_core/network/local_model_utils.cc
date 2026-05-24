// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "network/local_model_utils.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include "config/config.h"
#include "common/file_utils.h"
#include "common/log_wrapper.h"
#include "platform/platform.h"

namespace prosophor {

std::vector<std::string> GetSearchPaths() {
    std::vector<std::string> paths;

    // 1. Relative to the running prosophor binary
    std::string self_path = Platform::GetSelfExePath();
    if (!self_path.empty()) {
        auto bin_dir = std::filesystem::path(self_path).parent_path().string();
        paths.push_back(bin_dir + "/llama-server");
    }

    // 2. ~/.prosophor/bin/llama-server
    std::string home = Platform::HomeDir();
    if (!home.empty()) {
        paths.push_back(home + "/.prosophor/bin/llama-server");
        paths.push_back(home + "/.prosophor/llama-server");
    }

    // 3. Development paths
    if (!self_path.empty()) {
        auto exe_dir = std::filesystem::path(self_path).parent_path();
        if (!exe_dir.empty()) {
            // 3a. ../llama.cpp/build/bin/llama-server
            paths.push_back((exe_dir / ".." / ".." / "llama.cpp" / "build" / "bin" / "llama-server").lexically_normal().string());
            paths.push_back((exe_dir / ".." / "llama.cpp" / "build" / "bin" / "llama-server").lexically_normal().string());
            // 3b. Project build directories (build/ on Linux, build_win/ on Windows)
            auto project_root = exe_dir.parent_path().parent_path();
            paths.push_back((project_root / "build" / "bin" / "llama-server").lexically_normal().string());
            paths.push_back((project_root / "build_win" / "bin" / "llama-server").lexically_normal().string());
            paths.push_back((project_root / "build" / "install" / "bin" / "llama-server").lexically_normal().string());
            paths.push_back((project_root / "build_win" / "install" / "bin" / "llama-server").lexically_normal().string());
        }
    }

    // 4. CWD relative
    paths.push_back("./llama-server");

    // 5. Common locations
    paths.push_back("/usr/local/bin/llama-server");
    paths.push_back("/usr/bin/llama-server");

    // 6. Append .exe variants for all base paths (cross-platform symmetry)
    size_t n = paths.size();
    for (size_t i = 0; i < n; ++i) {
        paths.push_back(paths[i] + ".exe");
    }

    return paths;
}

std::string FindServerBinary() {
    const auto& config = ProsophorConfig::GetInstance();
    for (const auto& lm : config.local_models) {
        if (!lm.server_path.empty() && FileExists(lm.server_path)) {
            return lm.server_path;
        }
    }

    for (const auto& path : GetSearchPaths()) {
        if (FileExists(path)) {
            LOG_DEBUG("Found llama-server at: {}", path);
            return path;
        }
    }

    // Try `which llama-server`
    std::string which = Platform::RunShellCommand("which llama-server 2>/dev/null");
    if (!which.empty()) {
        if (which.back() == '\n') which.pop_back();
        if (FileExists(which)) {
            return which;
        }
    }

    return "";
}

void DetectHardware(int& out_gpu_layers, int& out_threads) {
    out_gpu_layers = 0;
    out_threads = 0;

    if (Platform::kIsLinux) {
        std::string nvidia = Platform::RunShellCommand(
            "nvidia-smi --query-gpu=memory.total --format=csv,noheader,nounits 2>/dev/null");
        if (!nvidia.empty()) {
            int vram_mb = std::atoi(nvidia.c_str());
            if (vram_mb > 0) {
                LOG_DEBUG("Detected NVIDIA GPU with {} MB VRAM", vram_mb);
                if (vram_mb >= 24000) {
                    out_gpu_layers = -1;
                } else if (vram_mb >= 12000) {
                    out_gpu_layers = 999;
                } else if (vram_mb >= 6000) {
                    out_gpu_layers = 32;
                } else {
                    out_gpu_layers = 16;
                }
            }
        }

        if (out_gpu_layers == 0) {
            std::string other_gpu = Platform::RunShellCommand(
                "lspci 2>/dev/null | grep -i 'vga\\|3d' | grep -iv 'nvidia' | head -1");
            if (!other_gpu.empty()) {
                LOG_DEBUG("Detected non-NVIDIA GPU: {}", other_gpu);
            }
        }

        std::string cpu_count_str = Platform::RunShellCommand(
            "grep -c '^processor' /proc/cpuinfo 2>/dev/null || nproc");
        if (!cpu_count_str.empty()) {
            long cpu_count = std::atol(cpu_count_str.c_str());
            out_threads = std::max(1L, cpu_count - 1);
            LOG_DEBUG("Detected {} CPU threads, recommending {}", cpu_count, out_threads);
        }
    } else if (Platform::kIsMacOS) {
        std::string ram_str = Platform::RunShellCommand("sysctl -n hw.memsize 2>/dev/null");
        if (!ram_str.empty()) {
            int64_t ram = std::atoll(ram_str.c_str()) / (1024LL * 1024LL * 1024LL);
            LOG_DEBUG("Detected {} GB unified memory", ram);
            if (ram >= 32) {
                out_gpu_layers = -1;
            } else if (ram >= 16) {
                out_gpu_layers = 999;
            } else {
                out_gpu_layers = 16;
            }
        }

        std::string cpu_str = Platform::RunShellCommand("sysctl -n hw.ncpu 2>/dev/null");
        if (!cpu_str.empty()) {
            out_threads = std::max(1, std::atoi(cpu_str.c_str()) - 2);
        }
    }

    LOG_INFO("Hardware detection: GPU layers={}, threads={}",
             out_gpu_layers == -1 ? -1 : out_gpu_layers,
             out_threads == 0 ? -1 : out_threads);
}

}  // namespace prosophor
