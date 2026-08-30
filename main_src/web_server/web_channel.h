// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "web_server/channel.h"
#include "web_server/web_protocol.h"

namespace prosophor {

/// WS 连接句柄:由 ws_handler(web_server 层)创建并注册。
/// 生命周期归连接线程;send 回调内部持锁并与"置 closed"互斥,
/// 推送与销毁不会并发 → 杜绝 WebSocket 对象 UAF。
struct WsPeer {
    std::string conn_id;
    std::string user_id;
    std::string display_name;                                // 展示名,可空
    std::function<bool(const std::string&)> send;            // 非阻塞;失败/已断返回 false
};

/// Web 渠道:ImChannel 子类,只做"连接注册表 + 推送路由 + 入站转发",
/// 不感知会话与记忆(路由归 WebGateway)。
/// 群成员查询经 SetGroupMemberProvider 回调注入(GroupStore 在 web_server 层),
/// 本类不反向依赖 web_server。
class WebChannel : public ImChannel {
public:
    const std::string& GetName() const override;             // "web"
    bool Start() override;                                   // 只置 running;监听由 WebServerApp 负责
    void Stop() override;                                    // 清空注册表
    void OnAgentOutput(const ImChatContext& ctx, AgentRuntimeState state,
                       const std::string& state_msg,
                       const std::string& delta,
                       const std::optional<MessageSchema>& reply) override;
    void SendError(const ImChatContext& ctx, const std::string& error) override;

    // ── 供 web_server 层调用(ws_handler / rest_api)──
    void AttachConnection(const std::string& conn_id, const std::string& user_id,
                          const std::string& display_name,
                          std::function<bool(const std::string&)> send);
    void DetachConnection(const std::string& conn_id);
    void SetGroupMemberProvider(
        std::function<std::vector<std::string>(const std::string& gid)> provider);
    /// 群绑定角色查询注入(GroupStore 在 web_server 层,经回调注入)。
    void SetGroupRoleProvider(
        std::function<std::optional<std::string>(const std::string& gid)> provider);

    /// 入站:Web 用户发消息(chat_id 由前端给定:私聊 p2p_{uid} / 群聊 group_{gid})。
    /// 在 WS 连接线程上调用,非阻塞。
    void InboundSend(const std::string& conn_id, const ClientSend& send);
    /// 用户是否在线(任一连接)。
    bool IsOnline(const std::string& user_id) const;
    /// 指定用户的所有在线连接。
    std::vector<std::shared_ptr<WsPeer>> PeersOf(const std::string& user_id) const;
    /// 群成员上线/离线广播(由 ws_handler 在连接建立/断开后调用)。
    void BroadcastPresence(const std::string& chat_id, const std::string& user_id, bool online);
    /// chat_id → 在线用户列表(供 get_online 帧)。
    std::vector<std::string> OnlineUserIdsForChat(const std::string& chat_id) const;

    /// chat_id 是否为群聊(前缀 group_)。
    static bool IsGroupChat(const std::string& chat_id);
    /// 从私聊 chat_id 提取 user_id(p2p_{uid})。
    static std::string UserIdOfP2p(const std::string& chat_id);
    /// 从私聊 chat_id 提取角色段(p2p_{uid}_{role};旧格式无 role 返回空)。
    static std::string RoleOfP2p(const std::string& chat_id);

private:
    /// 推送路由:私聊 → 该用户全部连接;群聊 → 群成员的全部在线连接。
    std::vector<std::shared_ptr<WsPeer>> PeersForChat(const ImChatContext& ctx) const;
    void PushToAll(const std::vector<std::shared_ptr<WsPeer>>& peers, const std::string& json_text);
    void PushToPeer(const std::shared_ptr<WsPeer>& peer, const std::string& json_text);

    std::atomic<bool> running_{false};
    std::string role_;                                       // config.web.role,Start 时读

    mutable std::mutex registry_mutex_;
    std::unordered_map<std::string, std::shared_ptr<WsPeer>> peers_;        // conn_id → peer
    std::unordered_map<std::string, std::vector<std::shared_ptr<WsPeer>>> peers_by_user_;
    std::function<std::vector<std::string>(const std::string&)> group_member_provider_;
    std::function<std::optional<std::string>(const std::string&)> group_role_provider_;
};

}  // namespace prosophor
