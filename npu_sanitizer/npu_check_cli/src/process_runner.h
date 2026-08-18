#ifndef NPU_CHECK_CLI_PROCESS_RUNNER_H
#define NPU_CHECK_CLI_PROCESS_RUNNER_H

#include "options.h"

#include <string>

namespace npu::sanitizer::cli {

int RunApplication(const Options& options, const std::string& libraryPath);

} // namespace npu::sanitizer::cli

#endif
