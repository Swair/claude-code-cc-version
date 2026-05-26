// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "config/config.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "common/constants.h"
#include "common/log_wrapper.h"
#include "common/file_utils.h"
#include "managers/agent_role_loader.h"
#include "platform/platform.h"

namespace prosophor {

std::string ProsophorConfig::config_path_override_;
ProsophorConfig* ProsophorConfig::instance_ = nullptr;

ProsophorConfig& ProsophorConfig::GetInstance() {
    static ProsophorConfig instance;
    static bool initialized = false;

    if (!initialized) {
        std::string config_path = DefaultConfigPath();
        CreateDefaultConfig(config_path);

        try {
            instance = LoadFromFile(config_path);
        } catch (const std::exception& e) {
            LOG_WARN("Failed to load config, using defaults: {}", e.what());
        }
        initialized = true;
    }

    return instance;
}

const ModelConfig& ProsophorConfig::GetModelConfig() const {
    // Load default role to get its provider and agent config
    std::string primary_role = default_role.empty() ? "default" : default_role[0];
    std::string role_path = "config/.prosophor/roles/" + primary_role + ".json";
    if (FileExists(role_path)) {
        auto& loader = AgentRoleLoader::GetInstance();
        try {
            AgentRole role = loader.LoadRole(role_path);
            // Find the agent config from the role's provider
            auto prov_it = providers.find(role.provider_prot);
            if (prov_it != providers.end()) {
                auto& agent_map = prov_it->second.model_configs;
                // Try 1: role.model as key (agent name)
                auto agent_it = agent_map.find(role.model);
                if (agent_it == agent_map.end()) {
                    // Try 2: provider_name/model_name key
                    std::string full_key = role.provider_prot + "/" + role.model;
                    agent_it = agent_map.find(full_key);
                }
                if (agent_it != agent_map.end()) {
                    return agent_it->second;
                }
                // Fall back to provider's default model
                return prov_it->second.GetDefaultModel();
            }
        } catch (const std::exception& e) {
            LOG_WARN("Failed to load default role '{}', using fallback: {}", primary_role, e.what());
        }
    }

    // Fallback: use first provider's default model
    if (!providers.empty()) {
        return providers.begin()->second.GetDefaultModel();
    }

    static ModelConfig fallback_model;
    return fallback_model;
}

const ProviderConfig& ProsophorConfig::GetProvider(const std::string& name) const {
    auto it = providers.find(name);
    if (it != providers.end()) {
        return it->second;
    }
    static ProviderConfig default_provider;
    return default_provider;
}

const ModelConfig& ProviderConfig::GetDefaultModel() const {
    if (model_configs.empty()) {
        static ModelConfig default_model;
        return default_model;
    }
    // Array format: use first model; object format: try "default" key first, then first entry
    auto it = model_configs.find("default");
    if (it != model_configs.end()) {
        return it->second;
    }
    return model_configs.begin()->second;
}

bool ProviderConfig::FindEntryForModel(const std::string& provider_name,
                                        const std::string& model,
                                        std::string& out_base_url,
                                        std::string& out_api_key,
                                        int& out_timeout) const {
    for (const auto& entry : entries) {
        auto it = entry.models.find(model);
        if (it == entry.models.end()) {
            // Also try searching by model name in agent.config.model
            for (const auto& [k, v] : entry.models) {
                if (v.model == model) {
                    it = entry.models.find(k);
                    break;
                }
            }
        }
        if (it != entry.models.end()) {
            out_base_url = entry.base_url;
            out_api_key = entry.api_key;
            out_timeout = entry.timeout;
            return true;
        }
    }
    // Fallback: search by full key provider_name/model
    std::string full_key = provider_name + "/" + model;
    auto model_it = model_configs.find(full_key);
    if (model_it != model_configs.end()) {
        // Find matching entry
        for (const auto& entry : entries) {
            for (const auto& [k, v] : entry.models) {
                if (v.model == model || k == model) {
                    out_base_url = entry.base_url;
                    out_api_key = entry.api_key;
                    out_timeout = entry.timeout;
                    return true;
                }
            }
        }
    }
    return false;
}

// Substitutes environment variables in the form ${VAR_NAME}
static std::string SubstituteEnvVars(const std::string& input) {
    static const std::regex env_re(R"(\$\{([^}]+)\})");
    std::string result;
    auto begin = std::sregex_iterator(input.begin(), input.end(), env_re);
    auto end = std::sregex_iterator();

    size_t last_pos = 0;
    for (auto it = begin; it != end; ++it) {
        auto& match = *it;
        result.append(input, last_pos, match.position() - last_pos);
        std::string var_name = match[1].str();
        const char* env_val = std::getenv(var_name.c_str());
        if (env_val) {
            result.append(env_val);
        }
        last_pos = match.position() + match.length();
    }
    result.append(input, last_pos, std::string::npos);
    return result;
}

// Strips comments and trailing commas from JSON5
static std::string StripJson5(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    size_t i = 0;
    const size_t len = input.size();

    while (i < len) {
        if (input[i] == '"') {
            out += '"';
            ++i;
            while (i < len && input[i] != '"') {
                if (input[i] == '\\' && i + 1 < len) {
                    out += input[i];
                    out += input[i + 1];
                    i += 2;
                } else {
                    out += input[i];
                    ++i;
                }
            }
            if (i < len) {
                out += '"';
                ++i;
            }
            continue;
        }

        if (i + 1 < len && input[i] == '/' && input[i + 1] == '/') {
            i += 2;
            while (i < len && input[i] != '\n') ++i;
            continue;
        }

        if (i + 1 < len && input[i] == '/' && input[i + 1] == '*') {
            i += 2;
            while (i + 1 < len && !(input[i] == '*' && input[i + 1] == '/')) ++i;
            if (i + 1 < len) i += 2;
            continue;
        }

        out += input[i];
        ++i;
    }

    std::string result;
    result.reserve(out.size());
    for (size_t j = 0; j < out.size(); ++j) {
        if (out[j] == ',') {
            size_t k = j + 1;
            while (k < out.size() && (out[k] == ' ' || out[k] == '\t' ||
                                      out[k] == '\n' || out[k] == '\r')) {
                ++k;
            }
            if (k < out.size() && (out[k] == '}' || out[k] == ']')) {
                continue;
            }
        }
        result += out[j];
    }

    return result;
}

// Expands environment variables in JSON
static void ExpandEnvInJson(nlohmann::json& j) {
    if (j.is_string()) {
        auto& s = j.get_ref<std::string&>();
        if (s.find("${") != std::string::npos) {
            s = SubstituteEnvVars(s);
        }
    } else if (j.is_object()) {
        for (auto& [key, value] : j.items()) {
            ExpandEnvInJson(value);
        }
    } else if (j.is_array()) {
        for (auto& element : j) {
            ExpandEnvInJson(element);
        }
    }
}

ModelConfig ModelConfig::FromJson(const nlohmann::json& json) {
    ModelConfig config;
    config.name = json.value("name", "default");
    config.model = json.value("model", "claude-sonnet-4-6");
    config.temperature = json.value("temperature", kDefaultTemperature);
    config.max_tokens = json.value("max_tokens", json.value("maxTokens", kDefaultMaxTokens));
    config.context_window = json.value("context_window", json.value("contextWindow", kDefaultContextWindow));
    config.thinking = json.value("thinking", false);
    config.use_tools = json.value("use_tools", json.value("useTools", true));
    config.enable_streaming = json.value("enable_streaming", json.value("enableStreaming", true));
    config.auto_compact = json.value("auto_compact", json.value("autoCompact", true));
    config.compact_max_messages = json.value("compact_max_messages", json.value("compactMaxMessages", kDefaultCompactMaxMessages));
    config.compact_keep_recent = json.value("compact_keep_recent", json.value("compactKeepRecent", kDefaultCompactKeepRecent));
    config.compact_max_tokens = json.value("compact_max_tokens", json.value("compactMaxTokens", kDefaultCompactMaxTokens));
    return config;
}

ModelCost ModelCost::FromJson(const nlohmann::json& json) {
    ModelCost c;
    c.input = json.value("input", 0.0);
    c.output = json.value("output", 0.0);
    c.cache_read = json.value("cacheRead", json.value("cache_read", 0.0));
    c.cache_write = json.value("cacheWrite", json.value("cache_write", 0.0));
    return c;
}

ModelDefinition ModelDefinition::FromJson(const nlohmann::json& json) {
    ModelDefinition m;
    m.id = json.value("id", "");
    m.name = json.value("name", "");
    m.reasoning = json.value("reasoning", false);
    m.input = json.value("input", std::vector<std::string>{"text"});
    if (json.contains("cost") && json["cost"].is_object()) {
        m.cost = ModelCost::FromJson(json["cost"]);
    }
    m.context_window = json.value("contextWindow", json.value("context_window", 0));
    m.max_tokens = json.value("maxTokens", json.value("max_tokens", 0));
    return m;
}

ProviderConfig ProviderConfig::FromJson(const nlohmann::json& json) {
    ProviderConfig config;
    config.api_key = json.value("api_key", json.value("apiKey", ""));
    config.base_url = json.value("base_url", json.value("baseUrl", ""));
    config.timeout = json.value("timeout", kDefaultProviderTimeoutSec);

    // Parse models — supports array (preferred) and object (legacy) formats
    const auto& models_json_key = json.contains("models") ? "models" : "model";
    if (json.contains(models_json_key)) {
        const auto& aj = json[models_json_key];
        if (aj.is_array()) {
            for (const auto& item : aj) {
                ModelConfig agent = ModelConfig::FromJson(item);
                if (!agent.model.empty()) {
                    config.model_configs[agent.model] = agent;
                }
            }
        } else if (aj.is_object()) {
            for (const auto& [key, value] : aj.items()) {
                ModelConfig agent = ModelConfig::FromJson(value);
                agent.name = key;
                if (json.contains("tools_use")) {
                    agent.use_tools = json.value("tools_use", true);
                }
                config.model_configs[key] = agent;
            }
        }
    }

    if (json.contains("models") && json["models"].is_array()) {
        for (const auto& m : json["models"]) {
            config.models.push_back(ModelDefinition::FromJson(m));
        }
    }
    return config;
}

LlamacppModelConfig LlamacppModelConfig::FromJson(const nlohmann::json& json) {
    LlamacppModelConfig config;
    config.model_path = Platform::NormalizePath(json.value("model_path", ""));
    config.port = json.value("port", 8080);
    config.n_gpu_layers = json.value("n_gpu_layers", json.value("nGpuLayers", -1));
    config.n_threads = json.value("n_threads", json.value("nThreads", 0));
    config.auto_start = json.value("auto_start", json.value("autoStart", true));
    config.start_timeout_ms = json.value("start_timeout_ms", json.value("startTimeoutMs", 60000));
    config.server_path = json.value("server_path", json.value("serverPath", ""));
	    // Inference parameters
    config.n_ctx         = json.value("n_ctx",          4096);
    config.max_new_tokens = json.value("max_new_tokens", 2048);
    config.temperature   = json.value("temperature",    0.7f);
    config.top_p         = json.value("top_p",          0.95f);
    return config;
}

nlohmann::json LlamacppModelConfig::ToJson() const {
    nlohmann::json j;
    j["model_path"] = model_path;
    j["port"] = port;
    j["n_gpu_layers"] = n_gpu_layers;
    j["n_threads"] = n_threads;
    j["auto_start"] = auto_start;
    if (start_timeout_ms != 60000) j["start_timeout_ms"] = start_timeout_ms;
    if (!server_path.empty()) j["server_path"] = server_path;
	    j["n_ctx"]           = n_ctx;
    j["max_new_tokens"]  = max_new_tokens;
    j["temperature"]     = temperature;
    j["top_p"]           = top_p;
    return j;
}

TtsConfig TtsConfig::FromJson(const nlohmann::json& json) {
    TtsConfig config;
    config.enabled = json.value("enabled", true);
    config.backend = json.value("backend", "edge-tts");
    config.gs_url = json.value("gs_url", json.value("gsUrl", "http://127.0.0.1:9880"));
    config.gs_install_path = json.value("gs_install_path", json.value("gsInstallPath", ""));
    config.gs_auto_start = json.value("gs_auto_start", json.value("gsAutoStart", true));
    config.gs_port = json.value("gs_port", json.value("gsPort", 9880));
    config.gs_ref_audio_path = json.value("gs_ref_audio_path", json.value("gsRefAudioPath", ""));
    config.gs_ref_audio_text = json.value("gs_ref_audio_text", json.value("gsRefAudioText", ""));
    config.gs_ref_audio_lang = json.value("gs_ref_audio_lang", json.value("gsRefAudioLang", "zh"));
    config.gs_text_lang = json.value("gs_text_lang", json.value("gsTextLang", "zh"));
    // sherpa-onnx TTS
    config.sherpa_model_dir  = json.value("sherpa_model_dir",  "");
    config.sherpa_speaker_id = json.value("sherpa_speaker_id", 0);
    config.sherpa_speed      = json.value("sherpa_speed",       1.0f);
    return config;
}

AsrConfig AsrConfig::FromJson(const nlohmann::json& json) {
    AsrConfig config;
    config.enabled    = json.value("enabled",    false);
    config.backend    = json.value("backend",    std::string("sherpa-onnx"));
    config.model_dir  = json.value("model_dir",  std::string(""));
    config.script_path = json.value("script_path", std::string(""));
    config.language   = json.value("language",   std::string("zh"));
    config.n_threads  = json.value("n_threads",  4);
    return config;
}

ToolConfig ToolConfig::FromJson(const nlohmann::json& json) {
    ToolConfig config;
    config.enabled = json.value("enabled", true);
    config.allowed_paths = json.value("allowed_paths", std::vector<std::string>{});
    config.denied_paths = json.value("denied_paths", std::vector<std::string>{});
    config.allowed_cmds = json.value("allowed_cmds", std::vector<std::string>{});
    config.denied_cmds = json.value("denied_cmds", std::vector<std::string>{});
    config.timeout = json.value("timeout", kDefaultToolTimeoutSec);
    return config;
}

SkillEntryConfig SkillEntryConfig::FromJson(const nlohmann::json& json) {
    SkillEntryConfig config;
    config.enabled = json.value("enabled", true);
    return config;
}

SkillsLoadConfig SkillsLoadConfig::FromJson(const nlohmann::json& json) {
    SkillsLoadConfig config;
    config.extra_dirs = json.value("extraDirs", std::vector<std::string>{});
    return config;
}

SkillsConfig SkillsConfig::FromJson(const nlohmann::json& json) {
    SkillsConfig config;
    config.path = json.value("path", "");
    config.auto_approve = json.value("auto_approve", json.value("autoApprove", std::vector<std::string>{}));
    if (json.contains("configs") && json["configs"].is_object()) {
        config.configs = json["configs"];
    }
    if (json.contains("load") && json["load"].is_object()) {
        config.load = SkillsLoadConfig::FromJson(json["load"]);
    }
    if (!config.path.empty() && config.load.extra_dirs.empty()) {
        config.load.extra_dirs.push_back(config.path);
    }
    if (json.contains("entries") && json["entries"].is_object()) {
        for (const auto& [key, value] : json["entries"].items()) {
            config.entries[key] = SkillEntryConfig::FromJson(value);
        }
    }
    return config;
}

ProsophorConfig ProsophorConfig::FromJson(const nlohmann::json& json) {
    nlohmann::json expanded = json;
    ExpandEnvInJson(expanded);

    ProsophorConfig config;
    config.log_level = json.value("log_level", json.value("logLevel", "info"));
    // default_role can be string (backwards compat) or array of strings
    if (json.contains("default_role") && json["default_role"].is_array()) {
        config.default_role = json["default_role"].get<std::vector<std::string>>();
    } else {
        std::string single = json.value("default_role", json.value("defaultRole", "default"));
        config.default_role = {single};
    }
    config.enable_summary = json.value("enable_summary", true);
    config.sprite_assets_dir = ExpandHome(json.value("sprite_assets_dir", "~/.prosophor/assets"));

    if (json.contains("providers") && json["providers"].is_object()) {
        for (const auto& [key, value] : json["providers"].items()) {
            if (value.is_array()) {
                // Array format: keep all entries, key models by provider_name/model_name
                ProviderConfig merged_config;
                bool first = true;
                for (const auto& entry : value) {
                    ProviderConfig entry_config = ProviderConfig::FromJson(entry);

                    if (first) {
                        merged_config = entry_config;
                        first = false;
                    }

                    // Store entry for lookup by FindEntryForModel
                    ProviderEntryConfig e;
                    e.api_key = entry_config.api_key;
                    e.base_url = entry_config.base_url;
                    e.timeout = entry_config.timeout;
                    e.models = entry_config.model_configs;
                    merged_config.entries.push_back(std::move(e));
                    // Key models as provider_name/model_name
                    for (auto& [model_name, model_config] : entry_config.model_configs) {
                        std::string model_key = key + "/" + model_config.model;
                        merged_config.model_configs[model_key] = model_config;
                    }
                    // Merge models
                    for (auto& model : entry_config.models) {
                        merged_config.models.push_back(model);
                    }
                }
                config.providers[key] = merged_config;
            } else {
                // Object format: use directly
                config.providers[key] = ProviderConfig::FromJson(value);
            }
        }
    }
    if (json.contains("security") && json["security"].is_object()) {
        config.security = SecurityConfig::FromJson(json["security"]);
    }
    if (json.contains("tools") && json["tools"].is_object()) {
        config.tools = ToolConfig::FromJson(json["tools"]);
    }
    if (json.contains("skills") && json["skills"].is_object()) {
        config.skills = SkillsConfig::FromJson(json["skills"]);
    }
    if (json.contains("tts") && json["tts"].is_object()) {
        config.tts = TtsConfig::FromJson(json["tts"]);
    }
    if (json.contains("asr") && json["asr"].is_object()) {
        config.asr = AsrConfig::FromJson(json["asr"]);
    }
    if (json.contains("llamacpp_models") && json["llamacpp_models"].is_array()) {
        for (const auto& m : json["llamacpp_models"]) {
            config.llamacpp_models.push_back(LlamacppModelConfig::FromJson(m));
        }
    }
    return config;
}

ProsophorConfig ProsophorConfig::LoadFromFile(const std::string& filepath) {
    std::string expanded_path = ExpandHome(filepath);

    if (!FileExists(expanded_path)) {
        throw std::runtime_error("Config file not found: " + expanded_path);
    }

    std::ifstream file(expanded_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + expanded_path);
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    std::string clean = StripJson5(content);
    nlohmann::json json = nlohmann::json::parse(clean);

    return FromJson(json);
}

std::string ProsophorConfig::ExpandHome(const std::string& path) {
    std::string expanded = path;
    if (expanded.size() >= 2 && expanded.substr(0, 2) == "~/") {
        std::string home = GetHomeDir();
        if (!home.empty()) {
            expanded = home + expanded.substr(1);
        }
    }
    return expanded;
}

std::string ProsophorConfig::DefaultConfigPath() {
    if (!config_path_override_.empty()) {
        return config_path_override_;
    }
    // Support environment variable override: export PROSOPHOR_CONFIG="/path/to/config.json"
    const char* env_path = std::getenv("PROSOPHOR_CONFIG");
    if (env_path != nullptr && env_path[0] != '\0') {
        return env_path;
    }
    const auto user_config = ExpandHome("~/.prosophor/settings.json");
    if (FileExists(user_config)) {
        return user_config;
    }

    std::string exe_path = Platform::GetSelfExePath();
    if (!exe_path.empty()) {
        auto local_config = std::filesystem::path(exe_path).parent_path() / ".prosophor" / "settings.json";
        if (FileExists(local_config.string())) {
            return local_config.string();
        }
    }
    return user_config;
}

std::filesystem::path ProsophorConfig::InstallConfigDir() {
    std::string exe_path = Platform::GetSelfExePath();
    if (!exe_path.empty()) {
        auto local_dir = std::filesystem::path(exe_path).parent_path() / ".prosophor";
        if (DirExists(local_dir.string())) {
            return local_dir;
        }
    }
    return {};
}

std::filesystem::path ProsophorConfig::BaseDir() {
    // Support environment variable override
    const char* env_path = std::getenv("PROSOPHOR_CONFIG");
    if (env_path != nullptr && env_path[0] != '\0') {
        return std::filesystem::path(env_path).parent_path();
    }
    // Use user home directory (always writable)
    return ExpandHome("~/.prosophor");
}

// Creates a default config file with documentation comments
void ProsophorConfig::CreateDefaultConfig(const std::string& filepath) {
    std::string expanded_path = ExpandHome(filepath);

    // Create directory if it doesn't exist
    std::filesystem::path parent_dir = std::filesystem::path(expanded_path).parent_path();
    std::filesystem::create_directories(parent_dir);

    // Don't overwrite existing config
    if (FileExists(expanded_path)) {
        return;
    }

    // Check if there's a demo config at config/.prosophor/settings.json
    std::string demo_config_path = "config/.prosophor/settings.json";
    if (FileExists(demo_config_path)) {
        // Copy demo config to ~/.prosophor/settings.json
        try {
            std::filesystem::copy_file(demo_config_path, expanded_path,
                                        std::filesystem::copy_options::overwrite_existing);
            LOG_DEBUG("Created config from demo: {}", demo_config_path);
            return;
        } catch (const std::exception& e) {
            LOG_WARN("Failed to copy demo config: {}", e.what());
            // Fall through to create default config
        }
    }

    std::ofstream file(expanded_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to create config file: " + expanded_path);
    }

    // Write config template with comments
    file << R"(// Prosophor Configuration File
// Path: ~/.prosophor/settings.json
//
// Structure:
//   providers.<provider_name>.models.<model_name> = { model, temperature, ... }
//   Roles are defined in config/.prosophor/roles/*.json
//
// Example:
//   providers.anthropic.models.default.model = "qwen3.5-plus"
//   providers.deepseek.models.pro.model = "deepseek-v4-pro"

{
  "default_role": ["default"],        // Default roles (SDL: one sprite per role, TUI: first only)
  "log_level": "info",

  // Provider configuration (array format supports multiple instances)
  "providers": {
    "anthropic": [
      {
        "api_key": "${ANTHROPIC_API_KEY}",
        "base_url": "https://api.anthropic.com",
        "timeout": 60,

        // Multiple model configurations
        "models": {
          "default": {
            "model": "claude-sonnet-4-6",
            "temperature": 0.7,
            "max_tokens": 8192,
            "context_window": 128000,
            "use_tools": true,
            "thinking": false,
            "enable_streaming": true
          }
        }
      }
    ],

    // DeepSeek (OpenAI-compatible protocol)
    "deepseek": [
      {
        "api_key": "${DEEPSEEK_API_KEY}",
        "base_url": "https://api.deepseek.com/chat/completions",
        "timeout": 60,
        "models": {
          "default": {
            "model": "deepseek-chat",
            "temperature": 0.7,
            "max_tokens": 8192,
            "context_window": 128000,
            "use_tools": true
          }
        }
      }
    ]
  },

  "security": {
    "permission_level": "auto",
    "allow_local_execute": true
  },

  "tools": {
    "enabled": true,
    "timeout": 60
  },

  "skills": {
    "path": "./skills",
    "auto_approve": ["read_file", "grep"]
  }
}
)";

