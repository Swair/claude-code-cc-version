// Copyright 2026 Prosophor Contributors
// SPDX-License-Identifier: Apache-2.0
//
// POSIX implementation of pipe and process operations

#include "platform/platform.h"

#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

#include "common/log_wrapper.h"

namespace prosophor {

PipePair Platform::CreatePipe() {
    int pipefd[2];
    if (::pipe(pipefd) == -1) {
        LOG_ERROR("Failed to create pipe: {}", strerror(errno));
        return {};
    }
    return { pipefd[0], pipefd[1] };
}

bool Platform::ClosePipe(int fd) {
    if (fd >= 0 && ::close(fd) == -1) {
        LOG_ERROR("Failed to close pipe: {}", strerror(errno));
        return false;
    }
    return true;
}

int Platform::ReadPipe(int fd, char* buf, size_t size) {
    return ::read(fd, buf, size);
}

int Platform::WritePipe(int fd, const char* buf, size_t size) {
    return ::write(fd, buf, size);
}

bool Platform::Dup2Pipe(int old_fd, int new_fd) {
    return ::dup2(old_fd, new_fd) == new_fd;
}

ForkedProcess Platform::ForkAndExec(const std::string& command,
                          const std::vector<std::string>& args,
                          const std::string& workdir,
                          const std::vector<std::string>& env) {
    ForkedProcess result;

    PipePair stdin_pipe = Platform::CreatePipe();
    PipePair stdout_pipe = Platform::CreatePipe();

    if (stdin_pipe.read_fd < 0 || stdout_pipe.write_fd < 0) {
        LOG_ERROR("Failed to create pipes for ForkAndExec");
        return result;
    }

    pid_t pid = fork();
    if (pid == -1) {
        LOG_ERROR("Failed to fork: {}", strerror(errno));
        Platform::ClosePipe(stdin_pipe.read_fd);
        Platform::ClosePipe(stdin_pipe.write_fd);
        Platform::ClosePipe(stdout_pipe.read_fd);
        Platform::ClosePipe(stdout_pipe.write_fd);
        return result;
    }

    if (pid == 0) {
        close(stdin_pipe.write_fd);
        close(stdout_pipe.read_fd);

        dup2(stdin_pipe.read_fd, STDIN_FILENO);
        dup2(stdout_pipe.write_fd, STDOUT_FILENO);
        dup2(stdout_pipe.write_fd, STDERR_FILENO);

        close(stdin_pipe.read_fd);
        close(stdout_pipe.write_fd);

        if (!workdir.empty()) {
            if (chdir(workdir.c_str()) != 0) {
                _exit(127);
            }
        }

        if (!env.empty()) {
            for (const auto& e : env) {
                putenv(const_cast<char*>(e.c_str()));
            }
        }

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(command.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execvp(command.c_str(), argv.data());
        _exit(1);
    }

    close(stdin_pipe.read_fd);
    close(stdout_pipe.write_fd);

    result.pid = pid;
    result.stdin_fd = stdin_pipe.write_fd;
    result.stdout_fd = stdout_pipe.read_fd;

    LOG_DEBUG("Forked process {} with stdin={}, stdout={}",
              pid, result.stdin_fd, result.stdout_fd);
    return result;
}

bool Platform::WaitProcess(int pid) {
    int status;
    if (waitpid(pid, &status, 0) == -1) {
        LOG_ERROR("Failed to wait for process {}: {}", pid, strerror(errno));
        return false;
    }
    return true;
}
}  // namespace prosophor