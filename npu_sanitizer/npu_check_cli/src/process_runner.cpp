/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "process_runner.h"

#include "dbi_environment.h"
#include "uds_client.h"

#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <utility>
#include <unistd.h>

namespace npu::sanitizer::cli {
namespace {

volatile sig_atomic_t g_childProcessGroup = -1;

class UniqueFd {
public:
    explicit UniqueFd(int fd = -1) noexcept : fd_(fd) {}
    ~UniqueFd() { Reset(); }
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;
    UniqueFd(UniqueFd&& other) noexcept : fd_(other.Release()) {}

    UniqueFd& operator=(UniqueFd&& other) noexcept
    {
        if (this != &other) {
            Reset(other.Release());
        }
        return *this;
    }

    int Get() const noexcept { return fd_; }

    int Release() noexcept
    {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    void Reset(int fd = -1) noexcept
    {
        if (fd_ >= 0) {
            (void)close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_;
};

void ForwardSignal(int signalNumber)
{
    const int savedErrno = errno;
    const sig_atomic_t processGroup = g_childProcessGroup;
    if (processGroup > 0) {
        (void)kill(-static_cast<pid_t>(processGroup), signalNumber);
    }
    errno = savedErrno;
}

class SignalForwarder {
public:
    SignalForwarder(pid_t childProcessGroup, std::string& error)
    {
        struct sigaction action {};
        action.sa_handler = ForwardSignal;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        g_childProcessGroup = static_cast<sig_atomic_t>(childProcessGroup);
        for (size_t index = 0; index < signals_.size(); ++index) {
            if (sigaction(signals_[index], &action, &previous_[index]) != 0) {
                error = std::string("sigaction: ") + std::strerror(errno);
                Restore();
                return;
            }
            ++installed_;
        }
        good_ = true;
    }

    ~SignalForwarder() { Restore(); }

    SignalForwarder(const SignalForwarder&) = delete;
    SignalForwarder& operator=(const SignalForwarder&) = delete;

    bool Good() const { return good_; }

private:
    void Restore()
    {
        while (installed_ != 0) {
            --installed_;
            (void)sigaction(signals_[installed_], &previous_[installed_], nullptr);
        }
        g_childProcessGroup = -1;
        good_ = false;
    }

    const std::array<int, 3> signals_{SIGINT, SIGTERM, SIGHUP};
    std::array<struct sigaction, 3> previous_{};
    size_t installed_ = 0;
    bool good_ = false;
};

class SignalBlocker {
public:
    explicit SignalBlocker(std::string& error)
    {
        sigemptyset(&blocked_);
        sigaddset(&blocked_, SIGINT);
        sigaddset(&blocked_, SIGTERM);
        sigaddset(&blocked_, SIGHUP);
        if (sigprocmask(SIG_BLOCK, &blocked_, &previous_) != 0) {
            error = std::string("sigprocmask(SIG_BLOCK): ") + std::strerror(errno);
            return;
        }
        active_ = true;
    }

    ~SignalBlocker() { (void)RestoreSignalMask(); }

    SignalBlocker(const SignalBlocker&) = delete;
    SignalBlocker& operator=(const SignalBlocker&) = delete;

    bool Good() const { return active_; }

    bool Restore() { return RestoreSignalMask() == 0; }

    bool Restore(std::string& error)
    {
        const int result = RestoreSignalMask();
        if (result != 0) {
            error = std::string("sigprocmask(SIG_SETMASK): ") + std::strerror(result);
        }
        return result == 0;
    }

private:
    int RestoreSignalMask() noexcept
    {
        if (!active_) {
            return 0;
        }
        if (sigprocmask(SIG_SETMASK, &previous_, nullptr) != 0) {
            return errno;
        }
        active_ = false;
        return 0;
    }

    sigset_t blocked_{};
    sigset_t previous_{};
    bool active_ = false;
};

class OutputSink {
public:
    OutputSink(const std::string& consolePath, const std::string& logPath)
        : console_(consolePath, std::ios::binary | std::ios::trunc), logRequired_(!logPath.empty())
    {
        if (!logPath.empty()) {
            log_.open(logPath, std::ios::binary | std::ios::app);
        }
    }

    bool Good() const { return console_.is_open() && (!logRequired_ || log_.is_open()); }

    void Console(const char* data, size_t size)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        WriteFd(STDOUT_FILENO, data, size);
        console_.write(data, static_cast<std::streamsize>(size));
        console_.flush();
        if (log_.is_open()) {
            log_.write(data, static_cast<std::streamsize>(size));
            log_.flush();
        }
    }

    void Sanitizer(const std::string& message, bool error = false)
    {
        std::string line = "npu_check: " + message;
        if (line.back() != '\n') {
            line.push_back('\n');
        }
        std::lock_guard<std::mutex> lock(mutex_);
        WriteFd(error ? STDERR_FILENO : STDOUT_FILENO, line.data(), line.size());
        if (log_.is_open()) {
            log_.write(line.data(), static_cast<std::streamsize>(line.size()));
            log_.flush();
        }
    }

private:
    static void WriteFd(int fd, const char* data, size_t size)
    {
        size_t offset = 0;
        while (offset < size) {
            const ssize_t result = write(fd, data + offset, size - offset);
            if (result < 0 && errno == EINTR) {
                continue;
            }
            if (result <= 0) {
                return;
            }
            offset += static_cast<size_t>(result);
        }
    }

    mutable std::mutex mutex_;
    std::ofstream console_;
    std::ofstream log_;
    bool logRequired_ = false;
};

bool RandomBytes(void* output, size_t size)
{
    auto* cursor = static_cast<unsigned char*>(output);
    size_t total = 0;
    while (total < size) {
        const ssize_t result = getrandom(cursor + total, size - total, 0);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return false;
        }
        total += static_cast<size_t>(result);
    }
    return true;
}

// 生成 32 个十六进制字符（16 字节随机数）。失败返回空串。
// 唯一用途是 UDS 抽象地址的随机段；会话 nonce 已随协议简化删除。
std::string HexRandom()
{
    std::array<unsigned char, 16> bytes{};
    if (!RandomBytes(bytes.data(), bytes.size())) {
        return {};
    }
    constexpr char kHexDigits[] = "0123456789abcdef";
    std::string text(bytes.size() * 2, '0');
    for (size_t index = 0; index < bytes.size(); ++index) {
        text[index * 2] = kHexDigits[bytes[index] >> 4u];
        text[index * 2 + 1] = kHexDigits[bytes[index] & 0x0fu];
    }
    return text;
}

bool CreateSessionDirectory(std::string& directory, std::string& error)
{
    char pattern[] = "/tmp/npu_check-XXXXXX";
    char* created = mkdtemp(pattern);
    if (created == nullptr) {
        error = std::string("mkdtemp: ") + std::strerror(errno);
        return false;
    }
    if (chmod(created, 0700) != 0) {
        const int chmodError = errno;
        (void)rmdir(created);
        error = std::string("chmod session directory: ") + std::strerror(chmodError);
        return false;
    }
    directory = created;
    return true;
}

bool EnsureDirectory(const std::string& path, std::string& error)
{
    std::error_code filesystemError;
    std::filesystem::create_directories(path, filesystemError);
    if (filesystemError) {
        error = "cannot create directory '" + path + "': " + filesystemError.message();
        return false;
    }
    return true;
}

std::vector<char*> BuildArgv(const std::vector<std::string>& arguments)
{
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }
    argv.push_back(nullptr);
    return argv;
}

uint64_t ParseSummaryErrors(const std::string& summary)
{
    const std::string key = "errors=";
    const size_t position = summary.find(key);
    if (position == std::string::npos) {
        return 0;
    }
    const char* start = summary.data() + position + key.size();
    const char* end = summary.data() + summary.size();
    uint64_t value = 0;
    const auto result = std::from_chars(start, end, value);
    return result.ec == std::errc{} && result.ptr != start ? value : 0;
}

int ChildExitCode(int status)
{
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 125;
}

} // namespace

// TODO(configure-registry): Configure 迁移到选项注册表编码后删除本函数。
// 注入库当前仍消费字符串式的 ipc::ToolConfig，这里由新的 Options 派生出来，
// 使参数解析可以先行改造完毕，而不必同时改动线路格式。
ipc::ToolConfig LegacyToolConfig(const Options& options, const std::string& sessionDirectory)
{
    ipc::ToolConfig config;
    if (!options.tools.empty()) {
        config.toolName = ipc::ToolName(options.tools.front().toolId);
    }
    config.strict = true;
    config.keepTemp = false;
    config.logFile = options.logFile;
    config.workDir = sessionDirectory;
    config.probeCacheDir = sessionDirectory + "/probe-cache";
    return config;
}

int RunApplication(const Options& options, const std::string& libraryPath)
{
    // 旧 Configure 只能承载单个工具名。解析层已经支持多工具，这里在下发前拦下，
    // 避免静默地只启用第一个工具、让用户以为其余工具也在生效。
    if (options.tools.size() > 1) {
        std::cerr << "npu_check: the current injection library accepts a single --tool; "
                     "multi-tool Configure requires the registry encoding\n";
        return 125;
    }

    std::string sessionDirectory;
    std::string error;
    if (!CreateSessionDirectory(sessionDirectory, error)) {
        std::cerr << "npu_check: " << error << '\n';
        return 125;
    }
    const ipc::ToolConfig toolConfig = LegacyToolConfig(options, sessionDirectory);
    if (!EnsureDirectory(toolConfig.workDir, error) || !EnsureDirectory(toolConfig.probeCacheDir, error)) {
        std::cerr << "npu_check: " << error << '\n';
        return 125;
    }

    const std::string consolePath = sessionDirectory + "/console.log";
    // UDS 地址走抽象命名空间，不在会话目录里创建 socket 文件。前导 '@' 是与注入库
    // 之间的书写约定，两端填 sockaddr_un 时会把它换成 '\0'（见 BuildAbstractAddress）。
    // 随机部分保证并发实例之间不撞车；它不是秘密 —— /proc/net/unix 任何 UID 都能读到，
    // 身份校验完全由 SO_PEERCRED 承担。
    const std::string udsName = "@npu_check-" + HexRandom();
    if (udsName.size() <= 11) {
        std::cerr << "npu_check: cannot generate a UDS address\n";
        return 125;
    }
    OutputSink output(consolePath, toolConfig.logFile);
    if (!output.Good()) {
        std::cerr << "npu_check: cannot open console or log output\n";
        return 125;
    }
    // 会话标识只剩 session_id 一个：它在每一帧都被校验，覆盖面严格大于原先只在 Hello
    // 校验一次的 nonce，因此 nonce 已删除（见 wire_protocol.h 的说明）。
    uint64_t sessionId = 0;
    if (!RandomBytes(&sessionId, sizeof(sessionId)) || sessionId == 0) {
        output.Sanitizer("cannot generate session identity", true);
        return 125;
    }

    int consolePipe[2] = {-1, -1};
    if (pipe2(consolePipe, O_CLOEXEC) != 0) {
        output.Sanitizer(std::string("pipe2: ") + std::strerror(errno), true);
        return 125;
    }
    UniqueFd consoleRead(consolePipe[0]);
    UniqueFd consoleWrite(consolePipe[1]);
    SignalBlocker signalBlocker(error);
    if (!signalBlocker.Good()) {
        output.Sanitizer(error, true);
        return 125;
    }
    const pid_t child = fork();
    if (child < 0) {
        output.Sanitizer(std::string("fork: ") + std::strerror(errno), true);
        return 125;
    }
    if (child == 0) {
        if (setpgid(0, 0) != 0) {
            dprintf(STDERR_FILENO, "npu_check: setpgid failed: %s\n", std::strerror(errno));
            _exit(127);
        }
        if (!signalBlocker.Restore()) {
            dprintf(STDERR_FILENO, "npu_check: cannot restore child signal mask\n");
            _exit(127);
        }
        consoleRead.Reset();
        if (dup2(consoleWrite.Get(), STDOUT_FILENO) < 0 || dup2(consoleWrite.Get(), STDERR_FILENO) < 0) {
            _exit(127);
        }
        consoleWrite.Reset();
        const std::string sessionText = std::to_string(sessionId);
        const std::string parentText = std::to_string(getppid());
        const std::string timeoutText = std::to_string(options.handshakeTimeoutMs);
        (void)setenv("ACL_API_INJECTION", libraryPath.c_str(), 1);
        (void)setenv(ipc::kUdsNameEnv, udsName.c_str(), 1);
        (void)setenv(ipc::kSessionIdEnv, sessionText.c_str(), 1);
        (void)setenv(ipc::kCliPidEnv, parentText.c_str(), 1);
        (void)setenv(ipc::kHandshakeTimeoutEnv, timeoutText.c_str(), 1);
        std::string environmentError;
        if (!ApplyEnvironment(BuildDbiEnvironment(options.toolConfig), environmentError)) {
            dprintf(STDERR_FILENO, "npu_check: %s\n", environmentError.c_str());
            _exit(126);
        }
        auto argv = BuildArgv(options.application);
        execvp(argv[0], argv.data());
        dprintf(STDERR_FILENO, "npu_check: execvp failed: %s\n", std::strerror(errno));
        _exit(127);
    }
    consoleWrite.Reset();
    if (setpgid(child, child) != 0 && errno != EACCES && errno != ESRCH) {
        output.Sanitizer(std::string("setpgid: ") + std::strerror(errno), true);
        (void)kill(child, SIGKILL);
        (void)waitpid(child, nullptr, 0);
        return 125;
    }
    SignalForwarder signalForwarder(child, error);
    if (!signalForwarder.Good()) {
        output.Sanitizer(error, true);
        if (kill(-child, SIGKILL) != 0) {
            (void)kill(child, SIGKILL);
        }
        (void)waitpid(child, nullptr, 0);
        return 125;
    }
    if (!signalBlocker.Restore(error)) {
        output.Sanitizer(error, true);
        if (kill(-child, SIGKILL) != 0) {
            (void)kill(child, SIGKILL);
        }
        (void)waitpid(child, nullptr, 0);
        return 125;
    }

    output.Sanitizer("session=" + std::to_string(sessionId) + " console=\"" + consolePath + "\"");
    std::atomic<bool> childExited{false};
    std::thread consoleReader([&output, &childExited, fd = std::move(consoleRead)] {
        std::array<char, 8192> buffer{};
        std::chrono::steady_clock::time_point drainDeadline{};
        while (true) {
            pollfd descriptor{};
            descriptor.fd = fd.Get();
            descriptor.events = POLLIN;
            int pollResult = 0;
            do {
                pollResult = poll(&descriptor, 1, 100);
            } while (pollResult < 0 && errno == EINTR);
            if (pollResult < 0 || (descriptor.revents & (POLLERR | POLLNVAL)) != 0) {
                break;
            }
            if ((descriptor.revents & POLLIN) != 0) {
                const ssize_t bytes = read(fd.Get(), buffer.data(), buffer.size());
                if (bytes < 0 && errno == EINTR) {
                    continue;
                }
                if (bytes <= 0) {
                    break;
                }
                output.Console(buffer.data(), static_cast<size_t>(bytes));
            } else if ((descriptor.revents & POLLHUP) != 0) {
                break;
            }
            if (!childExited) {
                continue;
            }
            if (drainDeadline.time_since_epoch().count() == 0) {
                drainDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            } else if (std::chrono::steady_clock::now() >= drainDeadline) {
                break;
            }
        }
    });

    UdsClient client;
    std::string ready;
    const bool handshake = client.ConnectAndConfigure(
        udsName, sessionId, static_cast<uint32_t>(child), options.handshakeTimeoutMs, toolConfig, ready, error);
    std::atomic<bool> sessionEnd{false};
    std::atomic<bool> protocolComplete{handshake};
    std::atomic<uint64_t> sanitizerErrors{0};
    std::thread receiver;
    if (handshake) {
        output.Sanitizer("handshake=ready " + ready);
        receiver = std::thread([&] {
            while (true) {
                ipc::Frame frame{};
                std::string receiveError;
                // 采集阶段不设超时：应用可能跑数小时，任何固定值都是错的。终止由
                // 对端关闭连接（CLOSED）或子进程退出驱动，见下面的分支。
                const ipc::IoStatus status = client.Receive(frame, ipc::kNoDeadline, receiveError);
                if (status == ipc::IoStatus::CLOSED) {
                    break;
                }
                if (status == ipc::IoStatus::TIMEOUT) {
                    if (childExited) {
                        break;
                    }
                    continue;
                }
                if (status != ipc::IoStatus::OK) {
                    protocolComplete = false;
                    output.Sanitizer("UDS receive failed: " + receiveError, true);
                    break;
                }
                std::string message;
                if (!ipc::DecodeText(frame.payload, message, receiveError)) {
                    protocolComplete = false;
                    output.Sanitizer("UDS payload failed: " + receiveError, true);
                    break;
                }
                const bool isError =
                    frame.type == ipc::MessageType::DIAGNOSTIC || frame.type == ipc::MessageType::ERROR;
                output.Sanitizer(std::string(ipc::MessageTypeName(frame.type)) + " " + message, isError);
                if (frame.type == ipc::MessageType::SUMMARY) {
                    sanitizerErrors = ParseSummaryErrors(message);
                } else if (frame.type == ipc::MessageType::SESSION_END) {
                    sessionEnd = message.find("status=complete") != std::string::npos;
                    break;
                } else if (frame.type == ipc::MessageType::ERROR) {
                    protocolComplete = false;
                }
            }
        });
    } else {
        protocolComplete = false;
        output.Sanitizer("handshake=missing reason=\"" + error + "\"", true);
    }

    int childStatus = 0;
    while (waitpid(child, &childStatus, 0) < 0) {
        if (errno != EINTR) {
            childStatus = 125 << 8;
            break;
        }
    }
    childExited = true;
    consoleReader.join();
    if (receiver.joinable()) {
        receiver.join();
    }
    const int childExit = ChildExitCode(childStatus);
    output.Sanitizer(
        "child_exit=" + std::to_string(childExit) + " handshake=" + (handshake ? "ready" : "missing") +
        " session_end=" + (sessionEnd ? "complete" : "missing"));

    int result = 0;
    if (childExit != 0) {
        result = childExit;
    } else if (!handshake || !protocolComplete || !sessionEnd) {
        result = 125;
    } else if (sanitizerErrors != 0) {
        // 目标行为是"默认透传应用退出码"，由内部选项 --error-exitcode 覆盖。这里先接上
        // --error-exitcode；把未指定时的默认值从 2 翻转为透传会改变现有 demo 的退出码，
        // 留待报告语义改造时一并调整。
        result = options.errorExitCode != 0 ? options.errorExitCode : 2;
    }

    std::error_code cleanupError;
    std::filesystem::remove_all(sessionDirectory, cleanupError);
    return result;
}

} // namespace npu::sanitizer::cli
