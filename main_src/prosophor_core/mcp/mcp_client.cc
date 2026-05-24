// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "mcp/mcp_client.h"

#include <chrono>
#include <thread>
#include <filesystem>

#include "common/log_wrapper.h"
#include "common/file_utils.h"
#include "platform/platform.h"

namespace prosophor {

McpClient& McpClient::GetInstance() {
    static McpClient instance;
    return instance;
}

McpClient::~McpClient() {
    Shutdown();
}

void McpClient::Initialize(const std::vector<McpServerConfig>& servers) {
    LOG_INFO("Initializing MCP client with {} servers", servers.size());

    for (const auto& config : servers) {
        if (!config.enabled) {
            LOG_INFO("Skipping disabled server: {}", config.name);
            continue;
        }
        ConnectToServer(config);
    }
}

bool McpClient::ConnectToServer(const McpServerConfig& config) {
    LOG_INFO("Connecting to MCP server: {} (type: {})", config.name, config.type);

    if (servers_.find(config.name) != servers_.end()) {
        LOG_WARN("Server {} already connected", config.name);
        return true;
    }

    auto conn = std::make_unique<ServerConnection>();
    conn->name = config.name;
    conn->type = config.type;

    if (config.type == "stdio") {
        if (Platform::kIsWindows) {
            LOG_ERROR("stdio MCP servers not supported on Windows");
            return false;
        }

        // Build env vars for the child process
        std::vector<std::string> env_vars;
        if (!config.env.empty()) {
            for (auto it = config.env.begin(); it != config.env.end(); ++it) {
                env_vars.push_back(it.key() + "=" + it.value().get<std::string>());
            }
        }

        auto proc = Platform::ForkAndExec(config.command, config.args, "", env_vars);
        if (proc.pid <= 0) {
            LOG_ERROR("Failed to start MCP server {}", config.name);
            return false;
        }

        conn->process_handle = reinterpret_cast<void*>(static_cast<intptr_t>(proc.pid));
        conn->stdin_fd = proc.stdin_fd;
        conn->stdout_fd = proc.stdout_fd;
        conn->type = "stdio";

        Platform::SetPipeNonBlocking(conn->stdout_fd);

        LOG_INFO("Started MCP server {} with PID {}", config.name, proc.pid);

        // Initialize the connection by listing tools
        try {
            nlohmann::json init_params = {
                {"protocolVersion", "2024-11-05"},
                {"capabilities", {{"tools", {}}, {"resources", {}}, {"prompts", {}}}},
                {"clientInfo", {{"name", "Prosophor"}, {"version", "1.0.0"}}}
            };

            auto result = SendRequest(*conn, "initialize", init_params);
            LOG_INFO("MCP server {} initialized: {}", config.name, result.dump());

            SendRequest(*conn, "notifications/initialized", nlohmann::json::object());

            try {
                auto tools_result = SendRequest(*conn, "tools/list", nlohmann::json::object());
                if (tools_result.contains("tools")) {
                    for (const auto& tool_json : tools_result["tools"]) {
                        McpTool tool;
                        tool.name = tool_json.value("name", "");
                        tool.description = tool_json.value("description", "");
                        tool.input_schema = tool_json.value("inputSchema", nlohmann::json::object());
                        tool.server_name = config.name;
                        conn->tools.push_back(tool);
                    }
                    LOG_INFO("Loaded {} tools from MCP server {}", conn->tools.size(), config.name);
                }
            } catch (const std::exception& e) {
                LOG_WARN("Failed to list tools from {}: {}", config.name, e.what());
            }

            try {
                auto resources_result = SendRequest(*conn, "resources/list", nlohmann::json::object());
                if (resources_result.contains("resources")) {
                    for (const auto& res_json : resources_result["resources"]) {
                        McpResource res;
                        res.uri = res_json.value("uri", "");
                        res.name = res_json.value("name", "");
                        res.description = res_json.value("description", "");
                        res.mime_type = res_json.value("mimeType", "text/plain");
                        conn->resources.push_back(res);
                    }
                    LOG_INFO("Loaded {} resources from MCP server {}", conn->resources.size(), config.name);
                }
            } catch (const std::exception& e) {
                LOG_WARN("Failed to list resources from {}: {}", config.name, e.what());
            }

        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize MCP server {}: {}", config.name, e.what());
            return false;
        }

        servers_[config.name] = std::move(conn);
        return true;
    } else if (config.type == "sse" || config.type == "websocket") {
        // SSE/WebSocket implementation would go here
        // For now, just log that it's not yet implemented
        LOG_WARN("SSE/WebSocket MCP connections not yet implemented: {}", config.name);
        return false;
    }

    return false;
}

void McpClient::DisconnectFromServer(const std::string& server_name) {
    auto it = servers_.find(server_name);
    if (it != servers_.end()) {
        Platform::ClosePipe(it->second->stdin_fd);
        Platform::ClosePipe(it->second->stdout_fd);

        if (it->second->process_handle) {
            int pid = static_cast<int>(reinterpret_cast<intptr_t>(it->second->process_handle));
            Platform::KillProcess(pid, false);
        }

        servers_.erase(it);
        LOG_INFO("Disconnected from MCP server: {}", server_name);
    }
}

bool McpClient::IsConnected(const std::string& server_name) const {
    return servers_.find(server_name) != servers_.end();
}

std::vector<McpTool> McpClient::GetAvailableTools() const {
    std::vector<McpTool> all_tools;
    for (const auto& [name, conn] : servers_) {
        all_tools.insert(all_tools.end(), conn->tools.begin(), conn->tools.end());
    }
    return all_tools;
}

std::vector<McpTool> McpClient::GetServerTools(const std::string& server_name) const {
    auto it = servers_.find(server_name);
    if (it != servers_.end()) {
        return it->second->tools;
    }
    return {};
}

std::vector<McpResource> McpClient::GetAvailableResources() const {
    std::vector<McpResource> all_resources;
    for (const auto& [name, conn] : servers_) {
        all_resources.insert(all_resources.end(), conn->resources.begin(), conn->resources.end());
    }
    return all_resources;
}

std::vector<McpPrompt> McpClient::GetAvailablePrompts() const {
    std::vector<McpPrompt> all_prompts;
    for (const auto& [name, conn] : servers_) {
        all_prompts.insert(all_prompts.end(), conn->prompts.begin(), conn->prompts.end());
    }
    return all_prompts;
}

nlohmann::json McpClient::SendRequest(ServerConnection& conn, const std::string& method,
                                       const nlohmann::json& params) {
    conn.request_id++;

    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", conn.request_id},
        {"method", method},
        {"params", params}
    };

