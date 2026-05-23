// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "file_utils.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <string>
#include <unordered_map>
#include <chrono>
#include <ctime>
#include <vector>

namespace prosophor {

inline void InitLog(const std::string& level = "info") {
    static const std::unordered_map<std::string, spdlog::level::level_enum> kLevelMap = {
        {"trace", spdlog::level::trace},
        {"debug", spdlog::level::debug},
        {"info", spdlog::level::info},
        {"warn", spdlog::level::warn},
        {"warning", spdlog::level::warn},
        {"error", spdlog::level::err},
        {"critical", spdlog::level::critical}
    };

    auto it = kLevelMap.find(level);
    spdlog::level::level_enum log_level = (it != kLevelMap.end()) ? it->second : spdlog::level::info;

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

#ifndef NDEBUG
    // Debug: stdout only, no file logging
#else
    // Release: also write to ~/.prosophor/log/log-YYYYMMDD.txt
    std::string log_dir = ExpandHome("~/.prosophor/log");
    EnsureDirectory(log_dir);
    auto now_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &now_t);
#else
    localtime_r(&now_t, &tm);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y%m%d", &tm);
    std::string log_file = log_dir + "/log-" + buf + ".txt";
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true));
#endif

    auto logger = std::make_shared<spdlog::logger>("prosophor", sinks.begin(), sinks.end());
    logger->set_level(log_level);
    spdlog::set_default_logger(logger);
}

}  // namespace prosophor

#define LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define LOG_FATAL(...) spdlog::error(__VA_ARGS__)
