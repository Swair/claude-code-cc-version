// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0

#include "virtual_sprite/chat_window.h"
#include "virtual_sprite/panels/panel_helpers.h"
#include "virtual_sprite/sprite.h"
#include "virtual_sprite/sprite_manager.h"
#include "virtual_sprite/layout_config.h"
#include "agent_engine.h"
#include "components/chat_panel.h"
#include "media_engine/ui_component/input_panel.h"
#include "media_engine/media_engine.h"
#include "common/i18n.h"
#include "common/file_utils.h"

#include "platform/platform.h"
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <unordered_map>
#include <cstdlib>

namespace fs = std::filesystem;
namespace prosophor {

namespace {

// ============================================================================
// Cache scanner — targeted mainstream program cache scan
// ============================================================================

struct CacheEntry {
    std::string name;        // display name (e.g. "Chrome Cache")
    std::string path;        // full path on disk
    std::string category;    // category key: "browser", "chat", "ide", "devtool", "system"
    uint64_t size = 0;
    bool exists = false;
};

struct CacheCategory {
    std::string key;         // category key
    std::string label;       // translated label
    std::vector<CacheEntry> entries;
    uint64_t total_size = 0;
};

struct ScanState {
    std::atomic<bool> scanning{false};
    std::atomic<bool> cancel{false};
    std::atomic<int> progress{0};
    std::atomic<int> total_steps{0};
    std::string current_item;

