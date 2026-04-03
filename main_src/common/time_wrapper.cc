// Copyright 2026 AiCode Contributors
// SPDX-License-Identifier: Apache-2.0

#include "common/time_wrapper.h"

#include <sstream>
#include <iomanip>

namespace aicode {

namespace {

/// Now - Internal helper to get current time point
inline std::chrono::system_clock::time_point Now() {
    return std::chrono::system_clock::now();
}

}  // namespace

std::tm GetLocalTime(std::chrono::system_clock::time_point time_point) {
    auto time_t_val = std::chrono::system_clock::to_time_t(time_point);
    return GetLocalTime(time_t_val);
}

std::tm GetLocalTime(std::time_t time) {
    std::tm tm_result{};
#ifdef _WIN32
    localtime_s(&tm_result, &time);
#else
    localtime_r(&time, &tm_result);
#endif
    return tm_result;
}

std::string FormatTimestamp(std::chrono::system_clock::time_point time_point, const char* format) {
    auto time_t_val = std::chrono::system_clock::to_time_t(time_point);
    return FormatTimestamp(time_t_val, format);
}

std::string FormatTimestamp(std::time_t time, const char* format) {
    std::tm tm_result = GetLocalTime(time);

    std::ostringstream oss;
    oss << std::put_time(&tm_result, format);
    return oss.str();
}

std::string GetCurrentTimestamp() {
    return FormatTimestamp(Now());
}

int64_t GetCurrentTimeMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

std::string GenerateIdWithTimestamp(const std::string& prefix) {
    return prefix + std::to_string(GetCurrentTimeMillis());
}

std::string GetCurrentDate() {
    return FormatTimestamp(Now(), "%Y-%m-%d");
}

}  // namespace aicode
