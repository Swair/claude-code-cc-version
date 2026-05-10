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
namespace platform {

#ifdef _WIN32
constexpr bool kIsWindows = true;
#else
constexpr bool kIsWindows = false;
#endif

#ifdef __linux__
constexpr bool kIsLinux = true;
#else
constexpr bool kIsLinux = false;
#endif

#ifdef __APPLE__
constexpr bool kIsMacOS = true;
#else
constexpr bool kIsMacOS = false;
#endif

/// Convert system native encoding to UTF-8 (no-op on Linux/macOS)
std::string NativeToUtf8(const std::string& input);

/// Read a line from stdin (handles Windows console encoding)
std::string ReadLine();

/// Get home directory (cross-platform: HOME / USERPROFILE)
std::string HomeDir();

/// Convert time_t to local tm (thread-safe, cross-platform)
std::tm LocalTime(std::time_t t);

/// Read a line from console (bypasses stdin encoding issues on Windows)
std::string ReadConsoleLine();

/// Convert UTF-8 to wide string (UTF-16 on Windows, required for CreateProcessW etc.)
std::wstring Utf8ToWide(const std::string& utf8_str);

/// Escape argument for shell (cross-platform)
std::string ShellEscape(const std::string& arg);

/// Set console to UTF-8 mode (no-op on POSIX, sets CP on Windows)
void SetConsoleUtf8();

/// Get current process ID
int GetPid();

/// Get path to current executable
std::string GetSelfExePath();

/// Normalize path for the current platform.
/// On Windows with MinGW, converts POSIX-style /x/... paths to X:\... format.
std::string NormalizePath(const std::string& path);

/// Check if a filesystem path exists, with platform-specific path normalization.
bool PathExists(const std::string& path);

/// On Windows, returns win_path if non-empty, otherwise default_path.
/// On other platforms, always returns default_path.
/// Used to select a platform-specific config value at compile time without #ifdef.
std::string SelectPlatformPath(const std::string& default_path, const std::string& win_path);

/// Check if a TCP port is open on localhost
bool CheckPortOpen(int port);

/// Run a shell command and return stdout (empty on error)
std::string RunShellCommand(const char* cmd);

/// Return platform null device path ("/dev/null" on POSIX, "NUL" on Windows)
const char* NullDevice();

/// Default Chinese font path (platform-specific)
#ifdef _WIN32
constexpr const char* kDefaultFontPath = "C:/Windows/Fonts/msyh.ttc";
#else
constexpr const char* kDefaultFontPath = "/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf";
#endif

// Pipe operations - abstract POSIX APIs for LSP/MCP server communication
struct PipePair {
    int read_fd = -1;
    int write_fd = -1;
};
PipePair CreatePipe();
bool ClosePipe(int fd);
int ReadPipe(int fd, char* buf, size_t size);
int WritePipe(int fd, const char* buf, size_t size);
bool Dup2Pipe(int old_fd, int new_fd);

/// Set a pipe fd to non-blocking mode
bool SetPipeNonBlocking(int fd);

/// Check if last pipe operation failed with EAGAIN/EWOULDBLOCK
bool IsPipeWouldBlock();

/// Get error message for last failed pipe operation
std::string GetPipeErrorString();

// Process creation for spawning servers (used by LSP/MCP)
struct ForkedProcess {
    int pid = -1;
    int stdin_fd = -1;
    int stdout_fd = -1;
};
ForkedProcess ForkAndExec(const std::string& command,
                          const std::vector<std::string>& args,
                          const std::string& workdir = "",
                          const std::vector<std::string>& env = {});
bool WaitProcess(int pid);
int GetPid();

/// Opaque handle to a launched subprocess
struct Subprocess {
    int pid = -1;
};

/// Launch a subprocess with args, detach stdin/stdout/stderr
Subprocess LaunchProcess(const std::vector<std::string>& args);

/// Launch a detached background process from a shell command string.
/// Linux: system(cmd &) and captures PID via $!.
/// Windows: CreateProcess with DETACHED_PROCESS.
/// Returns PID or -1 on failure.
int LaunchDetachedCommand(const std::string& command);

/// Check if a process with given PID is still alive
bool IsProcessAlive(int pid);

/// Kill a process, optionally forcing immediate termination
bool KillProcess(int pid, bool force = false);

/// Result of running a command with output capture
struct CommandOutput {
    std::string output;
    int exit_code = -1;
};

/// Run a shell command and capture stdout+stderr with optional timeout and workdir
CommandOutput RunCommandWithOutput(const std::string& command,
                                    int timeout_seconds = 0,
                                    const std::string& workdir = "");

	/// Result of executing a trigger script
	struct ScriptResult {
	    int return_code = 0;
	    std::string output;
	    std::string error_output;
	    bool timeout = false;
	};

	/// Execute a script file with timeout protection.
	/// Sets execute permission on POSIX, wraps .py/.sh with interpreter on Windows.
	ScriptResult ExecuteScriptWithTimeout(const std::string& script_path, int timeout_ms = 100);

}  // namespace platform
}  // namespace prosophor
