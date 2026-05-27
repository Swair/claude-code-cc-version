// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "common/file_utils.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

#include "common/log_wrapper.h"

namespace prosophor {

// ============================================================================
// Path Utilities
// ============================================================================

std::string GetHomeDir() {
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    return home ? std::string(home) : "";
}

std::string ExpandHome(const std::string& path) {
    std::string expanded = path;
    if (expanded.size() >= 2 && expanded.substr(0, 2) == "~/") {
        std::string home = GetHomeDir();
        if (!home.empty()) {
            expanded = home + expanded.substr(1);
        }
    }
    return expanded;
}

bool EnsureDirectory(const std::string& path) {
    std::error_code ec;
    if (std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec)) {
        return true;
    }
    return std::filesystem::create_directories(path, ec);
}

// ============================================================================
// File Reading
// ============================================================================

std::optional<std::string> ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
}

std::string ReadFileOrFail(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
}

std::optional<nlohmann::json> ReadJson(const std::string& path) {
    auto content = ReadFile(path);
    if (!content) {
        return std::nullopt;
    }
    try {
        return nlohmann::json::parse(*content);
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to parse JSON file {}: {}", path, e.what());
        return std::nullopt;
    }
}

nlohmann::json ReadJsonOrFail(const std::string& path) {
    std::string content = ReadFileOrFail(path);
    try {
        return nlohmann::json::parse(content);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse JSON file " + path + ": " + e.what());
    }
}

// ============================================================================
// File Writing
// ============================================================================

bool WriteFile(const std::string& path, const std::string& content, bool append) {
    std::ofstream file(path, append ? std::ios::app : std::ios::out);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: {}", path);
        return false;
    }
    file << content;
    return file.good();
}

bool WriteJson(const std::string& path, const nlohmann::json& json, int indent) {
    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: {}", path);
        return false;
    }
    file << json.dump(indent);
    return file.good();
}

bool WriteOrderedJson(const std::string& path, const nlohmann::ordered_json& json, int indent) {
    std::string content = json.dump(indent);
    return WriteFile(path, content);
}

void WriteJsonOrFail(const std::string& path, const nlohmann::json& json, int indent) {
    if (!WriteJson(path, json, indent)) {
        throw std::runtime_error("Failed to write JSON file: " + path);
    }
}

// ============================================================================
// File Existence Checks
// ============================================================================

bool FileExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_regular_file(path, ec);
}

bool DirExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && std::filesystem::is_directory(path, ec);
}

bool RemoveFile(const std::string& path) {
    std::error_code ec;
    return std::filesystem::remove(path, ec);
}

uintmax_t FileSize(const std::string& path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : size;
}

std::string FindFileInDirs(const std::string& base_dir, const std::string& filename) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(base_dir, ec)) {
        if (!entry.is_directory()) continue;
        std::filesystem::path candidate = entry.path() / filename;
        if (std::filesystem::exists(candidate, ec)) {
            return candidate.string();
        }
    }
    return {};
}

}  // namespace prosophor