    std::string request_str = request.dump() + "\n";
    WriteToServer(conn, request_str);

    // Read response with timeout
    std::string response_str = ReadFromServer(conn);
    if (response_str.empty()) {
        throw std::runtime_error("No response from MCP server " + conn.name);
    }

    return ParseResponse(response_str);
}

nlohmann::json McpClient::ParseResponse(const std::string& response) {
    auto json = nlohmann::json::parse(response);

    if (json.contains("error")) {
        throw std::runtime_error("MCP error: " + json["error"].value("message", "unknown error"));
    }

    if (json.contains("result")) {
        return json["result"];
    }

    return json;
}

std::string McpClient::ReadFromServer(ServerConnection& conn) {
    if (Platform::kIsWindows) {
        throw std::runtime_error("ReadFromServer not implemented on Windows");
    }

    std::string line;
    char buffer[4096];

    auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(30);

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (now - start > timeout) {
            throw std::runtime_error("Timeout reading from MCP server " + conn.name);
        }

        int bytes_read = Platform::ReadPipe(conn.stdout_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            line += buffer;

            if (line.find('\n') != std::string::npos ||
                (line.find('{') != std::string::npos && line.rfind('}') != std::string::npos)) {
                while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
                    line.pop_back();
                }
                if (!line.empty()) {
                    LOG_DEBUG("Read from MCP server {}: {}", conn.name, line);
                    return line;
                }
            }
        } else if (bytes_read == 0) {
            throw std::runtime_error("MCP server " + conn.name + " closed connection");
        } else if (Platform::IsPipeWouldBlock()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } else {
            throw std::runtime_error("Error reading from MCP server " + conn.name + ": " + Platform::GetPipeErrorString());
        }
    }
}

void McpClient::WriteToServer(ServerConnection& conn, const std::string& data) {
    if (Platform::kIsWindows) {
        throw std::runtime_error("WriteToServer not implemented on Windows");
    }

    LOG_DEBUG("Writing to MCP server {}: {}", conn.name, data);

    int bytes_written = Platform::WritePipe(conn.stdin_fd, data.c_str(), data.size());
    if (bytes_written < 0) {
        throw std::runtime_error("Failed to write to MCP server " + conn.name + ": " + Platform::GetPipeErrorString());
    }
    if (static_cast<size_t>(bytes_written) < data.size()) {
        throw std::runtime_error("Incomplete write to MCP server " + conn.name);
    }
}

std::string McpClient::CallTool(const std::string& tool_name, const nlohmann::json& arguments) {
    // Find which server provides this tool
    for (auto& [name, conn] : servers_) {
        for (const auto& tool : conn->tools) {
            if (tool.name == tool_name) {
                LOG_INFO("Calling MCP tool {} on server {}", tool_name, name);

                nlohmann::json params = {
                    {"name", tool_name},
                    {"arguments", arguments}
                };

                auto result = SendRequest(*conn, "tools/call", params);

                // Extract content from result
                if (result.contains("content")) {
                    std::string content;
                    for (const auto& block : result["content"]) {
                        if (block.value("type", "") == "text") {
                            if (!content.empty()) content += "\n";
                            content += block.value("text", "");
                        }
                    }
                    return content;
                }
                return result.dump();
            }
        }
    }

    throw std::runtime_error("Tool not found: " + tool_name);
}

