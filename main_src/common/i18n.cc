// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "common/i18n.h"
#include "common/log_wrapper.h"
#include "common/file_utils.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>

namespace prosophor {

I18n& I18n::Instance() {
    static I18n instance;
    return instance;
}

void I18n::Init(const std::string& default_lang) {
    lang_ = default_lang;
    Load(lang_);
}

void I18n::SetLanguage(const std::string& lang) {
    if (lang == lang_) return;

    // Check cache first
    auto it = cache_.find(lang);
    if (it != cache_.end()) {
        entries_ = *it->second;
        lang_ = lang;
        LOG_INFO("[I18n] Switched to language '{}' (cached)", lang);
        return;
    }

    Load(lang);
}

void I18n::Load(const std::string& lang) {
    std::string path = GetHomeDir() + "/.prosophor/lang/" + lang + ".json";
    if (!std::filesystem::exists(path)) {
        LOG_WARN("[I18n] Translation file not found: {}, keeping current", path);
        return;
    }

    try {
        std::ifstream ifs(path);
        nlohmann::json j;
        ifs >> j;

        std::unordered_map<std::string, std::string> map;
        for (auto& [key, value] : j.items()) {
            if (value.is_string()) {
                map[key] = value.get<std::string>();
            }
        }

        // Cache and apply
        cache_[lang] = std::make_unique<std::unordered_map<std::string, std::string>>(map);
        entries_ = std::move(map);
        lang_ = lang;
        LOG_INFO("[I18n] Loaded {} translations for '{}'", entries_.size(), lang);
    } catch (const std::exception& e) {
        LOG_ERROR("[I18n] Failed to load '{}': {}", path, e.what());
    }
}

const std::string& I18n::Get(const std::string& key) const {
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        return it->second;
    }
    missing_fallback_ = key;
    return missing_fallback_;
}

} // namespace prosophor
