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

namespace fs = std::filesystem;
namespace prosophor {

namespace {

struct DiskInfo {
    char drive_letter;
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    uint64_t used_bytes = 0;
};

struct ScanResult {
    std::string name;
    std::string path;
    uint64_t size = 0;
};

struct ScanState {
    std::atomic<bool> scanning{false};
    std::atomic<bool> cancel{false};
    std::atomic<uint64_t> scanned_files{0};
    std::string current_path;
    std::vector<ScanResult> large_files;
    std::vector<ScanResult> temp_files;
    std::vector<ScanResult> old_files;
    uint64_t total_size = 0;
    uint64_t temp_total = 0;
    uint64_t old_total = 0;
};

static ScanState s_state;
static char s_sel_drive[4] = "C:";
static int s_depth = 3;
static double s_min_mb = 10.0;

static std::vector<DiskInfo> s_drive_cache;
static std::once_flag s_drive_flag;

std::vector<DiskInfo> ListDrives() {
    std::call_once(s_drive_flag, []() {
        DWORD mask = GetLogicalDrives();
        for (char d = 'C'; d <= 'Z'; ++d) {
            if (!(mask & 1)) { mask >>= 1; continue; }
            mask >>= 1;
            std::string root = std::string(1, d) + ":\\";
            UINT type = GetDriveTypeA(root.c_str());
            if (type != DRIVE_FIXED && type != DRIVE_RAMDISK) continue;
            ULARGE_INTEGER total, free;
            if (!GetDiskFreeSpaceExA(root.c_str(), nullptr, &total, &free)) continue;
            s_drive_cache.push_back({d, total.QuadPart, free.QuadPart,
                total.QuadPart - free.QuadPart});
        }
        std::sort(s_drive_cache.begin(), s_drive_cache.end(),
            [](auto& a, auto& b) { return a.used_bytes > b.used_bytes; });
    });
    return s_drive_cache;
}

bool IsTemp(const fs::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext) c = std::tolower(c);
    return ext == ".tmp" || ext == ".log" || ext == ".cache" || ext == ".dmp";
}

bool IsOld(const fs::path& p) {
    auto ext = p.extension().string();
    for (auto& c : ext) c = std::tolower(c);
    return ext == ".bak" || ext == ".old" || ext == ".backup";
}

std::string FmtSize(uint64_t bytes) {
    const char* u[] = {"B","KB","MB","GB","TB"};
    int i = 0;
    double v = (double)bytes;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    std::ostringstream os;
    os << std::fixed << std::setprecision(1) << v << " " << u[i];
    return os.str();
}

void DoScan(const fs::path& dir, int depth,
            std::vector<ScanResult>& large,
            std::vector<ScanResult>& temp,
            std::vector<ScanResult>& old,
            uint64_t min_size, int max_depth) {
    if (s_state.cancel.load() || depth > max_depth) return;
    std::error_code ec;
    try {
        for (auto& e : fs::directory_iterator(dir, ec)) {
            if (s_state.cancel.load()) return;
            s_state.scanned_files.fetch_add(1);
            std::string name;
            try {
                name = e.path().filename().string();
            } catch (...) { continue; }
            s_state.current_path = e.path().string();
            if (name == "$Recycle.Bin" || name == "System Volume Information")
                continue;
            if (e.is_directory(ec)) {
                DoScan(e.path(), depth + 1, large, temp, old, min_size, max_depth);
            } else if (e.is_regular_file(ec)) {
                auto sz = e.file_size(ec);
                if (ec) continue;
                ScanResult r{name, e.path().string(), sz};
                if (sz >= min_size) large.push_back(r);
                if (IsTemp(e.path())) temp.push_back(r);
                if (IsOld(e.path())) old.push_back(r);
            }
        }
    } catch (const fs::filesystem_error&) {
    }
}

} // anonymous namespace