    file.close();
}

int ModelConfig::DynamicMaxIterations() const {
    if (context_window <= kContextWindow32K) return kMinMaxIterations;
    if (context_window >= kContextWindow200K) return kMaxMaxIterations;

    double ratio = static_cast<double>(context_window - kContextWindow32K) /
                   (kContextWindow200K - kContextWindow32K);
    return kMinMaxIterations +
           static_cast<int>(ratio * (kMaxMaxIterations - kMinMaxIterations));
}

// ==================== ProsophorConfig JSON Serialization ====================

nlohmann::json ProsophorConfig::ToJson() const {
    nlohmann::json json = nlohmann::json::object();

    json["default_role"] = default_role;  // nlohmann_json: vector → JSON array
    json["enable_summary"] = enable_summary;

    // Serialize local models (before providers, before log_level)
    if (!llamacpp_models.empty()) {
        nlohmann::json models_json = nlohmann::json::array();
        for (const auto& m : llamacpp_models) {
            models_json.push_back(m.ToJson());
        }
        json["llamacpp_models"] = models_json;
    }

    json["log_level"] = log_level;

    // Serialize providers (each provider name → array of entries)
    nlohmann::json providers_json = nlohmann::json::object();
    for (const auto& [name, config] : providers) {
        nlohmann::json entries_json = nlohmann::json::array();
        if (!config.entries.empty()) {
            for (const auto& entry : config.entries) {
                nlohmann::json models_json = nlohmann::json::array();
                for (const auto& [model_name, model_config] : entry.models) {
                    nlohmann::json model_json;
                    model_json["context_window"] = model_config.context_window;
                    model_json["enable_streaming"] = model_config.enable_streaming;
                    model_json["max_tokens"] = model_config.max_tokens;
                    model_json["model"] = model_config.model;
                    model_json["temperature"] = model_config.temperature;
                    model_json["thinking"] = model_config.thinking;
                    models_json.push_back(model_json);
                }
                nlohmann::json entry_json;
                entry_json["models"] = models_json;
                entry_json["api_key"] = entry.api_key;
                entry_json["base_url"] = entry.base_url;
                entry_json["timeout"] = entry.timeout;
                entries_json.push_back(entry_json);
            }
        } else {
            // Fallback for entries that were never parsed (shouldn't happen)
            nlohmann::json models_json = nlohmann::json::array();
            for (const auto& [model_name, model_config] : config.model_configs) {
                nlohmann::json model_json;
                model_json["context_window"] = model_config.context_window;
                model_json["enable_streaming"] = model_config.enable_streaming;
                model_json["max_tokens"] = model_config.max_tokens;
                model_json["model"] = model_config.model;
                model_json["temperature"] = model_config.temperature;
                model_json["thinking"] = model_config.thinking;
                models_json.push_back(model_json);
            }
            nlohmann::json entry_json;
            entry_json["models"] = models_json;
            entry_json["api_key"] = config.api_key;
            entry_json["base_url"] = config.base_url;
            entry_json["timeout"] = config.timeout;
            entries_json.push_back(entry_json);
        }
        providers_json[name] = entries_json;
    }
    json["providers"] = providers_json;

    // Serialize security
    nlohmann::json security_json = nlohmann::json::object();
    security_json["permission_level"] = security.permission_level;
    security_json["allow_local_execute"] = security.allow_local_execute;
    json["security"] = security_json;

    json["sprite_assets_dir"] = sprite_assets_dir;

    // Serialize tools
    nlohmann::json tools_json = nlohmann::json::object();
    tools_json["enabled"] = tools.enabled;
    tools_json["timeout"] = tools.timeout;
    if (!tools.allowed_paths.empty()) tools_json["allowed_paths"] = tools.allowed_paths;
    if (!tools.denied_paths.empty()) tools_json["denied_paths"] = tools.denied_paths;
    if (!tools.allowed_cmds.empty()) tools_json["allowed_cmds"] = tools.allowed_cmds;
    if (!tools.denied_cmds.empty()) tools_json["denied_cmds"] = tools.denied_cmds;
    json["tools"] = tools_json;

    // Serialize TTS
    nlohmann::json tts_json = nlohmann::json::object();
    tts_json["enabled"] = tts.enabled;
    nlohmann::json edge_tts_json = nlohmann::json::object();
    edge_tts_json["backend"] = "edge-tts";
    edge_tts_json["gs_url"] = tts.gs_url;
    edge_tts_json["gs_auto_start"] = tts.gs_auto_start;

    tts_json["provider"] = nlohmann::json::array({edge_tts_json});
    json["tts"] = tts_json;

    nlohmann::json asr_json = nlohmann::json::object();
    asr_json["enabled"] = asr.enabled;
    asr_json["backend"] = asr.backend;
    asr_json["script_path"] = asr.script_path;
    asr_json["model_dir"] = asr.model_dir;
    json["asr"] = asr_json;

    return json;
}

void ProsophorConfig::SaveToFile(const std::string& filepath) const {
    auto json = ToJson();
    prosophor::WriteJson(filepath, json, 2);
    LOG_DEBUG("Configuration saved to {}", filepath);
}

}  // namespace prosophor
