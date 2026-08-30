// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "web_server/web_protocol.h"

#include <chrono>

#include <nlohmann/json.hpp>

namespace prosophor {

// ── 客户端 → 服务端 ────────────────────────────────────────────────────────

std::optional<ClientFrame> ParseClientFrame(const std::string& json_text) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(json_text);
    } catch (...) {
        return std::nullopt;
    }
    if (!json.is_object() || !json.contains("type") || !json["type"].is_string()) {
        return std::nullopt;
    }

    ClientFrame frame;
    frame.type = json["type"].get<std::string>();

    if (frame.type == "hello") {
        ClientHello hello;
        if (json.contains("token") && json["token"].is_string()) {
            hello.token = json["token"].get<std::string>();
        }
        if (json.contains("client") && json["client"].is_string()) {
            hello.client = json["client"].get<std::string>();
        }
        frame.hello = std::move(hello);
    } else if (frame.type == "send") {
        ClientSend send;
        send.chat_id       = json.value("chat_id", "");
        send.text          = json.value("text", "");
        send.client_msg_id = json.value("client_msg_id", "");
        if (send.chat_id.empty() || send.text.empty()) {
            return std::nullopt;
        }
        frame.send = std::move(send);
    } else if (frame.type == "stop") {
        ClientStop stop;
        stop.chat_id = json.value("chat_id", "");
        if (stop.chat_id.empty()) {
            return std::nullopt;
        }
        frame.stop = std::move(stop);
    } else if (frame.type == "ping") {
        ClientPing ping;
        ping.t = json.value("t", "");
        frame.ping = std::move(ping);
    } else if (frame.type == "get_online") {
        ClientGetOnline online;
        online.chat_id = json.value("chat_id", "");
        if (online.chat_id.empty()) {
            return std::nullopt;
        }
        frame.get_online = std::move(online);
    } else {
        return std::nullopt;
    }

    return frame;
}

// ── 服务端 → 客户端 ────────────────────────────────────────────────────────

namespace {

/// 空字符串字段不写入 JSON(前端只按 present 字段消费)。
nlohmann::ordered_json BaseFrame(const std::string& type) {
    nlohmann::ordered_json json;
    json["type"] = type;
    return json;
}

}  // namespace

std::string MakeWelcomeFrame(const std::string& user_id, const std::string& username,
                             const std::string& display_name, int64_t server_ts,
                             const std::string& token) {
    auto json = BaseFrame("welcome");
    nlohmann::ordered_json user;
    user["user_id"] = user_id;
    if (!username.empty()) user["username"] = username;
    if (!display_name.empty()) user["display_name"] = display_name;
    json["user"] = std::move(user);
    json["server_ts"] = server_ts;
    if (!token.empty()) json["token"] = token;
    return json.dump();
}

std::string MakeAckFrame(const std::string& client_msg_id, const std::string& chat_id,
                         const std::string& session_id, int64_t ts) {
    auto json = BaseFrame("ack");
    json["client_msg_id"] = client_msg_id;
    json["chat_id"] = chat_id;
    json["session_id"] = session_id;
    json["ts"] = ts;
    return json.dump();
}

std::string MakeSessionReadyFrame(const std::string& chat_id, const std::string& session_id,
                                  const std::string& role_id, bool created) {
    auto json = BaseFrame("session_ready");
    json["chat_id"] = chat_id;
    json["session_id"] = session_id;
    json["role_id"] = role_id;
    json["created"] = created;
    return json.dump();
}

std::string MakeStreamStartFrame(const std::string& chat_id, const std::string& session_id,
                                 const std::string& role_id) {
    auto json = BaseFrame("stream_start");
    json["chat_id"] = chat_id;
    json["session_id"] = session_id;
    json["role_id"] = role_id;
    return json.dump();
}

std::string MakeDeltaFrame(const std::string& chat_id, const std::string& session_id,
                           const std::string& delta) {
    auto json = BaseFrame("delta");
    json["chat_id"] = chat_id;
    json["session_id"] = session_id;
    json["delta"] = delta;
    return json.dump();
}

std::string MakeStateFrame(const std::string& chat_id, const std::string& session_id,
                           const std::string& state, const std::string& state_msg) {
    auto json = BaseFrame("state");
    json["chat_id"] = chat_id;
    json["session_id"] = session_id;
    json["state"] = state;
    if (!state_msg.empty()) json["state_msg"] = state_msg;
    return json.dump();
}

std::string MakeReplyFrame(const std::string& chat_id, const std::string& session_id,
                           const std::string& content, int64_t ts) {
    auto json = BaseFrame("reply");
    json["chat_id"] = chat_id;
    json["session_id"] = session_id;
    json["content"] = content;
    json["ts"] = ts;
    return json.dump();
}

std::string MakeErrorFrame(const std::string& code, const std::string& message,
                           const std::string& chat_id) {
    auto json = BaseFrame("error");
    json["code"] = code;
    json["message"] = message;
    if (!chat_id.empty()) json["chat_id"] = chat_id;
    return json.dump();
}

std::string MakePongFrame(const std::string& t) {
    auto json = BaseFrame("pong");
    json["t"] = t;
    return json.dump();
}

std::string MakeOnlineFrame(const std::string& chat_id,
                            const std::vector<std::pair<std::string, std::string>>& members,
                            const std::vector<std::string>& online_ids) {
    auto json = BaseFrame("online");
    json["chat_id"] = chat_id;
    auto members_json = nlohmann::ordered_json::array();
    for (const auto& [user_id, display_name] : members) {
        nlohmann::ordered_json member;
        member["user_id"] = user_id;
        member["display_name"] = display_name;
        members_json.push_back(std::move(member));
    }
    json["members"] = std::move(members_json);
    json["online_ids"] = online_ids;
    return json.dump();
}

std::string MakePresenceFrame(const std::string& chat_id, const std::string& user_id,
                              bool online) {
    auto json = BaseFrame("presence");
    json["chat_id"] = chat_id;
    json["user_id"] = user_id;
    json["online"] = online;
    return json.dump();
}

std::string StateToString(AgentRuntimeState state) {
    switch (state) {
        case AgentRuntimeState::IDLE:
        case AgentRuntimeState::COMPLETE:
        case AgentRuntimeState::STREAM_MODE_COMPLETE:
        case AgentRuntimeState::STREAM_CONTENT_END:
            return "idle";
        case AgentRuntimeState::BEGINNING:
        case AgentRuntimeState::EXECUTING_TOOL:
        case AgentRuntimeState::TOOL_USE:
        case AgentRuntimeState::STREAM_TOOL_START:
        case AgentRuntimeState::STREAM_TOOL:
        case AgentRuntimeState::STREAM_TOOL_END:
            return "working";
        case AgentRuntimeState::WAITING_PERMISSION:
            return "waiting_permission";
        case AgentRuntimeState::STATE_ERROR:
            return "error";
        case AgentRuntimeState::STREAM_CONTENT_TYPING:
        case AgentRuntimeState::STREAM_CONTENT_START:
        case AgentRuntimeState::STREAM_THINKING_END:
            return "streaming";
        case AgentRuntimeState::STREAM_THINKING_START:
        case AgentRuntimeState::STREAM_THINKING:
            return "thinking";
    }
    return "idle";
}

int64_t CurrentTimestampMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace prosophor
