// Copyright 2026 AiCode Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace aicode {

// ============================================================================
// String Encoding Utilities
// ============================================================================

/// ConvertToUtf8 - Convert system default encoding (e.g., GBK on Windows China) to UTF-8
/// @param input String that may be in non-UTF-8 encoding
/// @return UTF-8 encoded string (returns input as-is if already UTF-8 or conversion fails)
std::string ConvertToUtf8(const std::string& input);

/// IsUtf8 - Check if a string is valid UTF-8 encoded
/// @param input String to check
/// @return true if valid UTF-8, false otherwise
bool IsUtf8(const std::string& input);

// ============================================================================
// Cross-Platform Input Utilities
// ============================================================================

/// ReadLine - Read a line from stdin with proper UTF-8 encoding on all platforms
/// On Windows: Uses ReadConsoleW to read wide chars and converts to UTF-8
/// On POSIX: Uses std::getline (assumes terminal is already UTF-8)
/// @return UTF-8 encoded line (without trailing newline)
std::string ReadLine();

}  // namespace aicode
