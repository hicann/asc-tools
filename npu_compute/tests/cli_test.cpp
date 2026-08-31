/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "config.h"
#include "launcher.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int Check(bool condition, const char* expression, int line)
{
    if (condition) {
        return 0;
    }
    std::fprintf(stderr, "line %d: check failed: %s\n", line, expression);
    return 1;
}

#define CHECK(expression)                               \
    do {                                                \
        if (Check((expression), #expression, __LINE__)) \
            return 1;                                   \
    } while (false)

using npu_compute::compute_launcher::CliConfig;
using npu_compute::compute_launcher::ParseCli;
using npu_compute::compute_launcher::PrintUsage;

bool Parse(const std::vector<std::string>& arguments, CliConfig* config, std::string* error)
{
    std::vector<std::string> storage = arguments;
    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (std::string& argument : storage) {
        argv.push_back(argument.data());
    }
    return ParseCli(static_cast<int>(argv.size()), argv.data(), config, error);
}

std::string ReadStream(FILE* stream)
{
    std::string content;
    std::rewind(stream);
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), stream) != nullptr) {
        content += buffer;
    }
    return content;
}

int TestCollectionExport()
{
    CliConfig config;
    std::string error;
    CHECK(Parse(
        {"npu-compute", "--section", "PipeUtilization", "-o", "result.npu-rep", "./app", "--app-value"}, &config,
        &error));
    CHECK(error.empty());
    CHECK(config.export_path == "result.npu-rep");
    CHECK(config.program == "./app");
    CHECK(config.program_arguments == std::vector<std::string>{"--app-value"});

    CHECK(Parse({"npu-compute", "--section", "Memory", "--export", "reports", "./app"}, &config, &error));
    CHECK(config.export_path == "reports");
    return 0;
}

int TestBusinessExitCodes()
{
    CHECK(npu_compute::compute_launcher::kUsageErrorExitCode == 2);
    CHECK(npu_compute::compute_launcher::kCollectionErrorExitCode == 3);
    CHECK(npu_compute::compute_launcher::kReportErrorExitCode == 4);
    CHECK(npu_compute::compute_launcher::kInternalErrorExitCode == 5);
    return 0;
}

int TestImportExportParsing()
{
    CliConfig config;
    std::string error;
    CHECK(Parse({"npu-compute", "--import", "old.npu-rep", "--export", "new.npu-rep"}, &config, &error));
    CHECK(config.import_path == "old.npu-rep");
    CHECK(config.export_path == "new.npu-rep");
    CHECK(config.program.empty());

    CHECK(Parse({"npu-compute", "-i", "old.npu-rep", "-o", "new.npu-rep"}, &config, &error));
    CHECK(config.import_path == "old.npu-rep");
    CHECK(config.export_path == "new.npu-rep");
    return 0;
}

int TestForceOptionsAreRejected()
{
    CliConfig config;
    std::string error;
    CHECK(!Parse({"npu-compute", "--section", "Memory", "-f", "./app"}, &config, &error));
    CHECK(!error.empty());
    CHECK(!Parse(
        {"npu-compute", "--section", "Memory", "-o", "result.npu-rep", "--force-overwrite", "./app"}, &config, &error));
    CHECK(!error.empty());
    return 0;
}

int TestExistingCliBehavior()
{
    CliConfig config;
    std::string error;
    const bool parsed = Parse(
        {"npu-compute", "--section", "PipeUtilization", "--section", "Memory", "--section", "MemoryL0", "--section",
         "MemoryUB", "--section", "L2Cache", "./app", "--export", "app-owned", "--force-overwrite"},
        &config, &error);
    if (!parsed) {
        std::fprintf(stderr, "APP argv parse error: %s\n", error.c_str());
    }
    CHECK(parsed);
    CHECK(
        config.sections == std::vector<std::string>({"PipeUtilization", "Memory", "MemoryL0", "MemoryUB", "L2Cache"}));
    CHECK(!config.export_path.has_value());
    CHECK(config.program_arguments == std::vector<std::string>({"--export", "app-owned", "--force-overwrite"}));
    return 0;
}

int TestHelpTakesPriority()
{
    const std::vector<std::vector<std::string>> help_arguments = {
        {"npu-compute", "-h"},
        {"npu-compute", "--help"},
        {"npu-compute", "-h", "--help"},
        {"npu-compute", "--section", "A", "--help", "--export", "result.npu-rep"},
        {"npu-compute", "--bad-option", "--help"},
        {"npu-compute", "--section", "--help"},
    };
    for (const auto& arguments : help_arguments) {
        CliConfig config;
        std::string error;
        CHECK(Parse(arguments, &config, &error));
        CHECK(error.empty());
        CHECK(config.show_help);
        CHECK(config.sections.empty());
        CHECK(!config.export_path.has_value());
        CHECK(config.program.empty());
        CHECK(config.program_arguments.empty());
    }
    return 0;
}

int TestHelpMatchingAndProgramBoundary()
{
    CliConfig config;
    std::string error;
    CHECK(!Parse({"npu-compute", "--help=value"}, &config, &error));
    CHECK(!error.empty());

    error.clear();
    CHECK(!Parse({"npu-compute", "-hh"}, &config, &error));
    CHECK(!error.empty());

    error.clear();
    CHECK(Parse({"npu-compute", "--section", "Memory", "./app", "--help", "-h"}, &config, &error));
    CHECK(error.empty());
    CHECK(!config.show_help);
    CHECK(config.program == "./app");
    CHECK(config.program_arguments == std::vector<std::string>({"--help", "-h"}));
    return 0;
}

int TestHelpText()
{
    FILE* stream = std::tmpfile();
    CHECK(stream != nullptr);
    PrintUsage(stream, "npu-compute");
    CHECK(std::fflush(stream) == 0);
    const std::string usage = ReadStream(stream);
    CHECK(std::fclose(stream) == 0);
    CHECK(usage.find("force-overwrite") == std::string::npos);
    CHECK(usage.find("-o, --export") != std::string::npos);
    CHECK(usage.find("-i, --import") != std::string::npos);
    return 0;
}

} // namespace

int main()
{
    if (TestBusinessExitCodes() != 0 || TestCollectionExport() != 0 || TestImportExportParsing() != 0 ||
        TestForceOptionsAreRejected() != 0 || TestExistingCliBehavior() != 0 || TestHelpTakesPriority() != 0 ||
        TestHelpMatchingAndProgramBoundary() != 0 || TestHelpText() != 0) {
        return 1;
    }
    return 0;
}
