#ifndef NPU_CHECK_CLI_OPTIONS_H
#define NPU_CHECK_CLI_OPTIONS_H

#include "wire_protocol.h"

#include <string>
#include <vector>

namespace npu::sanitizer::cli {

struct Options {
    ipc::ToolConfig toolConfig;
    std::string libraryPath;
    int handshakeTimeoutMs = 10000;
    bool showHelp = false;
    std::vector<std::string> application;
};

bool ParseOptions(int argc, char** argv, Options& options, std::string& error);
bool ResolveLibraryPath(const std::string& requested, std::string& resolved, std::string& error);
std::string Usage();

} // namespace npu::sanitizer::cli

#endif
