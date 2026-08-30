// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
//
// Windows implementation of pipe and process operations

#include "platform/platform.h"

#include <io.h>
#include <fcntl.h>
#include <windows.h>
#include <process.h>
#include <cstring>

#include "common/log_wrapper.h"

namespace prosophor {

PipePair Platform::CreatePipe() {
    PipePair result;
    HANDLE read_handle, write_handle;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    if (!::CreatePipe(&read_handle, &write_handle, &sa, 0)) {
        LOG_ERROR("Failed to create pipe: {}", GetLastError());
        return result;
    }

    // Convert HANDLEs to file descriptors using _open_osfhandle
    result.read_fd = _open_osfhandle(reinterpret_cast<intptr_t>(read_handle), _O_RDONLY);
    result.write_fd = _open_osfhandle(reinterpret_cast<intptr_t>(write_handle), _O_WRONLY);

    if (result.read_fd == -1 || result.write_fd == -1) {
        LOG_ERROR("Failed to convert handles to file descriptors");
        if (result.read_fd != -1) CloseHandle(read_handle);
        if (result.write_fd != -1) CloseHandle(write_handle);
        return {};
    }

    return result;
}

bool Platform::ClosePipe(int fd) {
    if (fd >= 0) {
        return _close(fd) == 0;
    }
    return true;
}

int Platform::ReadPipe(int fd, char* buf, size_t size) {
    return _read(fd, buf, static_cast<unsigned int>(size));
}

int Platform::WritePipe(int fd, const char* buf, size_t size) {
    return _write(fd, buf, static_cast<unsigned int>(size));
}

bool Platform::Dup2Pipe(int /*old_fd*/, int /*new_fd*/) {
    return false;
}

ForkedProcess Platform::ForkAndExec(const std::string& command,
                          const std::vector<std::string>& args,
                          const std::string& workdir,
                          const std::vector<std::string>& /*env*/) {
    ForkedProcess result;

    // Create pipes for stdin and stdout
    HANDLE stdin_read, stdin_write;
    HANDLE stdout_read, stdout_write;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    if (!::CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
        LOG_ERROR("Failed to create stdin pipe: {}", GetLastError());
        return result;
    }
    if (!::CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        LOG_ERROR("Failed to create stdout pipe: {}", GetLastError());
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        return result;
    }

    // Build command line
    std::string cmd_line;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) cmd_line += " ";
        if (args[i].find(' ') != std::string::npos) {
            cmd_line += "\"" + args[i] + "\"";
        } else {
            cmd_line += args[i];
        }
    }

    STARTUPINFOA si = {};
    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = stdout_write;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(
            command.c_str(),
            cmd_line.empty() ? nullptr : const_cast<char*>(cmd_line.c_str()),
            nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP,
            nullptr,
            workdir.empty() ? nullptr : workdir.c_str(),
            &si, &pi)) {
        LOG_ERROR("Failed to create process: {}", GetLastError());
        CloseHandle(stdin_read);
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        return result;
    }

    // Close the inherited handles in parent
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    CloseHandle(pi.hThread);

    // Convert HANDLEs to file descriptors
    result.stdin_fd = _open_osfhandle(reinterpret_cast<intptr_t>(stdin_write), _O_WRONLY);
    result.stdout_fd = _open_osfhandle(reinterpret_cast<intptr_t>(stdout_read), _O_RDONLY);
    result.pid = static_cast<int>(pi.dwProcessId);

    if (result.stdin_fd == -1 || result.stdout_fd == -1) {
        LOG_ERROR("Failed to convert process handles to file descriptors");
        CloseHandle(pi.hProcess);
        return {};
    }

    LOG_DEBUG("Started process {} with stdin={}, stdout={}",
              result.pid, result.stdin_fd, result.stdout_fd);
    return result;
}

bool Platform::WaitProcess(int pid) {
    HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!hProcess) {
        LOG_ERROR("Failed to open process {}: {}", pid, GetLastError());
        return false;
    }

    DWORD ret = WaitForSingleObject(hProcess, INFINITE);
    CloseHandle(hProcess);

    return ret == WAIT_OBJECT_0;
}

PipedProcess Platform::CreatePipedSubprocess(const std::string& command) {
    PipedProcess result;

    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    HANDLE stdin_read = nullptr, stdin_write = nullptr;
    HANDLE stdout_read = nullptr, stdout_write = nullptr;

    if (!::CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
        LOG_ERROR("Failed to create stdin pipe: {}", GetLastError());
        return result;
    }
    if (!::CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        LOG_ERROR("Failed to create stdout pipe: {}", GetLastError());
        CloseHandle(stdin_read); CloseHandle(stdin_write);
        return result;
    }

    // Parent ends must NOT be inherited by the child
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);  // stderr → parent's stderr

    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, const_cast<char*>(command.c_str()),
                        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) {
        LOG_ERROR("Failed to create process: {}", GetLastError());
        CloseHandle(stdin_read); CloseHandle(stdin_write);
        CloseHandle(stdout_read); CloseHandle(stdout_write);
        return result;
    }

    // Close child's ends in parent
    CloseHandle(stdin_read);
    CloseHandle(stdout_write);
    CloseHandle(pi.hThread);

    result.stdin_fd = _open_osfhandle(reinterpret_cast<intptr_t>(stdin_write), _O_WRONLY);
    result.stdout_fd = _open_osfhandle(reinterpret_cast<intptr_t>(stdout_read), _O_RDONLY);
    result.pid = static_cast<int>(pi.dwProcessId);
    CloseHandle(pi.hProcess);

    LOG_DEBUG("Created piped process {} (stdin_fd={}, stdout_fd={})",
              result.pid, result.stdin_fd, result.stdout_fd);
    return result;
}

int Platform::WaitProcessWithExitCode(int pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                                  FALSE, static_cast<DWORD>(pid));
    if (!hProcess) {
        LOG_ERROR("Failed to open process {}: {}", pid, GetLastError());
        return -1;
    }

    WaitForSingleObject(hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(hProcess, &exit_code);
    CloseHandle(hProcess);
    return static_cast<int>(exit_code);
}
}  // namespace prosophor