std::string McpClient::ReadResource(const std::string& uri) {
    // Find which server provides this resource
    for (auto& [name, conn] : servers_) {
        for (const auto& res : conn->resources) {
            if (res.uri == uri) {
                LOG_INFO("Reading MCP resource {} from server {}", uri, name);

                nlohmann::json params = {{"uri", uri}};
                auto result = SendRequest(*conn, "resources/read", params);

                if (result.contains("contents")) {
                    return result["contents"].dump();
                }
                return result.dump();
            }
        }
    }

    throw std::runtime_error("Resource not found: " + uri);
}

std::string McpClient::GetPrompt(const std::string& prompt_name, const nlohmann::json& args) {
    // Find which server provides this prompt
    for (auto& [name, conn] : servers_) {
        for (const auto& prompt : conn->prompts) {
            if (prompt.name == prompt_name) {
                LOG_INFO("Getting MCP prompt {} from server {}", prompt_name, name);

                nlohmann::json params = {
                    {"name", prompt_name},
                    {"arguments", args}
                };

                auto result = SendRequest(*conn, "prompts/get", params);
                return result.dump();
            }
        }
    }

    throw std::runtime_error("Prompt not found: " + prompt_name);
}

void McpClient::Shutdown() {
    LOG_INFO("Shutting down MCP client");
    for (auto& [name, conn] : servers_) {
        DisconnectFromServer(name);
    }
    servers_.clear();
}

std::vector<McpServerConfig> McpClient::GetConfiguredServers() const {
    std::vector<McpServerConfig> configs;
    for (const auto& [name, conn] : servers_) {
        McpServerConfig config;
        config.name = conn->name;
        config.type = conn->type;
        config.enabled = true;
        configs.push_back(config);
    }
    return configs;
}

bool McpClient::AddServer(const McpServerConfig& config) {
    if (servers_.find(config.name) != servers_.end()) {
        LOG_ERROR("MCP server {} already exists", config.name);
        return false;
    }

    LOG_INFO("Adding MCP server: {}", config.name);

    // Connect to the server
    if (!ConnectToServer(config)) {
        LOG_ERROR("Failed to connect to MCP server {}", config.name);
        return false;
    }

    // Save to file
    SaveServersToFile();
    return true;
}

bool McpClient::RemoveServer(const std::string& server_name) {
    auto it = servers_.find(server_name);
    if (it == servers_.end()) {
        LOG_ERROR("MCP server {} not found", server_name);
        return false;
    }

    LOG_INFO("Removing MCP server: {}", server_name);
    DisconnectFromServer(server_name);
    servers_.erase(it);

    // Save to file
    SaveServersToFile();
    return true;
}

std::string McpClient::GetConfigFilePath() const {
    return ExpandHome("~/.prosophor/mcp_servers.json");
}

bool McpClient::LoadServersFromFile() {
    std::string config_file = GetConfigFilePath();

    auto json_opt = ReadJson(config_file);
    if (!json_opt) {
        LOG_INFO("MCP config file does not exist: {}", config_file);
        return false;
    }

    try {
        nlohmann::json config = *json_opt;

        if (config.contains("servers") && config["servers"].is_array()) {
            std::vector<McpServerConfig> servers;
            for (const auto& server_json : config["servers"]) {
                McpServerConfig cfg;
                cfg.name = server_json.value("name", "");
                cfg.type = server_json.value("type", "stdio");
                cfg.command = server_json.value("command", "");
                if (server_json.contains("args")) {
                    cfg.args = server_json["args"].get<std::vector<std::string>>();
                }
                cfg.url = server_json.value("url", "");
                if (server_json.contains("env")) {
                    cfg.env = server_json["env"];
                }
                cfg.enabled = server_json.value("enabled", true);

                if (!cfg.name.empty()) {
                    servers.push_back(cfg);
                }
            }

            Initialize(servers);
            LOG_INFO("Loaded {} MCP servers from {}", servers.size(), config_file);
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load MCP config: {}", e.what());
    }

    return false;
}

bool McpClient::SaveServersToFile() {
    std::string config_file = GetConfigFilePath();

    // Create parent directory if needed
    auto parent_dir = std::filesystem::path(config_file).parent_path();
    std::filesystem::create_directories(parent_dir);

    nlohmann::json config = nlohmann::json::object();
    config["servers"] = nlohmann::json::array();

    for (const auto& [name, conn] : servers_) {
        nlohmann::json server = {
            {"name", name},
            {"type", conn->type},
            {"enabled", true}
        };
        config["servers"].push_back(server);
    }

    WriteJson(config_file, config, 2);
    LOG_INFO("Saved {} MCP servers to {}", servers_.size(), config_file);
    return true;
}

}  // namespace prosophor
