// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <optional>

namespace prosophor {

/// 子进程执行结果
struct SubprocessResult {
    int return_code = 0;
    std::string output;      // stdout 输出
    std::string error_output;  // stderr 输出
    bool timeout = false;
};

/// 执行脚本，带超时保护
/// @param script_path 脚本路径
/// @param timeout_ms 超时时间（毫秒）
/// @return 执行结果
SubprocessResult ExecuteScriptWithTimeout(
    const std::string& script_path,
    int timeout_ms = 100
);

}  // namespace prosophor
