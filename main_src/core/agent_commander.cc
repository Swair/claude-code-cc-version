// Copyright 2026 AiCode Contributors
// SPDX-License-Identifier: Apache-2.0

#include "core/agent_commander.h"

#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#include "common/log_wrapper.h"
#include "common/banner.h"
#include "common/input_queue.h"
#include "common/string_utils.h"
#include "core/agent_core.h"
#include "managers/memory_manager.h"
#include "core/skill_loader.h"
#include "core/system_prompt.h"
#include "managers/buddy_manager.h"
#include "managers/session_manager.h"
#include "cli/command_registry.h"
#include "providers/qwen_provider.h"
#include "providers/anthropic_provider.h"
#include "tools/tool_registry.h"

namespace aicode {

namespace {
AgentCommander* g_commander_ptr = nullptr;
}  // namespace

AgentCommander& AgentCommander::GetInstance() {
    static AgentCommander instance;
    return instance;
}

AgentCommander::AgentCommander()
    : interrupted_(false),
      waiting_permission_(false),
      permission_response_('\0'),
      workspace_path_(aicode::AiCodeConfig::BaseDir() / "workspace") {
    InitializeComponents();
}

AgentCommander::~AgentCommander() {
    if (memory_manager_) {
        memory_manager_->StopFileWatcher();
    }
}

void AgentCommander::InitializeComponents() {
    LOG_INFO("Initializing AiCode components...");

    // Get config from singleton (loads from ~/.aicode/settings.json)
    config_ = aicode::AiCodeConfig::GetInstance();

    // Get agent config from default provider's default agent
    auto provider_it = config_.providers.find(config_.default_provider);
    if (provider_it == config_.providers.end()) {
        LOG_WARN("Provider '{}' not found, using defaults", config_.default_provider);
        provider_it = config_.providers.find("anthropic");
    }
    if (provider_it == config_.providers.end()) {
        LOG_ERROR("No provider configured");
        throw std::runtime_error("No provider configured");
    }

    const auto& provider_config = provider_it->second;
    agent_config_ = provider_config.GetDefaultAgent();

    LOG_INFO("Using provider: {}, agent: {}, model: {}",
             config_.default_provider, agent_config_.name, agent_config_.model);

    // Create workspace directory
    std::filesystem::create_directories(workspace_path_);

    // Create AGENTS.md if not exists
    auto agents_file = workspace_path_ / "AGENTS.md";
    if (!std::filesystem::exists(agents_file)) {
        std::ofstream ofs(agents_file);
        if (ofs) {
            ofs << "# AI Agent Instructions\n\nThis file contains instructions for AI agents.\n";
            ofs.close();
        }
    }

    // Memory Manager
    memory_manager_ = std::make_shared<MemoryManager>(workspace_path_);
    memory_manager_->LoadWorkspaceFiles();
    memory_manager_->StartFileWatcher();

    // Skill Loader
    skill_loader_ = std::make_shared<SkillLoader>();

    // Tool Registry
    tool_registry_ = std::make_shared<ToolRegistry>();
    tool_registry_->RegisterBuiltinTools();
    tool_registry_->SetWorkspace(workspace_path_.string());

    // Set up permission confirmation callback for interactive mode
    std::cout << "[InitializeComponents] Setting up permission manager..." << std::endl;
    auto& perm_manager = PermissionManager::GetInstance();
    perm_manager.SetConfirmCallback(
        [this, &perm_manager](const std::string& tool_name, const nlohmann::json& input, const std::string& reason) -> bool {
            std::string cmd_display;
            if (tool_name == "bash" && input.contains("command")) {
                cmd_display = input["command"].get<std::string>();
            } else if (tool_name == "read_file" && input.contains("path")) {
                cmd_display = "read " + input["path"].get<std::string>();
            } else if (tool_name == "write_file" && input.contains("path")) {
                cmd_display = "write " + input["path"].get<std::string>();
            } else if (tool_name == "edit_file" && input.contains("path")) {
                cmd_display = "edit " + input["path"].get<std::string>();
            }

            // Check if this is a safe read-only command - auto-allow
            bool is_safe_command = false;
            if (tool_name == "bash" && !cmd_display.empty()) {
                // Safe read-only commands
                static const std::vector<std::string> safe_cmds = {
                    "cat ", "ls ", "find ", "grep ", "head ", "tail ",
                    "wc ", "sort ", "uniq ", "pwd ", "echo ", "true",
                    "git status", "git diff ", "git log ", "git show ",
                    "git branch", "git remote", "git rev-parse",
                    "test -", "[ -", "stat ", "file ", "which ", "whereis "
                };
                for (const auto& safe : safe_cmds) {
                    if (cmd_display.find(safe) == 0) {
                        is_safe_command = true;
                        break;
                    }
                }
                // Never auto-allow dangerous commands
                static const std::vector<std::string> unsafe_cmds = {
                    "rm -rf", "rm -r", "rm -f", "sudo", "mkfs", "dd ",
                    "chmod -R", "chown -R", "curl ", "wget ", "bash -c",
                    "sh -c", "eval ", "exec ", ":(){:|:&}"
                };
                for (const auto& unsafe : unsafe_cmds) {
                    if (cmd_display.find(unsafe) != std::string::npos) {
                        is_safe_command = false;
                        break;
                    }
                }
            } else if (tool_name == "read_file") {
                // read_file is safe
                is_safe_command = true;
            }

            if (is_safe_command) {
                // Auto-allow and add to allow rules
                LOG_DEBUG("Auto-allowing safe command: {}", cmd_display);
                PermissionRule rule;
                if (tool_name == "bash") {
                    rule.tool_name = "bash";
                    rule.command_pattern = cmd_display.substr(0, cmd_display.find(' ')) + " *";
                } else {
                    rule.tool_name = tool_name;
                }
                rule.default_level = PermissionLevel::Allow;
                perm_manager.AddAllowRule(rule);
                return true;
            }

            // Show permission prompt
            std::cout << "\n╔════════════════════════════════════════════╗\n";
            std::cout << "║         TOOL PERMISSION REQUEST            ║\n";
            std::cout << "╚════════════════════════════════════════════╝\n\n";
            std::cout << "Tool: " << tool_name;
            if (!cmd_display.empty()) {
                std::cout << "\nCommand: " << cmd_display;
            }
            std::cout << "\nReason: " << reason << "\n\n";
            std::cout << "Allow this operation? (y/n, or 'a' to always allow): " << std::flush;

            // Signal producer to pause reading and wait for permission response
            waiting_permission_ = true;
            permission_response_ = '\0';

            // Wait for user response from producer thread
            std::unique_lock<std::mutex> lock(permission_mutex_);
            permission_cv_.wait(lock, [this]() {
                return permission_response_ != '\0';
            });

            char response = permission_response_.load();
            waiting_permission_ = false;

            bool allowed = (response == 'y' || response == 'Y');

            // If user chose 'always allow', add a rule
            if (response == 'a' || response == 'A') {
                allowed = true;
                PermissionRule rule;
                if (tool_name == "bash" && !cmd_display.empty()) {
                    rule.tool_name = "bash";
                    // Extract base command
                    std::string base_cmd = cmd_display.substr(0, cmd_display.find(' '));
                    rule.command_pattern = base_cmd + " *";
                } else {
                    rule.tool_name = tool_name;
                }
                rule.default_level = PermissionLevel::Allow;
                perm_manager.AddAllowRule(rule);
                LOG_INFO("Added allow rule for: {}", tool_name);
            }

            return allowed;
        }
    );

    // Command Registry (for tab completion)
    command_registry_ = &CommandRegistry::GetInstance();
    command_registry_->Initialize();

    // LLM Provider
    llm_provider_ = std::make_shared<QwenProvider>(
        provider_config.api_key, provider_config.base_url, provider_config.timeout);
    LOG_INFO("Using {} provider with base_url: {}", config_.default_provider, provider_config.base_url);

    // Agent CloseLoop
    std::cout << "[InitializeComponents] Creating AgentCore..." << std::endl;
    agent_core_ = std::make_shared<AgentCore>(
        memory_manager_,
        skill_loader_,
        tool_registry_->GetToolSchemas(),
        [&](const std::string& tool_name, const nlohmann::json& args) -> std::string {
            // Check permission before executing tool
            auto perm_result = perm_manager.CheckPermission(tool_name, args);

            if (perm_result.level == PermissionLevel::Deny) {
                LOG_WARN("Tool {} confirmation denied by user", tool_name);
                throw std::runtime_error("Tool execution denied by user");
            }

            if (perm_result.level == PermissionLevel::Ask) {
                // Request user confirmation
                std::string reason = perm_result.reason;
                if (tool_name == "bash" && args.contains("command")) {
                    std::string cmd = args["command"].get<std::string>();
                    // Check for dangerous commands
                    if (cmd.find("rm -rf") != std::string::npos ||
                        cmd.find("sudo rm") != std::string::npos ||
                        cmd.find("mkfs") != std::string::npos ||
                        cmd.find(":(){:|:&}") != std::string::npos) {
                        reason = "Dangerous command detected";
                    }
                }

                if (!perm_manager.RequestUserConfirmation(tool_name, args, reason)) {
                    LOG_WARN("Tool {} confirmation denied by user", tool_name);
                    throw std::runtime_error("Tool execution denied by user");
                }
            }

            return tool_registry_->ExecuteTool(tool_name, args);
        },
        [&](const ChatRequest& request)-> ChatResponse {
            return llm_provider_->Chat(request);
        },
        [&](const ChatRequest& request, std::function<void(const ChatResponse&)> callback) {
            llm_provider_->ChatStream(request, callback);
        },
        agent_config_
    );

    LOG_INFO("AgentCore initialized: model={}, temp={}, max_tokens={}, context_window={}",
             agent_config_.model, agent_config_.temperature,
             agent_config_.max_tokens, agent_config_.context_window);

    LOG_INFO("InitializeComponents finished");
}

std::vector<SystemSchema> AgentCommander::BuildSystemPrompt() {
    return aicode::BuildSystemPrompt(config_, skill_loader_, memory_manager_, workspace_path_.string());
}

void AgentCommander::PrintHelp() {
    std::cout << "\nCommands:\n";
    std::cout << "  /help     - Show this help message\n";
    std::cout << "  /clear    - Clear conversation history\n";
    std::cout << "  /model    - Show/change current model\n";
    std::cout << "  /config   - Show current configuration\n";
    std::cout << "  /memory   - Show/save daily memory\n";
    std::cout << "  /skills   - List available skills\n";
    std::cout << "  /<skill>  - Load a skill (e.g., /git, /test)\n";
    std::cout << "  /buddy    - Show your companion pet\n";
    std::cout << "  /buddy hatch - Generate a new companion\n";
    std::cout << "  /permissions - Show permission rules\n";
    std::cout << "  /perms clear - Clear all permission rules\n";
    std::cout << "  /exit     - Exit the application\n";
    std::cout << "\n";
    std::cout << "Permission tips:\n";
    std::cout << "  - Safe commands (cat, ls, find, grep, git status, etc.) are auto-allowed\n";
    std::cout << "  - When prompted, enter 'y' to allow once, 'a' to always allow\n";
    std::cout << "\n";
}

bool AgentCommander::HandleCommand(const std::string& line) {
    if (line.empty() || line[0] != '/') {
        return false;  // Not a command
    }

    // Parse command line
    std::vector<std::string> args = CommandRegistry::ParseCommandLine(line);
    if (args.empty()) {
        return false;
    }

    std::string cmd_name = args[0].substr(1);  // Remove leading '/'
    std::vector<std::string> cmd_args(args.begin() + 1, args.end());

    // Check for exit commands first (always available)
    if (cmd_name == "exit" || cmd_name == "quit") {
        LOG_INFO("Exiting AiCode...");
        return true;
    }

    // Execute command via registry
    if (command_registry_ && command_registry_->HasCommand(cmd_name)) {
        CommandContext ctx;
        ctx.workspace = workspace_path_.string();
        ctx.session_id = SessionManager::GetInstance().GetCurrentSessionId();
        ctx.agent_core = agent_core_.get();

        CommandResult result = command_registry_->ExecuteCommand(cmd_name, cmd_args, ctx);

        if (!result.output.empty()) {
            std::cout << result.output << "\n";
        }
        if (!result.error.empty()) {
            std::cerr << result.error << "\n";
        }

        return result.success;
    }

    // Unknown command
    std::cout << "Unknown command: " << line << "\n";
    std::cout << "Type /help for available commands.\n";
    return true;
}

void AgentCommander::ProcessUserMessage(const std::string& line) {
    try {
        LOG_INFO("User: {}", line);

        if (agent_core_) {
            // Reset interrupted_ flag before processing new message
            interrupted_ = false;

            auto response = agent_core_->CloseLoop(line);

            // Check if interrupted during processing
            if (interrupted_) {
                interrupted_ = false;  // Reset for next use
                return;
            }

            // Print response
            for (const auto& msg : response) {
                // Check for interrupt during output
                if (interrupted_) {
                    return;
                }

                if (msg.role == "assistant") {
                    std::string text = msg.text();

                    // Also print tool use information
                    for (const auto& block : msg.content) {
                        if (block.type == "tool_use") {
                            if (!text.empty()) text += "\n";
                            text += "[Using tool: " + block.name + "]";
                        } else if (block.type == "tool_result") {
                            if (!text.empty()) text += "\n";
                            text += "[Tool result: " + block.content.substr(0, 100) +
                                    (block.content.size() > 100 ? "..." : "") + "]";
                        }
                    }

                    if (!text.empty()) {
#ifdef _WIN32
                        LOG_INFO("AiCode: {}", text);
#else
                        std::cout << "AiCode: " << text << std::endl;
#endif
                    } else if (!msg.content.empty()) {
#ifdef _WIN32
                        LOG_INFO("AiCode: [Processing with tools]");
#else
                        std::cout << "AiCode: [Processing with tools]" << std::endl;
#endif		
                    }
                }
            }
        }

    } catch (const std::exception& e) {
        LOG_ERROR("Error: {}", e.what());
        std::cout << "Error: " << e.what() << std::endl;
    }
}

int AgentCommander::Run() {
    // Set global pointer for signal handling (already set up in constructor)
    g_commander_ptr = this;

    // Load companion pet (if exists)
    auto& buddy = BuddyManager::GetInstance();
    buddy.LoadCompanion();

    // Print banner (with buddy if enabled in config)
    bool show_buddy = config_.show_buddy;
    aicode::PrintBanner(AICODE_VERSION, show_buddy);

    system_prompt_ = BuildSystemPrompt();

    // Set initial system prompt on agent
    agent_core_->SetSystemPrompt(system_prompt_, true);

    // Input queue for producer-consumer pattern
    InputQueue input_queue;


    LOG_INFO("Starting main loop...");

    // Producer thread: read input and handle commands
    std::thread producer_thread([this, &input_queue, show_buddy]() {
        LOG_INFO("Starting main producer_thread...");
        while (!interrupted_) {
            // Show prompt
#ifdef _WIN32
            LOG_INFO("> ");
#else
            std::cout << "> " << std::flush;
#endif

            std::string line = aicode::ReadLine();

            // Handle permission response during permission prompt
            if (waiting_permission_) {
                if (!line.empty()) {
                    char c = line[0];
                    if (c == 'y' || c == 'Y' || c == 'n' || c == 'N' || c == 'a' || c == 'A') {
                        permission_response_ = c;
                        permission_cv_.notify_one();
                    }
                }
                continue;
            }

            // Check if interrupted during ReadLine
            if (interrupted_) {
                std::cout << "[Interrupted]\n";
                interrupted_ = false;
                agent_core_->Stop();
                continue;
            }

            // Normal input handling - skip empty lines
            if (line.empty()) {
                continue;
            }

            if (line[0] == '/') {
                if (HandleCommand(line)) {
                    if (line == "/exit" || line == "/quit" || line == "/bye") {
                        interrupted_ = true;
                        input_queue.NotifyAll();
                        break;
                    }
                    continue;
                }

                std::cout << "Unknown command: " << line << "\n";
                std::cout << "Type /help for available commands\n";
                continue;
            }

            // Pass non-command input to consumer
            input_queue.Push(line);
        }
    });

    // Consumer thread: process user messages (LLM calls happen here)
    while (true) {
        std::string combined = input_queue.WaitAndPopAll(interrupted_);
        if (combined.empty()) {
            break;
        }
        ProcessUserMessage(combined);
    }

    if (interrupted_) {
        std::cout << "\n[Interrupted]\n\n";
    }

    if (producer_thread.joinable()) {
        producer_thread.join();
    }

    LOG_INFO("Shutting down AiCode...");
    return 0;
}

void AgentCommander::Stop() {
    interrupted_ = true;
    if (agent_core_) {
        agent_core_->Stop();
    }
}

}  // namespace aicode
