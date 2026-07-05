// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <mutex>

#include "common/noncopyable.h"

namespace prosophor {

struct ReleaseInfo {
    std::string tag_name;         // "v0.8.0"
    std::string download_url;     // 安装包下载地址
    std::string release_notes;    // 发布说明
    std::string filename;         // "Prosophor-0.8.0-win64-setup.exe"
    int64_t size = 0;
};

enum class CheckResult {
    kNoUpdate,       // 已是最新，不弹窗
    kUpdateReady,    // 有可用更新，弹窗
    kNoNetwork,      // 无网络连接
    kCheckFailed,    // 检查失败（两源都失败）
};

class UpdateChecker : Noncopyable {
public:
    static UpdateChecker& Instance();

    // 异步检查更新（启动时调用）
    void CheckForUpdate();

    // 查询结果
    CheckResult GetResult() const;
    ReleaseInfo GetLatestRelease() const;
    bool HasUpdate() const;
    bool IsCheckDone() const;

private:
    struct Version { int major=0, minor=0, patch=0; };
    static Version ParseVersion(const std::string& ver);
    static bool IsNewer(const Version& cur, const Version& latest);

    bool CheckNetwork();
    ReleaseInfo QueryGithubRelease();

    mutable std::mutex mutex_;
    ReleaseInfo latest_;
    CheckResult result_ = CheckResult::kCheckFailed;
    std::atomic<bool> check_done_{false};
};

}  // namespace prosophor
