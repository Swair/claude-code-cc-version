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

struct WebGroup {
    std::string group_id;
    std::string name;
    std::string description;
    std::string role_id;        // 群绑定 AI 角色
    std::string owner_id;       // 创建者(不可退群,可删群)
    std::vector<std::string> member_ids;
    std::string created_at;
};

/// 群组持久化(groups.json,纯文件,进程内 shared_mutex)。
/// 群会话在首条消息时由 WebGateway 路由惰性创建,本类只维护成员/角色元数据。
class GroupStore : public Noncopyable {
public:
    explicit GroupStore(std::string data_dir);

    /// 从 data_dir 加载;文件缺失/损坏返回 false 并告警(可重建)。
    bool Load();

    /// 创建群;创建者自动成为 owner + 成员。返回 group_id(失败 nullopt)。
    std::optional<std::string> CreateGroup(const std::string& name, const std::string& description,
                                           const std::string& role_id,
                                           const std::string& owner_id);
    /// 仅 owner 可删。
    bool DeleteGroup(const std::string& group_id, const std::string& requester_id);
    std::optional<WebGroup> GetGroup(const std::string& group_id) const;
    /// 我创建的 + 我加入的群。
    std::vector<WebGroup> ListGroupsForUser(const std::string& user_id) const;
    /// owner 批量拉人(已存在的跳过)。
    bool AddMembers(const std::string& group_id, const std::string& requester_id,
                    const std::vector<std::string>& user_ids);
    bool JoinGroup(const std::string& group_id, const std::string& user_id);
    /// owner 不可退;最后一个成员(即 owner)不可退。
    bool LeaveGroup(const std::string& group_id, const std::string& user_id);
    bool IsMember(const std::string& group_id, const std::string& user_id) const;
    bool IsOwner(const std::string& group_id, const std::string& user_id) const;
    /// 成员 ID 列表(供 WebChannel::SetGroupMemberProvider 注入)。
    std::vector<std::string> MemberIds(const std::string& group_id) const;

private:
    std::string groups_path() const;
    void SaveLocked() const;
    std::optional<WebGroup> FindLocked(const std::string& group_id) const;

    std::string data_dir_;
    mutable std::shared_mutex mutex_;
    std::vector<WebGroup> groups_;
};

}  // namespace prosophor
