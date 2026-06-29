// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "updater/update_checker.h"

#include <nlohmann/json.hpp>

#include "common/log_wrapper.h"
#include "common/thread_pool.h"
#include "network/curl_client.h"

namespace prosophor {

UpdateChecker& UpdateChecker::Instance() {
    static UpdateChecker instance;
    return instance;
}

// ============================================================================
// Version helpers
// ============================================================================

UpdateChecker::Version UpdateChecker::ParseVersion(const std::string& ver) {
    Version v{0, 0, 0};
    std::string s = ver;
    // Strip leading 'v' or 'V'
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
        s = s.substr(1);
    }
    // Split by '.'
    int* parts[] = {&v.major, &v.minor, &v.patch};
    size_t part_idx = 0;
    size_t start = 0;
    for (size_t i = 0; i <= s.size() && part_idx < 3; ++i) {
        if (i == s.size() || s[i] == '.') {
            if (i > start) {
                *parts[part_idx] = std::stoi(s.substr(start, i - start));
            }
            ++part_idx;
            start = i + 1;
        }
    }
    return v;
}

bool UpdateChecker::IsNewer(const Version& cur, const Version& latest) {
    if (latest.major != cur.major) return latest.major > cur.major;
    if (latest.minor != cur.minor) return latest.minor > cur.minor;
    return latest.patch > cur.patch;
}

// ============================================================================
// Asset filename: determine what to download based on platform
// ============================================================================

static std::string MakeAssetFilename(const std::string& tag_name) {
    // Strip leading 'v' from tag_name for the version portion
    std::string ver = tag_name;
    if (!ver.empty() && (ver[0] == 'v' || ver[0] == 'V')) {
        ver = ver.substr(1);
    }
#ifdef _WIN32
    return "Prosophor-" + ver + "-win64-setup.exe";
#elif defined(__APPLE__)
    return "Prosophor-" + ver + "-macos.dmg";
#else
    return "Prosophor-" + ver + "-linux.tar.gz";
#endif
}

// ============================================================================
// Network check
// ============================================================================

bool UpdateChecker::CheckNetwork() {
    HttpRequest req;
    req.url = "https://www.baidu.com";
    req.timeout_seconds = 5;
    req.user_agent = "Prosophor/" PROSOPHOR_VERSION;

    auto res = HttpClient::Instance().Get(req);
    return res.success();
}

// ============================================================================
// Query GitHub release
// ============================================================================

ReleaseInfo UpdateChecker::QueryGithubRelease() {
    ReleaseInfo info;
    HttpRequest req;
    req.url = "https://api.github.com/repos/Swair/prosophor/releases/latest";
    req.timeout_seconds = 10;
    req.user_agent = "Prosophor/" PROSOPHOR_VERSION;

    auto res = HttpClient::Instance().Get(req);
    if (!res.success()) {
        LOG_WARN("GitHub release query failed (HTTP {}): {}", res.status_code, res.error_msg);
        return info;
    }

    try {
        auto j = nlohmann::json::parse(res.body);

        // GitHub API may return error in JSON body
        if (j.contains("message")) {
            LOG_WARN("GitHub API error: {}", j["message"].get<std::string>());
            return info;
        }

        info.tag_name = j.value("tag_name", "");
        info.release_notes = j.value("body", "");
        info.filename = MakeAssetFilename(info.tag_name);

        if (j.contains("assets") && j["assets"].is_array()) {
            for (const auto& asset : j["assets"]) {
                std::string name = asset.value("name", "");
                if (name == info.filename) {
                    info.download_url = asset.value("browser_download_url", "");
                    info.size = asset.value("size", 0);
                    break;
                }
            }
        }

        // If no matching asset found, construct URL from tag_name
        if (info.download_url.empty()) {
            info.download_url = "https://github.com/Swair/prosophor/releases/download/"
                             + info.tag_name + "/" + info.filename;
        }

        LOG_DEBUG("GitHub latest release: {} ({})", info.tag_name, info.download_url);
    } catch (const std::exception& e) {
        LOG_WARN("Failed to parse GitHub release JSON: {}", e.what());
    }
    return info;
}

// ============================================================================
// CheckForUpdate (async)
// ============================================================================

void UpdateChecker::CheckForUpdate() {
    GetGlobalThreadPool().Submit([this]() {
        // Step 1: Check network
        if (!CheckNetwork()) {
            LOG_WARN("Network check failed — no network connectivity");
            std::lock_guard<std::mutex> lock(mutex_);
            result_ = CheckResult::kNoNetwork;
            check_done_ = true;
            return;
        }
        LOG_DEBUG("Network check passed");

        // Step 2: Query GitHub release
        ReleaseInfo release = QueryGithubRelease();

        // Step 3: Query failed
        if (release.tag_name.empty()) {
            LOG_WARN("GitHub release query failed");
            std::lock_guard<std::mutex> lock(mutex_);
            result_ = CheckResult::kCheckFailed;
            check_done_ = true;
            return;
        }

        // Step 4: Compare versions
        Version current = ParseVersion(PROSOPHOR_VERSION);
        Version latest = ParseVersion(release.tag_name);

        if (IsNewer(current, latest)) {
            LOG_INFO("Update available: {} (current: {})", release.tag_name, PROSOPHOR_VERSION);
            std::lock_guard<std::mutex> lock(mutex_);
            latest_ = release;
            result_ = CheckResult::kUpdateReady;
        } else {
            LOG_DEBUG("Already up to date: {}", PROSOPHOR_VERSION);
            std::lock_guard<std::mutex> lock(mutex_);
            result_ = CheckResult::kNoUpdate;
        }

        check_done_ = true;
    });
}

// ============================================================================
// Query methods
// ============================================================================

CheckResult UpdateChecker::GetResult() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return result_;
}

ReleaseInfo UpdateChecker::GetLatestRelease() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
}

bool UpdateChecker::HasUpdate() const {
    return GetResult() == CheckResult::kUpdateReady;
}

bool UpdateChecker::IsCheckDone() const {
    return check_done_.load();
}


}  // namespace prosophor
