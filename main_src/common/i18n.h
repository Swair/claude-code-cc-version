// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "common/noncopyable.h"
#include <string>
#include <unordered_map>
#include <memory>

namespace prosophor {

/// I18n: lightweight internationalisation singleton.
/// Loads key→value pairs from JSON translation files at
/// PROSOPHOR_SOURCE_DIR/config/lang/{lang}.json  and returns
/// translated strings via `Get(key)`.
class I18n : public Noncopyable {
public:
    static I18n& Instance();

    /// Set active language (e.g. "zh-CN", "en").
    /// Reloads translations from disk if not already cached.
    void SetLanguage(const std::string& lang);

    /// Get active language code.
    const std::string& GetLanguage() const { return lang_; }

    /// Translate a key to the active language.
    /// Returns key itself when no translation is found.
    const std::string& Get(const std::string& key) const;

    /// Convenience: same as Get() — for terse call sites.
    const std::string& operator()(const std::string& key) const { return Get(key); }

    /// Pre-load a language (called once at startup).
    void Init(const std::string& default_lang = "zh-CN");

private:
    I18n() = default;
    ~I18n() = default;

    void Load(const std::string& lang);

    mutable std::string missing_fallback_;
    std::string lang_{"zh-CN"};
    std::unordered_map<std::string, std::string> entries_;
    std::unordered_map<std::string, std::unique_ptr<std::unordered_map<std::string, std::string>>> cache_;
};

} // namespace prosophor
