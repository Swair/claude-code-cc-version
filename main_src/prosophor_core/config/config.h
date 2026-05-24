// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>

#include <nlohmann/json.hpp>

namespace prosophor {

// Forward declarations
struct AgentConfig;
struct ModelCost;
struct ModelDefinition;
struct ProviderConfig;
struct ToolConfig;
struct SkillEntryConfig;
struct SkillsLoadConfig;
struct SkillsConfig;
struct SecurityConfig;
struct ProsophorConfig;

/// Cost information for a model
struct ModelCost {
    double input = 0;
    double output = 0;
    double cache_read = 0;
    double cache_write = 0;

    static ModelCost FromJson(const nlohmann::json& json);
};

/// Definition of an LLM model
struct ModelDefinition {
    std::string id;
    std::string name;
    bool reasoning = false;
    std::vector<std::string> input = {"text"};
    ModelCost cost;
    int context_window = 0;
    int max_tokens = 0;

    static ModelDefinition FromJson(const nlohmann::json& json);
};

/// Agent behavior configuration
struct AgentConfig {
    std::string name = "default";
    std::string model = "claude-sonnet-4-6";
    double temperature = 0.7;
    int max_tokens = 8192;
    int context_window = 128000;
    bool thinking = false;
    bool use_tools = true;
    bool enable_streaming = true;  // Whether to use streaming for responses

    // Auto-compaction
    bool auto_compact = true;
    int compact_max_messages = 100;
    int compact_keep_recent = 20;
    int compact_max_tokens = 100000;

    static AgentConfig FromJson(const nlohmann::json& json);
    int DynamicMaxIterations() const;
};

/// Configuration for a single provider entry (each array item keeps its own api_key/base_url)
struct ProviderEntryConfig {
    std::string api_key;
    std::string base_url;
    int timeout = 30;
    std::unordered_map<std::string, AgentConfig> agents;
};

/// Configuration for an LLM provider
/// agents key format: "{provider_name}/{model_name}" → agent params
struct ProviderConfig {
    std::string api_key;
    std::string base_url;
    int timeout = 30;

    // agents key: "{provider_name}/{model_name}"
    std::unordered_map<std::string, AgentConfig> agents;

    std::vector<ModelDefinition> models;

    // All entries from the config array (each keeps its own api_key/base_url)
    std::vector<ProviderEntryConfig> entries;

    static ProviderConfig FromJson(const nlohmann::json& json);

    // Get default agent
    const AgentConfig& GetDefaultAgent() const;

    // Find entry by model name and return its base_url and api_key
    bool FindEntryForModel(const std::string& provider_name,
                           const std::string& model,
                           std::string& out_base_url,
                           std::string& out_api_key,
                           int& out_timeout) const;
};

/// Tool configuration settings
struct ToolConfig {
    bool enabled = true;
    std::vector<std::string> allowed_paths;
    std::vector<std::string> denied_paths;
    std::vector<std::string> allowed_cmds;
    std::vector<std::string> denied_cmds;
    int timeout = 60;

    static ToolConfig FromJson(const nlohmann::json& json);
};

/// Configuration for a single skill
struct SkillEntryConfig {
    bool enabled = true;

    static SkillEntryConfig FromJson(const nlohmann::json& json);
};

/// Configuration for loading skills
struct SkillsLoadConfig {
    std::vector<std::string> extra_dirs;

    static SkillsLoadConfig FromJson(const nlohmann::json& json);
};

/// Configuration for all skills
struct SkillsConfig {
    std::string path;
    std::vector<std::string> auto_approve;
    SkillsLoadConfig load;
    std::unordered_map<std::string, SkillEntryConfig> entries;
    nlohmann::json configs;

    static SkillsConfig FromJson(const nlohmann::json& json);
};

/// Configuration for local model server (llama-server)
struct LocalModelConfig {
    std::string model_path;    // Path to GGUF model file
    std::string model_path_for_win; // Windows-specific model path (overrides model_path on Win32)
    int port = 8080;           // Server port
    int n_gpu_layers = -1;     // GPU layers (-1 = all, 0 = CPU only)
    int n_threads = 0;         // Threads (0 = auto)
    bool auto_start = true;    // Auto-start with prosophor
    int start_timeout_ms = 300000; // Timeout for server startup (ms)
    std::string server_path;   // Path to llama-server binary (auto-detected if empty)

    bool IsValid() const { return !model_path.empty(); }

    static LocalModelConfig FromJson(const nlohmann::json& json);
    nlohmann::json ToJson() const;
};

/// Security configuration settings
struct SecurityConfig {
    std::string permission_level = "auto";
    bool allow_local_execute = true;

    static SecurityConfig FromJson(const nlohmann::json& json) {
        SecurityConfig c;
        c.permission_level = json.value("permission_level", json.value("permissionLevel", "auto"));
        c.allow_local_execute = json.value("allow_local_execute", json.value("allowLocalExecute", true));
        return c;
    }
};

/// Configuration for TTS (Text-to-Speech)
struct TtsConfig {
    bool enabled = true;
    std::string backend = "edge-tts";  // "edge-tts" or "gpt-sovits"

    // GPT-SoVITS settings
    std::string gs_url = "http://127.0.0.1:9880";
    std::string gs_install_path;       // Path to GPT-SoVITS installation dir
    bool gs_auto_start = true;         // Auto-start api_v2.py as subprocess
    int gs_port = 9880;
    std::string gs_ref_audio_path;
    std::string gs_ref_audio_text;
    std::string gs_ref_audio_lang = "zh";
    std::string gs_text_lang = "zh";

    static TtsConfig FromJson(const nlohmann::json& json);
};

/// Top-level Prosophor configuration
struct ProsophorConfig {
    std::string log_level = "info";
    std::vector<std::string> default_role = {"default"};  // Default roles (SDL: one sprite per role, TUI: first only)

    bool enable_summary = true;      // 是否启用对话摘要（system prompt 指令 + 摘要提取循环）

    std::string sprite_assets_dir;   // sprite 资源基目录 ~/.prosophor/assets，按 {sprite_id}/ 组织

    SecurityConfig security;
    std::unordered_map<std::string, ProviderConfig> providers;
    ToolConfig tools;
    SkillsConfig skills;
    TtsConfig tts;
    std::vector<LocalModelConfig> local_models;

    /// Get singleton instance
    static ProsophorConfig& GetInstance();

    /// Get current agent config from default provider
    const AgentConfig& GetAgentConfig() const;

    /// Get provider config
    const ProviderConfig& GetProvider(const std::string& name = "anthropic") const;

    /// Load config from file
    static ProsophorConfig FromJson(const nlohmann::json& json);
    static ProsophorConfig LoadFromFile(const std::string& filepath);
    static std::string ExpandHome(const std::string& path);
    static std::string DefaultConfigPath();
    static std::filesystem::path BaseDir();
    /// Directory containing shipped read-only config (exe-adjacent .prosophor/)
    static std::filesystem::path InstallConfigDir();
    static void CreateDefaultConfig(const std::string& filepath = DefaultConfigPath());

    /// Save config to file
    void SaveToFile(const std::string& filepath = DefaultConfigPath()) const;

    /// Convert to JSON
    nlohmann::json ToJson() const;

 private:
    static std::string config_path_override_;
    static ProsophorConfig* instance_;
};

}  // namespace prosophor
