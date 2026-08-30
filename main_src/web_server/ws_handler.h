// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <memory>
#include <string>

#include "web_server/web_channel.h"
#include "web_server/group_store.h"
#include "web_server/user_store.h"

namespace httplib {
class Server;
struct Request;
namespace ws {
class WebSocket;
}
}  // namespace httplib

namespace prosophor {

class WebGateway;

/// WS 连接生命周期:token 认证(GET /ws?token=) → 注册连接 → 帧收发 →
/// presence 广播 → 关闭清理。每个连接一个 httplib 线程。
///
/// 线程安全:send 回调经 WsSocketState 持锁,与"置 closed"互斥 →
/// 推送与连接销毁不会并发(WebChannel 侧见 web_channel.h)。
class WsHandler {
public:
    WsHandler(WebChannel& channel, WebGateway& gateway, UserStore& users, GroupStore& groups);

    /// 注册 GET /ws 路由(认证 + 帧分发)。
    void Register(httplib::Server& svr);

private:
    /// 连接生命周期主体:认证 → AttachConnection → presence 上线 → read 循环 → 清理。
    void OnConnection(const httplib::Request& req, httplib::ws::WebSocket& ws);

    /// 单帧分发(send/stop/ping/get_online)。
    void HandleFrame(const std::string& conn_id, const std::string& user_id,
                     const std::string& display_name, httplib::ws::WebSocket& ws,
                     const std::string& message);

    /// 向用户所在全部群广播上线/离线。
    void BroadcastUserPresence(const std::string& user_id, bool online);

    WebChannel& channel_;
    WebGateway& gateway_;
    UserStore& users_;
    GroupStore& groups_;
};

}  // namespace prosophor
