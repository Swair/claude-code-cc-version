// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <optional>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace prosophor {

// ============================================================================
// Path Utilities
// ============================================================================

/// ExpandHome - Expand ~ to home directory
/// @param path Input path (may start with ~/)
/// @return Expanded path with ~ replaced by home directory
std::string ExpandHome(const std::string& path);

/// GetHomeDir - Get home directory path
/// @return Home directory path
std::string GetHomeDir();

/// EnsureDirectory - Create directory if it doesn't exist
/// @param path Directory path
/// @return true if directory exists or was created successfully
bool EnsureDirectory(const std::string& path);

// ============================================================================
// File Reading
// ============================================================================

/// ReadFile - Read entire file content as string
/// @param path File path
/// @return File content, or nullopt if file doesn't exist or can't be read
std::optional<std::string> ReadFile(const std::string& path);

/// ReadFileOrFail - Read entire file content, throw on error
/// @param path File path
/// @return File content
/// @throws std::runtime_error if file can't be read
std::string ReadFileOrFail(const std::string& path);

/// ReadJson - Read and parse JSON file
/// @param path File path
/// @return Parsed JSON object, or nullopt if file doesn't exist
std::optional<nlohmann::json> ReadJson(const std::string& path);

/// ReadJsonOrFail - Read and parse JSON file, throw on error
/// @param path File path
/// @return Parsed JSON object
/// @throws std::runtime_error if file can't be read or parsed
nlohmann::json ReadJsonOrFail(const std::string& path);

// ============================================================================
// File Writing
// ============================================================================

/// WriteFile - Write string content to file
/// @param path File path
/// @param content Content to write
/// @param append If true, append to existing file
/// @return true if write succeeded
bool WriteFile(const std::string& path, const std::string& content, bool append = false);

/// WriteJson - Write JSON to file with formatting
/// @param path File path
/// @param json JSON object to write
/// @param indent Indentation (default: 2)
/// @return true if write succeeded
bool WriteJson(const std::string& path, const nlohmann::json& json, int indent = 2);

/// WriteJsonOrFail - Write JSON to file, throw on error
/// @param path File path
/// @param json JSON object to write
/// @param indent Indentation (default: 2)
/// @throws std::runtime_error if write fails
void WriteJsonOrFail(const std::string& path, const nlohmann::json& json, int indent = 2);

// ============================================================================
// File Existence Checks
// ============================================================================

/// FileExists - Check if file exists
/// @param path File path
/// @return true if file exists
bool FileExists(const std::string& path);

/// DirExists - Check if directory exists
/// @param path Directory path
/// @return true if directory exists
bool DirExists(const std::string& path);

}  // namespace prosophor
