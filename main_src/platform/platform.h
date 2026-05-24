// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <ctime>
#include <string>
#include <vector>

// Make Windows headers available everywhere via platform.h
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace prosophor {

// -- Data structures (used by Platform methods) -------------------------

/// Pipe pair for subprocess stdin/stdout communication.
struct PipePair {
    int read_fd = -1;
    int write_fd = -1;
};

/// Process handles for spawned LSP/MCP servers.
struct ForkedProcess {
    int pid = -1;
    int stdin_fd = -1;
    int stdout_fd = -1;
};

/// Opaque handle to a launched subprocess.
struct Subprocess {
    int pid = -1;
};

/// Result of running a command with output capture.
struct CommandOutput {
    std::string output;
    int exit_code = -1;
};

/// Result of executing a trigger script.
struct ScriptResult {
    int return_code = 0;
    std::string output;
    std::string error_output;
    bool timeout = false;
};

// -- Platform abstraction (all static) ---------------------------------

class Platform {
public:
    Platform() = delete;

#ifdef _WIN32
    static constexpr bool kIsWindows = true;
#else
    static constexpr bool kIsWindows = false;
#endif

#ifdef __linux__
    static constexpr bool kIsLinux = true;
#else
    static constexpr bool kIsLinux = false;
#endif

#ifdef __APPLE__
    static constexpr bool kIsMacOS = true;
#else
    static constexpr bool kIsMacOS = false;
#endif

    /// Default Chinese font path (platform-specific)
#ifdef _WIN32
    static constexpr const char* kDefaultFontPath = "C:/Windows/Fonts/msyh.ttc";
#else
    static constexpr const char* kDefaultFontPath = "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf";
#endif

    /// Convert system native encoding to UTF-8 (no-op on Linux/macOS)
    static std::string NativeToUtf8(const std::string& input);

    /// Read a line from stdin (handles Windows console encoding)
    static std::string ReadLine();

    /// Get home directory (cross-platform: HOME / USERPROFILE)
    static std::string HomeDir();

    /// Read a line from console (bypasses stdin encoding issues on Windows)
    static std::string ReadConsoleLine();

    /// Convert UTF-8 to wide string (UTF-16 on Windows, required for CreateProcessW etc.)
    static std::wstring Utf8ToWide(const std::string& utf8_str);

    /// Escape argument for shell (cross-platform)
    static std::string ShellEscape(const std::string& arg);

    /// Set console to UTF-8 mode (no-op on POSIX, sets CP on Windows)
    static void SetConsoleUtf8();

    /// Get current process ID
    static int GetPid();

    /// Get path to current executable
    static std::string GetSelfExePath();

    /// Normalize path for the current platform.
    /// On Windows with MinGW, converts POSIX-style /x/... paths to X:\... format.
    static std::string NormalizePath(const std::string& path);

    /// Check if a filesystem path exists, with platform-specific path normalization.
    static bool PathExists(const std::string& path);

    /// On Windows, returns win_path if non-empty, otherwise default_path.
    /// On other platforms, always returns default_path.
    static std::string SelectPlatformPath(const std::string& default_path, const std::string& win_path);

    /// Check if a TCP port is open on localhost
    static bool CheckPortOpen(int port);

    /// Run a shell command and return stdout (empty on error)
    static std::string RunShellCommand(const char* cmd);

    /// Return platform null device path ("/dev/null" on POSIX, "NUL" on Windows)
    static const char* NullDevice();

    // -- Pipe operations (abstract POSIX APIs for LSP/MCP) ------------
    static PipePair CreatePipe();

    static bool ClosePipe(int fd);

    static int ReadPipe(int fd, char* buf, size_t size);

    static int WritePipe(int fd, const char* buf, size_t size);

    static bool Dup2Pipe(int old_fd, int new_fd);

    /// Set a pipe fd to non-blocking mode
    static bool SetPipeNonBlocking(int fd);

    /// Check if last pipe operation failed with EAGAIN/EWOULDBLOCK
    static bool IsPipeWouldBlock();

    /// Get error message for last failed pipe operation
    static std::string GetPipeErrorString();

    // -- Process operations -------------------------------------------
    static ForkedProcess ForkAndExec(const std::string& command,
                                     const std::vector<std::string>& args,
                                     const std::string& workdir = "",
                                     const std::vector<std::string>& env = {});

    static bool WaitProcess(int pid);

    /// Launch a subprocess with args, detach stdin/stdout/stderr
    static Subprocess LaunchProcess(const std::vector<std::string>& args);

    /// Launch a detached background process from a shell command string.
    static int LaunchDetachedCommand(const std::string& command);

    /// Check if a process with given PID is still alive
    static bool IsProcessAlive(int pid);

    /// Kill a process, optionally forcing immediate termination
    static bool KillProcess(int pid, bool force = false);

    /// Kill all processes with the given executable name (e.g. "llama-server")
    static bool KillProcessByName(const std::string& name);

    /// Run a shell command and capture stdout+stderr with optional timeout and workdir
    static CommandOutput RunCommandWithOutput(const std::string& command,
                                              int timeout_seconds = 0,
                                              const std::string& workdir = "");

    /// Execute a script file with timeout protection.
    static ScriptResult ExecuteScriptWithTimeout(const std::string& script_path, int timeout_ms = 100);

    /// Open native file open dialog. Returns selected file path, or empty on cancel.
    /// On non-Windows platforms, always returns empty.
    static std::string BrowseForFile(const char* filter = nullptr);

    /// Open native folder selection dialog. Returns selected directory path, or empty on cancel.
    /// On non-Windows platforms, always returns empty.
    static std::string BrowseForDirectory();
};

}  // namespace prosophor
