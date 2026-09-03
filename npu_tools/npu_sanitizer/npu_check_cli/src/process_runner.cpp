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

#include "uds_client.h"

#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <utility>
#include <unistd.h>

namespace npu::sanitizer::cli {
namespace {

volatile sig_atomic_t g_childProcessGroup = -1;

// 结构化维测日志的开关变量。
constexpr const char* kCliDebugEnv = "NPU_CHECK_CLI_DEBUG";

// 取值必须严格等于 "1"。宽松匹配（例如只看首字符）会让 "0"、"false"、"1x" 这类取值
// 意外打开日志，用户很难意识到是自己写的值被曲解了。这与注入库侧
// AclsanIsStdoutLogEnabled() 读 NPU_SAN_DEBUG 的判定保持一致。
bool IsCliDebugEnabled() noexcept
{
    const char* value = std::getenv(kCliDebugEnv);
    return value != nullptr && value[0] == '1' && value[1] == '\0';
}

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

// 报告出口。三条流各有去向，但都必须串行化 —— 否则应用输出与检查报告会在字节级交错。
//
// 旧实现还往会话目录里写一份 console.log。那个出口已经取消：会话目录随抽象命名空间
// 的改造失去了载体，而它承载的内容与 --log-file 完全重复。
class OutputSink {
public:
    explicit OutputSink(const std::string& logPath) : logRequired_(!logPath.empty())
    {
        if (!logPath.empty()) {
            log_.open(logPath, std::ios::binary | std::ios::app);
        }
    }

    bool Good() const { return !logRequired_ || log_.is_open(); }

    // 应用自身的 stdout/stderr：原样转发，不加任何前缀，不改写。
    void Console(const char* data, size_t size)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        WriteFd(STDOUT_FILENO, data, size);
        if (log_.is_open()) {
            log_.write(data, static_cast<std::streamsize>(size));
            log_.flush();
        }
    }

    // 结构化维测日志（6.1 的 [CLI] / [UDS] / [INJECTION] 三类）：默认不输出，
    // 由 NPU_CHECK_CLI_DEBUG=1 打开；开启后去 stderr，指定了 --log-file 时另存一份。
    //
    // 去 stderr 而不是 stdout，是为了不污染被脚本采集的应用输出与检查报告。
    //
    // 默认关闭的理由：这些是逐阶段的过程记录（注入库定位、会话标识、connect 重试次数、
    // 握手凭据比对、Configure 长度、Result 帧数……），排障时才有价值，平时只会把用户
    // 真正要看的检查报告淹掉。
    //
    // 注意 2.3.5 的结果摘要行 [CLI] outcome=... 不走这里 —— 它是 V1 唯一的对外结果
    // 信号（5.4），任何路径下都必须输出，绝不能被调试开关吞掉。
    void Structured(const std::string& line)
    {
        if (!IsCliDebugEnabled()) {
            return;
        }
        std::string text = line;
        if (text.empty() || text.back() != '\n') {
            text.push_back('\n');
        }
        std::lock_guard<std::mutex> lock(mutex_);
        WriteFd(STDERR_FILENO, text.data(), text.size());
        if (log_.is_open()) {
            log_.write(text.data(), static_cast<std::streamsize>(text.size()));
            log_.flush();
        }
    }