void ChatWindow::RenderComputerOrganizeView(int cont_x, int cont_y, int cont_w, int cont_h) {
    auto& L = I18n::Instance();
    float sm = Spacing();
    PanelFrame f(cont_x, cont_y, cont_w, cont_h, L.Get("view_computer_organize").c_str());

    // Split: left tool panel, right chat
    auto split = PanelContainer::SplitRight(f.a, 320.0f);

    // ── Left: Disk Tool ──
    {
        auto _scroll = PanelContainer::BeginScroll(split.main, 0, 4.0f, true);
        float x = split.main.x;
        float y = 0;
        float w = split.main.w;
        float lh = 22.0f * sm;

        // Drive buttons
        media_engine::DrawList::Text(x, y, media_engine::Colors::Gray55,
            L.Get("computer_organize_select_drive").c_str());
        y += lh;

        auto drives = ListDrives();
        float bx = x;
        float bw = 70.0f * sm;
        float bh = 32.0f * sm;
        for (auto& d : drives) {
            bool sel = (s_sel_drive[0] == d.drive_letter);
            auto bg = sel ? media_engine::Colors::OrangeLightest
                          : media_engine::Colors::White;
            auto brd = sel ? media_engine::Colors::Orange
                           : media_engine::Colors::CreamBorder;
            media_engine::DrawList::RoundRect(bx, y, bw, bh, 4.0f, bg);
            media_engine::DrawList::RoundRect(bx, y, bw, bh, 4.0f, brd);
            std::string lbl = std::string(1, d.drive_letter) + ":";
            media_engine::DrawList::Text(bx + 4.0f, y + 2.0f,
                media_engine::Colors::Gray40, lbl.c_str());
            media_engine::DrawList::Text(bx + 4.0f, y + bh * 0.5f + 2.0f,
                media_engine::Colors::Gray55,
                (FmtSize(d.used_bytes) + " / " + FmtSize(d.total_bytes)).c_str());
            media_engine::Layout::SetCursorScreenPos(bx, y);
            if (media_engine::ImGuiWidget::InvisibleButton(
                    ("dv" + std::string(1, d.drive_letter)).c_str(), bw, bh))
                s_sel_drive[0] = d.drive_letter;
            bx += bw + 6.0f * sm;
            if (bx + bw > x + w) { bx = x; y += bh + 6.0f * sm; }
        }
        y += bh + 10.0f * sm;

        // Scan button / progress
        if (!s_state.scanning.load()) {
            media_engine::Layout::SetCursorScreenPos(x, y);
            if (media_engine::ImGuiWidget::Button("Start Scan", 120.0f * sm, 30.0f * sm)) {
                s_state.cancel.store(false);
                s_state.large_files.clear();
                s_state.temp_files.clear();
                s_state.old_files.clear();
                s_state.scanned_files.store(0);
                s_state.total_size = 0;
                s_state.temp_total = 0;
                s_state.old_total = 0;
                uint64_t min_sz = (uint64_t)(s_min_mb * 1024.0 * 1024.0);
                int dp = s_depth;
                std::string root = std::string(s_sel_drive) + "\\";
                std::thread([root, min_sz, dp]() {
                    s_state.scanning.store(true);
                    DoScan(root, 0, s_state.large_files,
                        s_state.temp_files, s_state.old_files, min_sz, dp);
                    std::sort(s_state.large_files.begin(), s_state.large_files.end(),
                        [](auto& a, auto& b) { return a.size > b.size; });
                    if (s_state.large_files.size() > 100)
                        s_state.large_files.resize(100);
                    for (auto& r : s_state.temp_files) s_state.temp_total += r.size;
                    for (auto& r : s_state.old_files) s_state.old_total += r.size;
                    s_state.scanning.store(false);
                }).detach();
            }

            media_engine::DrawList::Text(x + 130.0f * sm, y + 4.0f * sm,
                media_engine::Colors::Gray55,
                ("Min: " + std::to_string((int)s_min_mb) + " MB").c_str());
            media_engine::Layout::SetCursorScreenPos(x + 130.0f * sm, y + 20.0f * sm);
            media_engine::ImGuiWidget::SliderFloat("##minsz", &s_min_mb, 1.0, 1000.0, "");
        } else {
            media_engine::DrawList::Text(x, y + 2.0f * sm, media_engine::Colors::Orange,
                ("Scanning... " + std::to_string(s_state.scanned_files.load()) + " files").c_str());
            media_engine::DrawList::Text(x, y + 20.0f * sm, media_engine::Colors::Gray55,
                s_state.current_path.c_str());
            media_engine::Layout::SetCursorScreenPos(x, y + 42.0f * sm);
            if (media_engine::ImGuiWidget::Button("Cancel", 100.0f * sm, 26.0f * sm))
                s_state.cancel.store(true);
        }
        y += 50.0f * sm;

        // Results
        if (!s_state.scanning.load() && (!s_state.large_files.empty() ||
            !s_state.temp_files.empty() || !s_state.old_files.empty())) {

            // Summary
            media_engine::DrawList::RoundRect(x, y, w, 36.0f * sm, 4.0f,
                media_engine::Colors::MilkyWhite);
            uint64_t total = s_state.temp_total + s_state.old_total;
            media_engine::DrawList::Text(x + 6.0f * sm, y + 4.0f * sm,
                media_engine::Colors::OrangeDeep,
                ("Scanned: " + std::to_string(s_state.scanned_files.load())).c_str());
            media_engine::DrawList::Text(x + 6.0f * sm, y + 20.0f * sm,
                media_engine::Colors::Gray40,
                ("Reclaimable: " + FmtSize(total)).c_str());
            y += 44.0f * sm;

            // Temp / cache
            if (!s_state.temp_files.empty()) {
                media_engine::DrawList::Text(x, y, media_engine::Colors::Orange,
                    ("Temporary Files  (" + FmtSize(s_state.temp_total) + ")").c_str());
                y += 20.0f * sm;
                int n = 0;
                for (auto& r : s_state.temp_files) {
                    if (++n > 15) { media_engine::DrawList::Text(x + 4.0f * sm, y,
                        media_engine::Colors::Gray55, "..."); y += 16.0f * sm; break; }
                    media_engine::DrawList::Text(x + 4.0f * sm, y,
                        media_engine::Colors::Gray40,
                        (r.name + "  " + FmtSize(r.size)).c_str());
                    y += 16.0f * sm;
                }
                y += 4.0f * sm;
            }

            // Backup / old
            if (!s_state.old_files.empty()) {
                media_engine::DrawList::Text(x, y, media_engine::Colors::Orange,
                    ("Backup / Old Files  (" + FmtSize(s_state.old_total) + ")").c_str());
                y += 20.0f * sm;
                int n = 0;
                for (auto& r : s_state.old_files) {
                    if (++n > 15) { media_engine::DrawList::Text(x + 4.0f * sm, y,
                        media_engine::Colors::Gray55, "..."); y += 16.0f * sm; break; }
                    media_engine::DrawList::Text(x + 4.0f * sm, y,
                        media_engine::Colors::Gray40,
                        (r.name + "  " + FmtSize(r.size)).c_str());
                    y += 16.0f * sm;
                }
                y += 4.0f * sm;
            }
        }
    }

    // ── Right: Chat Panel ──
    {
        auto& side = split.side;
        d_->chat_panel->SetPixelRect(side.x, side.y, side.w, side.h - 50.0f * sm);
        d_->input_panel->SetPixelRect(side.x, side.y + side.h - 50.0f * sm + 4.0f,
            side.w - 6.0f, 40.0f * sm);
        RenderChatContent();
    }
}

} // namespace prosophor
