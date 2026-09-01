/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "process_launcher.h"
#include "launcher.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace npu_compute::compute_launcher {
namespace {

constexpr int kProgramNotExecutableExitCode = 126;
constexpr int kProgramNotFoundExitCode = 127;
constexpr std::array<int, 3> kForwardedSignals = {SIGINT, SIGTERM, SIGHUP};

volatile sig_atomic_t g_app_process_group = 0;

enum class ChildErrorStage : uint32_t {
    SetProcessGroup = 1,
    Exec = 2,
};

struct ChildError {
    ChildErrorStage stage;
    int error_number;
};

struct SignalState {
    sigset_t forwarded_set{};
    sigset_t original_mask{};
    std::array<struct sigaction, kForwardedSignals.size()> original_actions{};
    std::size_t installed_actions = 0;
    bool signals_blocked = false;
};

void ForwardSignal(int signal_number)
{
    const sig_atomic_t process_group = g_app_process_group;
    if (process_group > 0) {
        kill(-static_cast<pid_t>(process_group), signal_number);
    }
}

void SetError(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
}

std::string ErrnoMessage(const char* operation, int error_number)
{
    return std::string(operation) + " failed: " + std::strerror(error_number);
}

void RestoreSignalState(SignalState* state)
{
    if (state == nullptr) {
        return;
    }

    sigprocmask(SIG_BLOCK, &state->forwarded_set, nullptr);
    g_app_process_group = 0;
    for (std::size_t i = 0; i < state->installed_actions; ++i) {
        sigaction(kForwardedSignals[i], &state->original_actions[i], nullptr);
    }
    sigprocmask(SIG_SETMASK, &state->original_mask, nullptr);
    state->signals_blocked = false;
    state->installed_actions = 0;
}

bool PrepareSignalState(SignalState* state, std::string* error)
{
    sigemptyset(&state->forwarded_set);
    for (int signal_number : kForwardedSignals) {
        sigaddset(&state->forwarded_set, signal_number);
    }
    if (sigprocmask(SIG_BLOCK, &state->forwarded_set, &state->original_mask) != 0) {
        SetError(ErrnoMessage("sigprocmask", errno), error);
        return false;
    }
    state->signals_blocked = true;

    struct sigaction action {};
    action.sa_handler = ForwardSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    for (std::size_t i = 0; i < kForwardedSignals.size(); ++i) {
        if (sigaction(kForwardedSignals[i], &action, &state->original_actions[i]) != 0) {
            const int saved_errno = errno;
            state->installed_actions = i;
            RestoreSignalState(state);
            SetError(ErrnoMessage("sigaction", saved_errno), error);
            return false;
        }
        state->installed_actions = i + 1;
    }
    return true;
}

void RestoreChildSignals(const SignalState& state)
{
    g_app_process_group = 0;
    for (std::size_t i = 0; i < state.installed_actions; ++i) {
        sigaction(kForwardedSignals[i], &state.original_actions[i], nullptr);
    }
    sigprocmask(SIG_SETMASK, &state.original_mask, nullptr);
}

int UnblockParentSignals(SignalState* state)
{
    if (sigprocmask(SIG_SETMASK, &state->original_mask, nullptr) != 0) {
        return errno;
    }
    state->signals_blocked = false;
    return 0;
}

void WriteChildError(int file_descriptor, ChildErrorStage stage, int error_number)
{
    const ChildError child_error{stage, error_number};
    const auto* data = reinterpret_cast<const uint8_t*>(&child_error);
    std::size_t written = 0;
    while (written < sizeof(child_error)) {
        const ssize_t result = write(file_descriptor, data + written, sizeof(child_error) - written);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
            continue;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }
        break;
    }
}

bool ReadChildError(int file_descriptor, ChildError* child_error, bool* has_error, std::string* error)
{
    auto* data = reinterpret_cast<uint8_t*>(child_error);
    std::size_t received = 0;
    while (received < sizeof(*child_error)) {
        const ssize_t result = read(file_descriptor, data + received, sizeof(*child_error) - received);
        if (result > 0) {
            received += static_cast<std::size_t>(result);
            continue;
        }
        if (result == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        SetError(ErrnoMessage("read exec error pipe", errno), error);
        return false;
    }

    if (received != 0 && received != sizeof(*child_error)) {
        SetError("read exec error pipe failed: incomplete child error", error);
        return false;
    }
    *has_error = received == sizeof(*child_error);
    return true;
}

bool WaitForChild(pid_t child_pid, int* status, std::string* error)
{
    while (true) {
        const pid_t result = waitpid(child_pid, status, 0);
        if (result == child_pid) {
            return true;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }
        SetError(ErrnoMessage("waitpid", errno), error);
        return false;
    }
}

int ExitCodeFromStatus(int status, std::string* error)
{
    if (WIFEXITED(status)) {
        const int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            SetError("APP exited with status " + std::to_string(exit_code), error);
        }
        return exit_code;
    }
    if (WIFSIGNALED(status)) {
        const int signal_number = WTERMSIG(status);
        SetError("APP terminated by signal " + std::to_string(signal_number), error);
        return 128 + signal_number;
    }
    SetError("waitpid returned an unsupported APP status", error);
    return kInternalErrorExitCode;
}

} // namespace