    std::vector<CacheCategory> categories;
    uint64_t grand_total_size = 0;
    bool has_results = false;
    bool report_sent = false;  // true after scan results auto-sent to agent
};

static ScanState s_state;
static bool s_show_deleted = false;

// ── Helpers ──

std::string FmtSize(uint64_t bytes) {
    const char* u[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double v = (double)bytes;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << v << " " << u[i];
    return os.str();
}

uint64_t ComputeDirSize(const fs::path& path, int max_depth = 5) {
    uint64_t total = 0;
    std::error_code ec;
    try {
        std::function<void(const fs::path&, int)> walk =
            [&](const fs::path& p, int d) {
                if (d > max_depth || s_state.cancel.load()) return;
                for (auto& e : fs::directory_iterator(p, ec)) {
                    if (ec) { ec.clear(); continue; }
                    if (s_state.cancel.load()) return;
                    if (e.is_regular_file(ec)) {
                        total += e.file_size(ec);
                        ec.clear();
                    } else if (e.is_directory(ec)) {
                        ec.clear();
                        walk(e.path(), d + 1);
                    }
                }
            };
        walk(path, 0);
    } catch (...) {}
    return total;
}

// Check if a path exists and return its total cached size
std::pair<bool, uint64_t> CheckCacheDir(const std::string& path_str) {
    std::error_code ec;
    fs::path p(path_str);
    if (!fs::exists(p, ec) || ec) return {false, 0};
    uint64_t sz = ComputeDirSize(p);
    return {true, sz};
}

// ── Cache definitions come from Platform::GetWellKnownCacheDirs() ──

// ── Scan all known cache locations ──
void DoScan() {
    s_state.scanning.store(true);
    s_state.cancel.store(false);
    s_state.progress.store(0);

    auto cache_dirs = Platform::GetWellKnownCacheDirs();
    s_state.total_steps.store((int)cache_dirs.size());
    s_state.grand_total_size = 0;
    s_state.has_results = false;
    s_state.report_sent = false;

    std::unordered_map<std::string, std::vector<CacheEntry>> cat_map;

    for (size_t i = 0; i < cache_dirs.size(); ++i) {
        if (s_state.cancel.load()) break;

        const auto& def = cache_dirs[i];
        s_state.current_item = def.name;
        s_state.progress.store((int)i + 1);

        std::string real_path = Platform::ExpandEnv(def.raw_path);

        auto [exists, size] = CheckCacheDir(real_path);
        CacheEntry entry{def.name, real_path, def.category, size, exists};
        cat_map[def.category].push_back(std::move(entry));
    }

    // Build categorized results
    s_state.categories.clear();
    uint64_t grand_total = 0;

    auto add_cat = [&](const std::string& key, const std::string& label) {
        auto it = cat_map.find(key);
        if (it == cat_map.end() || it->second.empty()) return;
        CacheCategory cat;
        cat.key = key;
        cat.label = label;
        cat.total_size = 0;
        for (auto& e : it->second) {
            if (e.exists) {
                cat.total_size += e.size;
                grand_total += e.size;
            }
            cat.entries.push_back(std::move(e));
        }
        std::sort(cat.entries.begin(), cat.entries.end(),
            [](const CacheEntry& a, const CacheEntry& b) {
                return a.size > b.size;
            });
        s_state.categories.push_back(std::move(cat));
    };

    // Order: system, browser, chat, ide, devtool
    auto& L = I18n::Instance();
    add_cat("system",  L.Get("cache_system"));
    add_cat("browser", L.Get("cache_browser"));
    add_cat("chat",    L.Get("cache_chat"));
    add_cat("ide",     L.Get("cache_ide"));
    add_cat("devtool", L.Get("cache_devtool"));

    s_state.grand_total_size = grand_total;
    s_state.has_results = true;
    s_state.scanning.store(false);
    s_state.current_item.clear();
}

// ── Build a text report of scan results for the AI agent ──
std::string BuildScanReport() {
    std::ostringstream os;
    os << "📊 缓存扫描报告\n\n";
    int total_items = 0;
    for (auto& cat : s_state.categories) {
        os << "【" << cat.label << "】(" << FmtSize(cat.total_size) << ")\n";
        for (auto& e : cat.entries) {
            if (!e.exists) continue;
            os << "  - " << e.name << ": " << FmtSize(e.size) << "\n";
            ++total_items;
        }
        os << "\n";
    }
    os << "总计可清理空间: " << FmtSize(s_state.grand_total_size) << "\n";
    os << "涉及 " << total_items << " 个缓存目录\n\n";
    os << "请分析以上缓存项，给出清理建议：哪些可以安全清理，哪些建议保留。";
    return os.str();
}

// ── Delete a single cache entry ──
bool DeleteCacheEntry(CacheEntry& entry) {
    std::error_code ec;
    if (!fs::exists(entry.path, ec) || ec) return true;
    fs::remove_all(entry.path, ec);
    if (ec) return false;
    entry.exists = false;
    entry.size = 0;
    return true;
}

// ── Delete all caches in a category ──
struct DeleteResult {
    std::string name;
    bool success;
};
std::vector<DeleteResult> DeleteCategory(CacheCategory& cat) {
    std::vector<DeleteResult> results;
    for (auto& entry : cat.entries) {
        if (!entry.exists) continue;
        bool ok = DeleteCacheEntry(entry);
        results.push_back({entry.name, ok});
    }
    cat.total_size = 0;
    for (auto& e : cat.entries) {
        if (e.exists) cat.total_size += e.size;
    }
    return results;
}

} // anonymous namespace

// ============================================================================
// RenderComputerOrganizeView — Main panel entry point
// ============================================================================

void ChatWindow::RenderComputerOrganizeView(int cont_x, int cont_y,
                                             int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    float sm = Spacing();
    PanelContainer f(cont_x, cont_y, cont_w, cont_h,
                     L.Get("view_computer_organize").c_str());

    // Split: left tool panel, right chat with agent
    auto split = PanelContainer::SplitRight(f.a, 340.0f);

    // ── Left: Cache scan panel ──
    {
        auto _scroll = PanelContainer::BeginScroll(split.main, 0, 4.0f, true);
        // local coordinates inside the scrollable child window
        float pad = 4.0f * sm;
        float lx = pad;               // local X
        float ly = 12.0f * sm;        // local Y (top padding)
        float w = split.main.w;       // local width
        float lh = 22.0f * sm;

        // ── Header description ──
        media_engine::Layout::SetCursorPos(lx, ly);
        media_engine::Text::Colored(media_engine::Colors::Gray55,
            L.Get("computer_organize_desc").c_str());
        ly += lh;

        // ── Auto-send results to agent when scan completes ──
        if (s_state.has_results && !s_state.scanning.load() && !s_state.report_sent) {
            s_state.report_sent = true;
            std::string sid = SpriteManager::GetInstance().GetFocusedSession();
            if (!sid.empty()) {
                std::string report = BuildScanReport();
                AgentEngine::GetInstance().SendUserMessage(sid, report);
            }
        }

        // ── Scan / Cancel buttons ──
        if (!s_state.scanning.load()) {
            media_engine::Layout::SetCursorPos(lx, ly);
            bool scan_clicked = media_engine::ImGuiWidget::Button(
                L.Get("cache_scan_btn").c_str(), 140.0f * sm, 30.0f * sm);

            if (scan_clicked) {
                s_state.scanning.store(false);
                s_state.cancel.store(false);
                s_state.progress.store(0);
                s_state.total_steps.store(0);
                s_state.current_item.clear();
                s_state.categories.clear();
                s_state.grand_total_size = 0;
                s_state.has_results = false;
                s_state.report_sent = false;
                s_show_deleted = false;
                std::thread(DoScan).detach();
            }

            if (s_state.has_results) {
                // Delete all button
                media_engine::Layout::SetCursorPos(lx + 150.0f * sm, ly);
                std::string clean_label = L.Get("cache_delete_all") + " (" +
                    FmtSize(s_state.grand_total_size) + ")";
                if (media_engine::ImGuiWidget::Button(
                        clean_label.c_str(), 200.0f * sm, 30.0f * sm)) {
                    for (auto& cat : s_state.categories) {
                        DeleteCategory(cat);
                    }
                    s_state.grand_total_size = 0;
                    for (auto& cat : s_state.categories) {
                        s_state.grand_total_size += cat.total_size;
                    }
                    s_show_deleted = true;
                }

                media_engine::Layout::SetCursorPos(lx + 360.0f * sm, ly + 6.0f * sm);
                media_engine::Text::Colored(media_engine::Colors::Gray40,
                    (L.Get("cache_cleanable") + " " + FmtSize(s_state.grand_total_size)).c_str());
            }
            ly += 38.0f * sm;

        } else {
            // ── Scanning progress ──
            float scroll_y = media_engine::Scroll::GetY();
            float sx = split.main.x;
            float sy = split.main.y;
            media_engine::DrawList::RoundRect(sx + lx, sy + ly - scroll_y,
                w - pad * 2, 36.0f * sm, 4.0f, media_engine::Colors::OrangeLightest);

            int pct = s_state.total_steps.load() > 0
                ? (s_state.progress.load() * 100 / s_state.total_steps.load())
                : 0;

            media_engine::Layout::SetCursorPos(lx + 6.0f * sm, ly + 2.0f * sm);
            media_engine::Text::Colored(media_engine::Colors::Orange,
                (L.Get("cache_scanning") + " " + std::to_string(pct) + "%").c_str());
            media_engine::Layout::SetCursorPos(lx + 6.0f * sm, ly + 20.0f * sm);
            media_engine::Text::Colored(media_engine::Colors::Gray55,
                s_state.current_item.c_str());

            media_engine::Layout::SetCursorPos(lx, ly + 42.0f * sm);
            if (media_engine::ImGuiWidget::Button(
                    L.Get("cache_cancel").c_str(), 100.0f * sm, 26.0f * sm))
                s_state.cancel.store(true);
            ly += 76.0f * sm;
        }

        // ── Results display ──
        if (s_state.has_results && !s_state.scanning.load()) {
            float scroll_y = media_engine::Scroll::GetY();
            float sx = split.main.x;
            float sy = split.main.y;

            if (s_show_deleted) {
                media_engine::DrawList::RoundRect(sx + lx, sy + ly - scroll_y,
                    w - pad * 2, 30.0f * sm, 4.0f, media_engine::Colors::GreenPale);
                media_engine::Layout::SetCursorPos(lx + 6.0f * sm, ly + 6.0f * sm);
                media_engine::Text::Colored(media_engine::Colors::GreenMid,
                    L.Get("cache_clean_done").c_str());
                ly += 36.0f * sm;

                // Show what was deleted
                for (auto& cat : s_state.categories) {
                    bool has_deleted = false;
                    for (auto& e : cat.entries) {
                        if (!e.exists && e.size == 0) { has_deleted = true; break; }
                    }
                    if (!has_deleted) continue;

                    media_engine::Layout::SetCursorPos(lx, ly);
                    media_engine::Text::Colored(media_engine::Colors::OrangeDeep,
                        cat.label.c_str());
                    ly += 20.0f * sm;

                    int n = 0;
                    for (auto& e : cat.entries) {
                        if (e.exists) continue;
                        if (++n > 10) {
                            media_engine::Layout::SetCursorPos(lx + 4.0f * sm, ly);
                            media_engine::Text::Colored(media_engine::Colors::Gray55, "...");
                            ly += 16.0f * sm;
                            break;
                        }
                        media_engine::Layout::SetCursorPos(lx + 4.0f * sm, ly);
                        media_engine::Text::Colored(media_engine::Colors::Gray55,
                            ("\xe2\x9c\x93 " + e.name).c_str());
                        ly += 16.0f * sm;
                    }
                    ly += 4.0f * sm;
                }
            }

            // ── Categorized results ──
            int cat_idx = 0;
            for (auto& cat : s_state.categories) {
                // Category header background
                media_engine::DrawList::RoundRect(sx + lx, sy + ly - scroll_y,
                    w - pad * 2, 30.0f * sm, 4.0f,
                    (cat_idx % 2 == 0) ? media_engine::Colors::MilkyWhite
                                       : media_engine::Colors::White);
                media_engine::Layout::SetCursorPos(lx + 6.0f * sm, ly + 4.0f * sm);
                media_engine::Text::Colored(media_engine::Colors::Orange,
                    cat.label.c_str());

                std::string size_str = FmtSize(cat.total_size);
                media_engine::Layout::SetCursorPos(lx + 6.0f * sm, ly + 18.0f * sm);
                media_engine::Text::Colored(media_engine::Colors::Gray40,
                    (size_str + " \xe2\x80\x94 " + std::to_string(cat.entries.size()) + " items").c_str());

                // Category delete button
                media_engine::Layout::SetCursorPos(lx + w - 100.0f * sm - pad, ly + 2.0f * sm);
                if (cat.total_size > 0 && media_engine::ImGuiWidget::Button(
                        (L.Get("cache_delete")).c_str(), 90.0f * sm, 24.0f * sm)) {
                    DeleteCategory(cat);
                    s_state.grand_total_size = 0;
                    for (auto& c : s_state.categories) {
                        s_state.grand_total_size += c.total_size;
                    }
                }
                ly += 36.0f * sm;

                // List each cache entry
                int n = 0;
                for (auto& entry : cat.entries) {
                    if (++n > 15) {
                        media_engine::Layout::SetCursorPos(lx + 4.0f * sm, ly);
                        media_engine::Text::Colored(media_engine::Colors::Gray55, "...");
                        ly += 16.0f * sm;
                        break;
                    }

                    if (entry.exists) {
                        media_engine::Layout::SetCursorPos(lx + 8.0f * sm, ly);
                        media_engine::Text::Colored(media_engine::Colors::Gray40,
                            entry.name.c_str());
                        media_engine::Layout::SetCursorPos(lx + w * 0.5f, ly);
                        media_engine::Text::Colored(media_engine::Colors::Gray55,
                            FmtSize(entry.size).c_str());

                        // Item-level delete button
                        media_engine::Layout::SetCursorPos(lx + w - 50.0f * sm - pad, ly);
                        if (media_engine::ImGuiWidget::Button(
                                ("\xc3\x97##" + entry.name).c_str(),
                                24.0f * sm, 16.0f * sm)) {
                            DeleteCacheEntry(entry);
                            cat.total_size = 0;
                            for (auto& e : cat.entries) {
                                if (e.exists) cat.total_size += e.size;
                            }
                            s_state.grand_total_size = 0;
                            for (auto& c : s_state.categories) {
                                s_state.grand_total_size += c.total_size;
                            }
                        }
                    } else {
                        media_engine::Layout::SetCursorPos(lx + 8.0f * sm, ly);
                        media_engine::Text::Colored(media_engine::Colors::Gray55,
                            ("\xe2\x9c\x97 " + entry.name).c_str());
                    }
                    ly += 16.0f * sm;
                }
                ly += 8.0f * sm;
                ++cat_idx;
            }

            // ── Grand total footer ──
            if (s_state.grand_total_size > 0) {
                media_engine::DrawList::RoundRect(sx + lx, sy + ly - scroll_y,
                    w - pad * 2, 30.0f * sm, 4.0f, media_engine::Colors::OrangeLightest);
                media_engine::Layout::SetCursorPos(lx + 6.0f * sm, ly + 6.0f * sm);
                media_engine::Text::Colored(media_engine::Colors::OrangeDeep,
                    (L.Get("cache_total") + " " + FmtSize(s_state.grand_total_size)).c_str());
                ly += 36.0f * sm;
            }
        }

        // ── Tip for agent integration ──
        if (!s_state.scanning.load()) {
            ly += 10.0f * sm;
            float scroll_y = media_engine::Scroll::GetY();
            float sx = split.main.x;
            float sy = split.main.y;
            media_engine::DrawList::RoundRect(sx + lx, sy + ly - scroll_y,
                w - pad * 2, 40.0f * sm, 4.0f, media_engine::Colors::CyanLight);
            media_engine::Layout::SetCursorPos(lx + 6.0f * sm, ly + 4.0f * sm);
            media_engine::Text::Colored(media_engine::Colors::Gray40,
                L.Get("cache_agent_tip").c_str());
            ly += 20.0f * sm;
            media_engine::Layout::SetCursorPos(lx + 6.0f * sm, ly + 2.0f * sm);
            media_engine::Text::Colored(media_engine::Colors::Gray55,
                L.Get("cache_agent_tip2").c_str());
        }
    }

    // ── Right: Chat Panel (agent integration) ──
    {
        auto& side = split.side;
        d_->chat_panel->SetPixelRect(side.x, side.y, side.w,
                                     side.h - 50.0f * sm);
        d_->input_panel->SetPixelRect(side.x,
            side.y + side.h - 50.0f * sm + 4.0f,
            side.w - 6.0f, 40.0f * sm);
        RenderChatContent();
    }
}

} // namespace prosophor
