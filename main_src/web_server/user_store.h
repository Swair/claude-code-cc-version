// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include "common/noncopyable.h"

namespace prosophor {

struct WebUser {
    std::string user_id;
    std::string username;
    std::string display_name;
    std::string salt;           // 32 hex
    std::string password_hash;  // Sha256Hex(salt + password)
    std::string created_at;
    std::string last_login_at;
};

struct WebToken {
    std::string token;      // 64 hex
    std::string user_id;
    int64_t expires_at_ms;  // epoch ms
};

/// 注册/登录结果:ok=false 时 error 为机器可读码。
struct UserResult {
    bool ok = false;
    std::string error;  // "invalid_username" | "username_taken" | "invalid_credentials" | ...
    std::string value;  // token(仅登录成功时)
    std::string user_id;  // 注册/登录成功时均为对应用户
};

/// 用户与 token 持久化(users.json / tokens.json,纯文件,进程内 shared_mutex)。
/// 密码只存 salted SHA-256(MVP 定位局域网 + 反代 TLS,见 docs)。
class UserStore : public Noncopyable {
public:
    explicit UserStore(std::string data_dir);

    /// 从 data_dir 加载;文件缺失/损坏返回 false 并告警(可重建)。
    bool Load();

    UserResult RegisterUser(const std::string& username, const std::string& password,
                            const std::string& display_name);
    UserResult LoginUser(const std::string& username, const std::string& password,
                         int token_ttl_hours);
    /// 设备身份免注册:device_id(客户端 UUID)已存在 → 复用并签发新 token;
    /// 不存在 → 自动创建用户(display_name = device_id 前 8 位)。
    UserResult GetOrCreateByDeviceId(const std::string& device_id, int token_ttl_hours);
    /// 吊销 token;不存在返回 false。
    bool LogoutUser(const std::string& token);
    /// token 有效(TTL 内)返回对应用户。
    std::optional<WebUser> ValidateToken(const std::string& token) const;
    std::optional<WebUser> GetUser(const std::string& user_id) const;
    std::vector<WebUser> ListUsers() const;

private:
    std::string users_path() const;
    std::string tokens_path() const;
    void SaveLocked() const;

    std::string data_dir_;
    mutable std::shared_mutex mutex_;
    std::vector<WebUser> users_;
    std::vector<WebToken> tokens_;
};

}  // namespace prosophor
