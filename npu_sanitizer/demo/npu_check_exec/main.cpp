#include "internal/aclsan_log.h"

#include <spawn.h>
#include <sys/wait.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

extern char** environ;

namespace {

constexpr char kInjectionLibraryName[] = "libnpu_check.so";

struct LaunchOptions {
    bool memcheckEnabled = false;
    bool synccheckEnabled = false;
    std::vector<char*> childArguments;
};

// 作用：解析一个或多个工具选项，并保留分隔符之后的原始子进程 argv。
bool ParseArguments(int argc, char* argv[], LaunchOptions* options)
{
    if (options == nullptr) {
        return false;
    }

    int index = 1;
    for (; index < argc && std::strcmp(argv[index], "--") != 0; ++index) {
        if (std::strcmp(argv[index], "--tool") != 0 || index + 1 >= argc) {
            ASC_SAN_ERROR("usage: npucheck --tool memcheck|synccheck [--tool ...] -- command [args...]");
            return false;
        }

        const char* tool = argv[++index];
        if (std::strcmp(tool, "memcheck") == 0) {
            options->memcheckEnabled = true;
        } else if (std::strcmp(tool, "synccheck") == 0) {
            options->synccheckEnabled = true;
        } else {
            ASC_SAN_ERROR("unsupported tool '%s': expected memcheck or synccheck", tool);
            return false;
        }
    }

    if (!options->memcheckEnabled && !options->synccheckEnabled) {
        ASC_SAN_ERROR("at least one --tool option is required");
        return false;
    }
    if (index >= argc || std::strcmp(argv[index], "--") != 0 || index + 1 >= argc) {
        ASC_SAN_ERROR("a command is required after --");
        return false;
    }

    options->childArguments.reserve(static_cast<std::size_t>(argc - index));
    for (++index; index < argc; ++index) {
        options->childArguments.push_back(argv[index]);
    }
    options->childArguments.push_back(nullptr);
    return true;
}

// 作用：覆盖父进程中的工具环境，使子进程继承明确的工具开关和 Injection SONAME。
bool ConfigureEnvironment(const LaunchOptions& options)
{
    if (setenv("ASC_SAN_MEMCHECK", options.memcheckEnabled ? "1" : "0", 1) != 0 ||
        setenv("ASC_SAN_SYNCCHECK", options.synccheckEnabled ? "1" : "0", 1) != 0 ||
        setenv("ACL_API_INJECTION", kInjectionLibraryName, 1) != 0) {
        ASC_SAN_ERROR("failed to configure sanitizer environment: %s", std::strerror(errno));
        return false;
    }
    return true;
}

// 作用：按固定顺序输出幂等归一化后的工具集合。
void PrintConfiguredTools(const LaunchOptions& options)
{
    std::cout << "[npucheck] configured tools=";
    if (options.memcheckEnabled) {
        std::cout << "memcheck";
    }
    if (options.memcheckEnabled && options.synccheckEnabled) {
        std::cout << ',';
    }
    if (options.synccheckEnabled) {
        std::cout << "synccheck";
    }
    std::cout << std::endl;
}

// 作用：等待指定子进程；信号打断 waitpid 时继续等待同一个 PID。
bool WaitForChild(pid_t child, int* status)
{
    if (status == nullptr) {
        return false;
    }

    while (waitpid(child, status, 0) == -1) {
        if (errno != EINTR) {
            ASC_SAN_ERROR("waitpid failed: %s", std::strerror(errno));
            return false;
        }
    }
    return true;
}

// 作用：把子进程的正常退出码或终止信号转换为 launcher 的退出状态。
int ChildExitStatus(int status)
{
    if (WIFEXITED(status)) {
        const int exitCode = WEXITSTATUS(status);
        std::cout << "[npucheck] child exited status=" << exitCode << '\n';
        return exitCode;
    }
    if (WIFSIGNALED(status)) {
        const int signalNumber = WTERMSIG(status);
        std::cout << "[npucheck] child terminated signal=" << signalNumber << '\n';
        return 128 + signalNumber;
    }

    ASC_SAN_ERROR("[npucheck] child ended with an unsupported wait status");
    return EXIT_FAILURE;
}

} // namespace

// 作用：配置 sanitizer 环境，保留父进程启动并等待 B，随后执行后续操作。
int main(int argc, char* argv[])
{
    LaunchOptions options;
    if (!ParseArguments(argc, argv, &options) || !ConfigureEnvironment(options)) {
        return EXIT_FAILURE;
    }

    PrintConfiguredTools(options);

    pid_t child = 0;
    const int spawnResult =
        posix_spawnp(&child, options.childArguments.front(), nullptr, nullptr, options.childArguments.data(), environ);
    if (spawnResult != 0) {
        ASC_SAN_ERROR("posix_spawnp failed: %s", std::strerror(spawnResult));
        return EXIT_FAILURE;
    }

    int status = 0;
    if (!WaitForChild(child, &status)) {
        return EXIT_FAILURE;
    }

    const int exitCode = ChildExitStatus(status);
    std::cout << "[npucheck] post action complete" << std::endl;
    return exitCode;
}
