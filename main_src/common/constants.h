// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace prosophor {

// Default LLM settings
inline constexpr int kDefaultMaxIterations = 32;
inline constexpr double kDefaultTemperature = 0.7;
inline constexpr int kDefaultMaxTokens = 8192;
inline constexpr int kDefaultContextWindow = 128000;

// Context window thresholds
inline constexpr int kContextWindow8K = 8000;
inline constexpr int kContextWindow16K = 16000;
inline constexpr int kContextWindow32K = 32000;
inline constexpr int kContextWindow128K = 128000;
inline constexpr int kContextWindow200K = 200000;

// Iteration bounds based on context window
inline constexpr int kMinMaxIterations = 32;
inline constexpr int kMaxMaxIterations = 160;

// Compaction settings
inline constexpr int kDefaultCompactMaxMessages = 100;
inline constexpr int kDefaultCompactKeepRecent = 20;
inline constexpr int kDefaultCompactMaxTokens = 100000;

// Provider settings
inline constexpr int kDefaultProviderTimeoutSec = 30;

// Tool settings
inline constexpr int kDefaultToolTimeoutSec = 60;
inline constexpr int kToolResultMaxChars = 50000;
inline constexpr int kToolResultKeepLines = 50;

// MCP settings
inline constexpr int kDefaultMcpTimeoutSec = 30;

// Gateway / HTTP ports
inline constexpr int kDefaultGatewayPort = 18800;
inline constexpr int kDefaultHttpPort = 18801;

// Overflow compaction retry limit
inline constexpr int kOverflowCompactionMaxRetries = 3;

// Token estimation
inline constexpr int kCharsPerTokenEstimate = 4;

// ANSI color codes for terminal output
namespace ColorCode {
inline constexpr const char* kReset  = "\033[0m";
inline constexpr const char* kGray   = "\033[90m";  // thinking (dim/internal)
inline constexpr const char* kGreen  = "\033[32m";  // prompt / complete
inline constexpr const char* kCyan   = "\033[36m";  // tool execution
inline constexpr const char* kRed    = "\033[31m";  // error
}

}  // namespace prosophor