    // 权威报告的输出通道：原样写出，不加 "npu_check: " 前缀，也不拆行。报告是给人读的
    // 多行文本，逐行加前缀只会破坏渲染器精心排好的版式。
    void Report(const std::string& text)
    {
        if (text.empty()) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        WriteFd(STDOUT_FILENO, text.data(), text.size());
        if (text.back() != '\n') {
            const char newline = '\n';
            WriteFd(STDOUT_FILENO, &newline, 1);
        }
        if (log_.is_open()) {
            log_.write(text.data(), static_cast<std::streamsize>(text.size()));
            if (text.back() != '\n') {
                log_.put('\n');
            }
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
    std::ofstream log_;
    bool logRequired_ = false;
};

bool ReadRandomDevice(unsigned char* output, size_t size)
{
    int fd = -1;
    do {
        fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0) {
        return false;
    }

    UniqueFd randomDevice(fd);
    size_t total = 0;
    while (total < size) {
        const ssize_t result = read(randomDevice.Get(), output + total, size - total);
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

bool RandomBytes(void* output, size_t size)
{
    auto* cursor = static_cast<unsigned char*>(output);
    size_t total = 0;
#ifdef SYS_getrandom
    while (total < size) {
        const ssize_t result = syscall(SYS_getrandom, cursor + total, size - total, 0);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0 && errno == ENOSYS) {
            break;
        }
        if (result <= 0) {
            return false;
        }
        total += static_cast<size_t>(result);
    }
    if (total == size) {
        return true;
    }
#endif
    return ReadRandomDevice(cursor + total, size - total);
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
    boost::system::error_code filesystemError;
    boost::filesystem::create_directories(path, filesystemError);
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

int ChildExitCode(int status)
{
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 125;
}

// 摘要行里的 child_exit 字段。被信号杀死与正常退出必须能区分开：两者映射到同一个
// 数值空间时，"退出码 137" 和 "被 SIGKILL 杀死" 就再也分不出来了。
std::string FormatChildExit(int status)
{
    if (WIFSIGNALED(status)) {
        return "signal:" + std::to_string(WTERMSIG(status));
    }
    return WIFEXITED(status) ? std::to_string(WEXITSTATUS(status)) : "unknown";
}

// --log-file 的三种形态：未指定 → 空；已存在的目录 → <dir>/npu_check-<session>.log；
// 其余一律当作文件路径。
bool ResolveLogPath(const std::string& requested, uint64_t sessionId, std::string& resolved, std::string& error)
{
    if (requested.empty()) {
        resolved.clear();
        return true;
    }
    boost::system::error_code filesystemError;
    if (boost::filesystem::is_directory(requested, filesystemError) && !filesystemError) {
        // 文件名带上 session：同一目录下并发跑多个实例不会互相覆盖。
        resolved = (boost::filesystem::path(requested) / ("npu_check-" + std::to_string(sessionId) + ".log")).string();
        return true;
    }
    const auto parent = boost::filesystem::path(requested).parent_path();
    if (!parent.empty() && !boost::filesystem::exists(parent, filesystemError)) {
        error = "log file directory does not exist: " + parent.string();
        return false;
    }
    resolved = requested;
    return true;
}

// 把 Options 里的工具集合转成线路上的 Configure 请求。解析阶段已经保证了排序与去重，
// 这里只做搬运，不再重新规范化 —— 若两处各做一遍，规则一旦分叉就很难发现。
ipc::ConfigureRequest BuildConfigureRequest(const Options& options)
{
    ipc::ConfigureRequest request;
    request.globalFlags = 0;
    request.tools = options.tools;
    return request;
}

} // namespace

std::string FormatResultSummary(const ResultSummary& summary)
{
    const char* outcome = "infra_failed";
    switch (summary.outcome) {
        case Outcome::kForwarded:
            outcome = "forwarded";
            break;
        case Outcome::kAppFailed:
            outcome = "app_failed";
            break;
        case Outcome::kInfraFailed:
            outcome = "infra_failed";
            break;
    }
    std::string hasErrors = "unknown";
    if (summary.hasErrors == 0) {
        hasErrors = "0";
    } else if (summary.hasErrors > 0) {
        hasErrors = "1";
    }
    return std::string("[CLI] outcome=") + outcome + " has_errors=" + hasErrors +
           " truncated=" + (summary.truncated ? "1" : "0") + " child_exit=" + summary.childExit +
           " exit=" + std::to_string(summary.exit);
}

int RunApplication(const Options& options, const std::string& libraryPath)
{
    // 摘要行必须在任何返回路径上都输出一次，因此用一个 RAII 守卫兜住，而不是在每个
    // return 前手写一遍 —— 后者只要漏掉一处，脚本就会在那条路径上什么都读不到。
    ResultSummary summary;
    struct SummaryGuard {
        const ResultSummary& summary;
        ~SummaryGuard() { std::cerr << FormatResultSummary(summary) << '\n'; }
    } summaryGuard{summary};

    std::string sessionDirectory;
    std::string error;
    if (!CreateSessionDirectory(sessionDirectory, error)) {
        std::cerr << "npu_check: " << error << '\n';
        return summary.exit = 125;
    }

    // 会话标识：它在每一帧都被校验，覆盖面严格大于原先只在 Hello 校验一次的 nonce，
    // 因此 nonce 已删除（见 wire_protocol.h 的说明）。
    uint64_t sessionId = 0;
    if (!RandomBytes(&sessionId, sizeof(sessionId)) || sessionId == 0) {
        std::cerr << "npu_check: cannot generate session identity\n";
        return summary.exit = 125;
    }

    // --work-dir 只控制 CLI 会话日志。DBI 使用自己管理的私有 runtime/cache 目录。
    const std::string workDir = options.workDir.empty() ? sessionDirectory : options.workDir;
    if (!EnsureDirectory(workDir, error)) {
        std::cerr << "npu_check: " << error << '\n';
        return summary.exit = 125;
    }

    std::string logPath;
    if (!ResolveLogPath(options.logFile, sessionId, logPath, error)) {
        std::cerr << "npu_check: " << error << '\n';
        return summary.exit = 125;
    }

    // UDS 地址走抽象命名空间，不在文件系统里创建 socket 文件。前导 '@' 是与注入库之间
    // 的书写约定，两端填 sockaddr_un 时会把它换成 '\0'（见 BuildAbstractAddress）。
    // 随机部分保证并发实例之间不撞车；它不是秘密 —— /proc/net/unix 任何 UID 都能读到，
    // 身份校验完全由 SO_PEERCRED 承担。
    const std::string udsName = "@npu_check-" + HexRandom();
    if (udsName.size() <= 11) {
        std::cerr << "npu_check: cannot generate a UDS address\n";
        return summary.exit = 125;
    }
    OutputSink output(logPath);
    if (!output.Good()) {
        std::cerr << "npu_check: cannot open '" << logPath << "' for writing\n";
        return summary.exit = 125;
    }

    const auto configure = BuildConfigureRequest(options);
    std::string toolNames;
    for (const auto& tool : options.tools) {
        toolNames += (toolNames.empty() ? "" : ",");
        toolNames += ipc::ToolName(tool.toolId);
    }
    output.Structured("[INJECTION] library=" + libraryPath + " result=resolved");

    int consolePipe[2] = {-1, -1};
    if (pipe2(consolePipe, O_CLOEXEC) != 0) {
        output.Sanitizer(std::string("pipe2: ") + std::strerror(errno), true);
        return summary.exit = 125;
    }
    UniqueFd consoleRead(consolePipe[0]);
    UniqueFd consoleWrite(consolePipe[1]);
    SignalBlocker signalBlocker(error);
    if (!signalBlocker.Good()) {
        output.Sanitizer(error, true);
        return summary.exit = 125;
    }

    // T0 必须取在 fork 之前的最后一步。
    //
    // 用户感知的是"从敲回车到应用真正开跑"这一整段，其中最不可控的恰恰是 exec 与
    // CANN 动态库加载。若把起点挪到 fork 之后、甚至 connect 成功之后，这段耗时就不
    // 占预算，用户设定的超时形同虚设。
    const ipc::DeadlineMs handshakeDeadline = ipc::DeadlineAfterMs(options.handshakeTimeoutMs);

    const pid_t child = fork();
    if (child < 0) {
        output.Sanitizer(std::string("fork: ") + std::strerror(errno), true);
        return summary.exit = 125;
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
        // 工作目录经环境变量下发：Configure 改用注册表编码后只承载工具与子选项，
        // 路径这类与协议无关的部署信息不再占线路。
        (void)setenv(ipc::kWorkDirEnv, workDir.c_str(), 1);
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
        return summary.exit = 125;
    }
    SignalForwarder signalForwarder(child, error);
    if (!signalForwarder.Good()) {
        output.Sanitizer(error, true);
        if (kill(-child, SIGKILL) != 0) {
            (void)kill(child, SIGKILL);
        }
        (void)waitpid(child, nullptr, 0);
        return summary.exit = 125;
    }
    if (!signalBlocker.Restore(error)) {
        output.Sanitizer(error, true);
        if (kill(-child, SIGKILL) != 0) {
            (void)kill(child, SIGKILL);
        }
        (void)waitpid(child, nullptr, 0);
        return summary.exit = 125;
    }

    output.Structured(
        "[CLI] session=" + std::to_string(sessionId) + " tools=" + toolNames + " app_pid=" + std::to_string(child) +
        " app_pgid=" + std::to_string(child));
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
    client.SetLogSink([&output](const std::string& line) { output.Structured(line); });
    const bool handshake = client.ConnectAndConfigure(
        udsName, sessionId, static_cast<uint32_t>(child), handshakeDeadline, configure, error);
    // Result 分片的拼接缓冲。只有 receiver 线程写，join 之后主线程才读。
    std::string result;
    std::atomic<bool> resultComplete{false};
    std::atomic<bool> resultHasErrors{false};
    std::atomic<bool> resultTruncated{false};
    std::atomic<uint64_t> resultFrames{0};
    std::atomic<bool> protocolComplete{handshake};
    std::thread receiver;
    if (handshake) {
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
                if (frame.type == ipc::MessageType::RESULT) {
                    std::string message;
                    if (!ipc::DecodeText(frame.payload, message, receiveError)) {
                        protocolComplete = false;
                        output.Sanitizer("UDS payload failed: " + receiveError, true);
                        break;
                    }
                    // 上限与注入库侧共用同一个常量：对端异常时不能让 CLI 一直吃内存。
                    if (result.size() + message.size() > ipc::kMaxResultBytes) {
                        protocolComplete = false;
                        output.Sanitizer("result exceeds the maximum report size", true);
                        break;
                    }
                    result.append(message);
                    ++resultFrames;
                    if ((frame.flags & ipc::kFlagMore) != 0) {
                        continue;
                    }
                    // MORE=0 的末帧到达即报告完整；结论位只在这一帧上有效，前面的分片
                    // 不带任何结论，所以这里必须读末帧的 flags 而不是任意一帧的。
                    resultHasErrors = (frame.flags & ipc::kFlagHasErrors) != 0;
                    resultTruncated = (frame.flags & ipc::kFlagTruncated) != 0;
                    resultComplete = true;
                    break;
                }
                if (frame.type == ipc::MessageType::ERROR) {
                    // Error 表示注入库侧基础设施失败，本次检查没有可信结论，不再等 Result。
                    protocolComplete = false;
                    ipc::ErrorPayload failure{};
                    std::string decodeError;
                    if (!ipc::DecodeError(frame.payload, failure, decodeError)) {
                        output.Sanitizer("ERROR malformed error payload: " + decodeError, true);
                        break;
                    }
                    // domain/code 是稳定取值进结构化日志，message 只原样转述给人看，
                    // 不参与任何判定。
                    output.Structured(
                        "[UDS] phase=error domain=" + std::to_string(static_cast<unsigned>(failure.domain)) +
                        " code=" + std::to_string(failure.code));
                    output.Sanitizer("ERROR " + failure.message, true);
                    break;
                }
                // 其余都是 must-ignore 的实时诊断，收到即打印，不进拼接缓冲。
                std::string message;
                if (!ipc::DecodeText(frame.payload, message, receiveError)) {
                    protocolComplete = false;
                    output.Sanitizer("UDS payload failed: " + receiveError, true);
                    break;
                }
                output.Sanitizer(std::string(ipc::MessageTypeName(frame.type)) + " " + message, true);
            }
        });
    } else {
        protocolComplete = false;
        output.Structured("[UDS] phase=handshake result=failed");
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

    // 判定顺序是固定的：先看有没有收到 MORE=0 的末帧，再看是不是被 Error 打断，
    // 最后才是"连接断了但报告没收全"。
    if (resultComplete) {
        output.Structured(
            "[UDS] phase=result frames=" + std::to_string(resultFrames.load()) +
            " bytes=" + std::to_string(result.size()) + " truncated=" + (resultTruncated ? "1" : "0") +
            " has_errors=" + (resultHasErrors ? "1" : "0"));
        output.Report(result);
        if (resultTruncated) {
            output.Sanitizer("report truncated: the diagnostic buffer reached its size limit", true);
        }
    } else if (handshake) {
        // 报告缺失或截断：已经收到的分片一律丢弃。半份报告看上去和完整报告没有区别，
        // 输出它等于让用户把"没查到问题"和"没查完"混为一谈。
        output.Structured(
            "[UDS] phase=result frames=" + std::to_string(resultFrames.load()) +
            " bytes=" + std::to_string(result.size()) + " truncated=unknown has_errors=unknown");
        output.Sanitizer("result missing or truncated; the partial report was discarded", true);
    }

    summary.truncated = resultTruncated;
    summary.childExit = FormatChildExit(childStatus);
    int exitCode = 0;
    if (!handshake || !protocolComplete || !resultComplete) {
        // 基础设施失败：拿不到可信结论，退 125 与应用自身的退出码区分开。
        // has_errors 保持 unknown —— 没收到完整 Result 就没有结论可言。
        summary.outcome = Outcome::kInfraFailed;
        exitCode = 125;
    } else {
        summary.hasErrors = resultHasErrors ? 1 : 0;
        // 默认透传应用退出码：检出问题不等于运行失败，是否要让 CI 挂掉由调用方决定。
        // 只有显式指定 --error-exitcode 时才用它覆盖。
        exitCode = childExit;
        if (resultHasErrors && options.errorExitCode != 0) {
            exitCode = options.errorExitCode;
        }
        // 检出问题不算 app_failed：检查跑完了、报告也拿到了，结论由 has_errors 承载。
        summary.outcome = childExit == 0 ? Outcome::kForwarded : Outcome::kAppFailed;
    }
    summary.exit = exitCode;

    // 只删自己 mkdtemp 出来的会话目录。--work-dir 指定的目录是用户的，即便本次的
    // npu_check.log 和 probe 缓存就落在里面，也一律不碰 —— 递归删一个用户给的路径
    // 是不可逆的，代价远大于留下几个文件。
    boost::system::error_code cleanupError;
    boost::filesystem::remove_all(sessionDirectory, cleanupError);
    return exitCode;
}

} // namespace npu::sanitizer::cli
