// Copyright 2026 AiCode Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <ctime>
#include <chrono>
#include <string>

namespace aicode {

/// GetLocalTime - Get local time from system_clock
/// @param time_point The time point to convert (default: now)
/// @return std::tm structure with local time
std::tm GetLocalTime(std::chrono::system_clock::time_point time_point = std::chrono::system_clock::now());

/// GetLocalTime - Get local time from time_t
/// @param time The time_t value to convert
/// @return std::tm structure with local time
std::tm GetLocalTime(std::time_t time);

/// FormatTimestamp - Format time_point to string
/// @param time_point The time point to format (default: now)
/// @param format The format string (default: "%Y-%m-%d %H:%M:%S")
/// @return Formatted timestamp string
std::string FormatTimestamp(
    std::chrono::system_clock::time_point time_point = std::chrono::system_clock::now(),
    const char* format = "%Y-%m-%d %H:%M:%S");

/// FormatTimestamp - Format time_t to string
/// @param time The time_t value to format
/// @param format The format string (default: "%Y-%m-%d %H:%M:%S")
/// @return Formatted timestamp string
std::string FormatTimestamp(std::time_t time, const char* format = "%Y-%m-%d %H:%M:%S");

/// GetCurrentTimestamp - Get current timestamp as formatted string
/// @return Current timestamp string in format "YYYY-MM-DD HH:MM:SS"
std::string GetCurrentTimestamp();

/// GetCurrentTimeMillis - Get current time as milliseconds since epoch
/// @return Current time in milliseconds (int64_t)
int64_t GetCurrentTimeMillis();

/// GenerateIdWithTimestamp - Generate a unique ID with current timestamp
/// @param prefix The prefix for the ID (e.g., "agent_", "todo_")
/// @return Unique ID string
std::string GenerateIdWithTimestamp(const std::string& prefix);

/// GetCurrentDate - Get current date as string (YYYY-MM-DD)
/// @return Current date string
std::string GetCurrentDate();

}  // namespace aicode