int LaunchProcessAndWait(const ProcessLaunchRequest& request, std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (request.program.empty()) {
        SetError("program is empty", error);
        return kInternalErrorExitCode;
    }

    std::vector<std::string> argument_storage;
    argument_storage.reserve(request.arguments.size() + 1);
    argument_storage.push_back(request.program);
    argument_storage.insert(argument_storage.end(), request.arguments.begin(), request.arguments.end());
    std::vector<char*> argument_pointers;
    argument_pointers.reserve(argument_storage.size() + 1);
    for (std::string& argument : argument_storage) {
        argument_pointers.push_back(argument.data());
    }
    argument_pointers.push_back(nullptr);

    std::vector<std::string> environment_storage = request.environment;
    std::vector<char*> environment_pointers;
    environment_pointers.reserve(environment_storage.size() + 1);
    for (std::string& entry : environment_storage) {
        environment_pointers.push_back(entry.data());
    }
    environment_pointers.push_back(nullptr);

    int error_pipe[2];
    if (pipe2(error_pipe, O_CLOEXEC) != 0) {
        SetError(ErrnoMessage("pipe2", errno), error);
        return kInternalErrorExitCode;
    }

    SignalState signal_state;
    if (!PrepareSignalState(&signal_state, error)) {
        close(error_pipe[0]);
        close(error_pipe[1]);
        return kInternalErrorExitCode;
    }

    const pid_t child_pid = fork();
    if (child_pid < 0) {
        const int saved_errno = errno;
        close(error_pipe[0]);
        close(error_pipe[1]);
        RestoreSignalState(&signal_state);
        SetError(ErrnoMessage("fork", saved_errno), error);
        return kInternalErrorExitCode;
    }

    if (child_pid == 0) {
        close(error_pipe[0]);
        RestoreChildSignals(signal_state);
        if (setpgid(0, 0) != 0) {
            const int saved_errno = errno;
            WriteChildError(error_pipe[1], ChildErrorStage::SetProcessGroup, saved_errno);
            _exit(kInternalErrorExitCode);
        }
        execvpe(request.program.c_str(), argument_pointers.data(), environment_pointers.data());
        const int saved_errno = errno;
        WriteChildError(error_pipe[1], ChildErrorStage::Exec, saved_errno);
        _exit(saved_errno == ENOENT ? kProgramNotFoundExitCode : kProgramNotExecutableExitCode);
    }

    close(error_pipe[1]);
    std::string management_error;
    if (setpgid(child_pid, child_pid) != 0 && errno != EACCES && errno != ESRCH) {
        management_error = ErrnoMessage("setpgid", errno);
    }
    g_app_process_group = static_cast<sig_atomic_t>(child_pid);
    const int unblock_error = UnblockParentSignals(&signal_state);
    if (unblock_error != 0 && management_error.empty()) {
        management_error = ErrnoMessage("sigprocmask", unblock_error);
    }

    ChildError child_error{};
    bool has_child_error = false;
    const bool pipe_read_success = ReadChildError(error_pipe[0], &child_error, &has_child_error, error);
    close(error_pipe[0]);

    int child_status = 0;
    const bool wait_success = WaitForChild(child_pid, &child_status, error);
    RestoreSignalState(&signal_state);

    if (!management_error.empty()) {
        SetError(management_error, error);
        return kInternalErrorExitCode;
    }
    if (!pipe_read_success || !wait_success) {
        return kInternalErrorExitCode;
    }
    if (has_child_error) {
        if (child_error.stage == ChildErrorStage::SetProcessGroup) {
            SetError(ErrnoMessage("child setpgid", child_error.error_number), error);
            return kInternalErrorExitCode;
        }
        SetError(
            "failed to start program '" + request.program + "': " + std::strerror(child_error.error_number), error);
        return child_error.error_number == ENOENT ? kProgramNotFoundExitCode : kProgramNotExecutableExitCode;
    }
    return ExitCodeFromStatus(child_status, error);
}

} // namespace npu_compute::compute_launcher
