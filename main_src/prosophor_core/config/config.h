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
struct ModelConfig;
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

/// Model behavior configuration
struct ModelConfig {
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

    static ModelConfig FromJson(const nlohmann::json& json);
    int DynamicMaxIterations() const;
};

/// Configuration for a single provider entry (each array item keeps its own api_key/base_url)
struct ProviderEntryConfig {
    std::string api_key;
    std::string base_url;
    int timeout = 30;
    std::unordered_map<std::string, ModelConfig> models;
};

/// Configuration for an LLM provider
/// models key format: "{provider_name}/{model_name}" → model params
struct ProviderConfig {
    std::string api_key;
    std::string base_url;
    int timeout = 30;

    // models key: "{provider_name}/{model_name}"
    std::unordered_map<std::string, ModelConfig> model_configs;

    std::vector<ModelDefinition> models;

    // All entries from the config array (each keeps its own api_key/base_url)
    std::vector<ProviderEntryConfig> entries;

    static ProviderConfig FromJson(const nlohmann::json& json);

    // Get default model config
    const ModelConfig& GetDefaultModel() const;

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

/// Configuration for local model (llama.cpp in-process)
struct LlamacppModelConfig {
    static LlamacppModelConfig FromJson(const nlohmann::json& json);
    nlohmann::json ToJson() const;
    std::string model_path;         // Path to GGUF model file
    int port = 8080;                // Legacy: kept for config compatibility
    int n_gpu_layers = 0;           // GPU layers (-1 = all on GPU, 0 = CPU only)
    int n_threads = 0;              // CPU threads (0 = auto-detect)
    bool auto_start = true;         // Load model on startup
    int start_timeout_ms = 300000;  // Legacy: kept for config compatibility
    std::string server_path;        // Legacy: kept for config compatibility

    // Inference parameters
    int   n_ctx        = 4096;   // Context window size (tokens)
    int   max_new_tokens = 2048; // Max generated tokens per response
    float temperature  = 0.7f;   // Sampling temperature (0.0 = greedy, 1.0 = creative)
    float top_p        = 0.95f;  // Nucleus sampling probability

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
    std::string backend = "edge-tts";  // "edge-tts" | "gpt-sovits" | "sherpa-onnx"

    // GPT-SoVITS settings
    std::string gs_url = "http://127.0.0.1:9880";
    std::string gs_install_path;
    bool gs_auto_start = true;
    int gs_port = 9880;
    std::string gs_ref_audio_path;
    std::string gs_ref_audio_text;
    std::string gs_ref_audio_lang = "zh";
    std::string gs_text_lang = "zh";

    // sherpa-onnx VITS settings
    std::string sherpa_model_dir;   // dir with model.onnx + tokens.txt [+ lexicon.txt]
    int   sherpa_speaker_id = 0;
    float sherpa_speed      = 1.0f;

    static TtsConfig FromJson(const nlohmann::json& json);
};

/// Configuration for ASR (Automatic Speech Recognition)
struct AsrConfig {
    bool enabled = false;
    std::string backend = "sherpa-onnx";  // "sherpa-onnx" | "sensevoice" (python subprocess)
    std::string model_dir;                // Path to sherpa-onnx model files
    std::string script_path;              // Path to run_asr.py (sensevoice backend only)
    std::string language  = "zh";         // Recognition language
    int n_threads = 4;

    static AsrConfig FromJson(const nlohmann::json& json);
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
    AsrConfig asr;
    std::vector<LlamacppModelConfig> llamacpp_models;

    /// Get singleton instance
    static ProsophorConfig& GetInstance();

    /// Get current agent config from default provider
    const ModelConfig& GetModelConfig() const;

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
