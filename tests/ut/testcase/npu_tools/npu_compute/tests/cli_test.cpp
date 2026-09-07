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

bool Parse(const std::vector<std::string>& arguments, CliConfig* config, std::vector<std::string>* errors)
{
    std::vector<std::string> storage = arguments;
    std::vector<char*> argv;
    argv.reserve(storage.size());
    for (std::string& argument : storage) {
        argv.push_back(argument.data());
    }
    return ParseCli(static_cast<int>(argv.size()), argv.data(), config, errors);
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
    std::vector<std::string> errors;
    CHECK(Parse(
        {"npu-compute", "--section", "PipeUtilization", "-o", "result.npu-rep", "./app", "--app-value"}, &config,
        &errors));
    CHECK(errors.empty());
    CHECK(config.export_path == "result.npu-rep");
    CHECK(config.program == "./app");
    CHECK(config.program_arguments == std::vector<std::string>{"--app-value"});

    CHECK(Parse({"npu-compute", "--section", "Memory", "--export", "reports", "./app"}, &config, &errors));
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
    std::vector<std::string> errors;
    CHECK(Parse({"npu-compute", "--import", "old.npu-rep", "--export", "new.npu-rep"}, &config, &errors));
    CHECK(config.import_path == "old.npu-rep");
    CHECK(config.export_path == "new.npu-rep");
    CHECK(config.program.empty());

    CHECK(Parse({"npu-compute", "-i", "old.npu-rep", "-o", "new.npu-rep"}, &config, &errors));
    CHECK(config.import_path == "old.npu-rep");
    CHECK(config.export_path == "new.npu-rep");

    CHECK(Parse({"npu-compute", "-iold.npu-rep", "-onew.npu-rep"}, &config, &errors));
    CHECK(config.import_path == "old.npu-rep");
    CHECK(config.export_path == "new.npu-rep");
    return 0;
}

int TestInlineLongOptionValues()
{
    CliConfig config;
    std::vector<std::string> errors;
    CHECK(Parse(
        {"npu-compute", "--section=Memory", "--replay-mode=kernel", "--export=result.npu-rep", "./app"}, &config,
        &errors));
    CHECK(errors.empty());
    CHECK(config.sections == std::vector<std::string>({"Memory"}));
    CHECK(config.replay_mode_specified);
    CHECK(config.export_path == "result.npu-rep");
    CHECK(config.program == "./app");
    return 0;
}

int TestForceOptionsAreRejected()
{
    CliConfig config;
    std::vector<std::string> errors;
    CHECK(!Parse({"npu-compute", "--section", "Memory", "-f", "./app"}, &config, &errors));
    CHECK(!errors.empty());
    errors.clear();
    CHECK(!Parse(
        {"npu-compute", "--section", "Memory", "-o", "result.npu-rep", "--force-overwrite", "./app"}, &config,
        &errors));
    CHECK(!errors.empty());
    return 0;
}

int TestExistingCliBehavior()
{
    CliConfig config;
    std::vector<std::string> errors;
    const bool parsed = Parse(
        {"npu-compute", "--section", "PipeUtilization", "--section", "Memory", "--section", "MemoryL0", "--section",
         "MemoryUB", "--section", "L2Cache", "./app", "--export", "app-owned", "--force-overwrite"},
        &config, &errors);
    if (!parsed) {
        for (const std::string& error : errors) {
            std::fprintf(stderr, "APP argv parse error: %s\n", error.c_str());
        }
    }
    CHECK(parsed);
    CHECK(
        config.sections == std::vector<std::string>({"PipeUtilization", "Memory", "MemoryL0", "MemoryUB", "L2Cache"}));
    CHECK(!config.export_path.has_value());
    CHECK(config.program_arguments == std::vector<std::string>({"--export", "app-owned", "--force-overwrite"}));
    return 0;
}

int TestHelpWithoutErrors()
{
    const std::vector<std::vector<std::string>> help_arguments = {
        {"npu-compute", "-h"},
        {"npu-compute", "--help"},
        {"npu-compute", "-h", "--help"},
        {"npu-compute", "--section", "Memory", "--help", "--export", "result.npu-rep"},
    };
    for (const auto& arguments : help_arguments) {
        CliConfig config;
        std::vector<std::string> errors;
        CHECK(Parse(arguments, &config, &errors));
        CHECK(errors.empty());
        CHECK(config.show_help);
        CHECK(config.program.empty());
        CHECK(config.program_arguments.empty());
    }
    return 0;
}

int TestHelpReportsAllOptionErrors()
{
    CliConfig config;
    std::vector<std::string> errors;

    CHECK(!Parse({"npu-compute", "--bad-option", "--help"}, &config, &errors));
    CHECK(errors == std::vector<std::string>({"unknown option: --bad-option"}));
    CHECK(config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "--section", "Invalid", "--help"}, &config, &errors));
    CHECK(errors == std::vector<std::string>({"unknown section: Invalid"}));
    CHECK(config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "--section", "--help"}, &config, &errors));
    CHECK(errors == std::vector<std::string>({"--section requires a value"}));
    CHECK(config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "--bad-one", "--bad-two", "--help"}, &config, &errors));
    CHECK(errors == std::vector<std::string>({"unknown option: --bad-one", "unknown option: --bad-two"}));
    CHECK(config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "--bad-one", "--bad-two"}, &config, &errors));
    CHECK(errors == std::vector<std::string>({"unknown option: --bad-one", "unknown option: --bad-two"}));
    CHECK(!config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "hh", "-h", "/path/to/run.sh"}, &config, &errors));
    CHECK(errors == std::vector<std::string>({"missing required --section option before program 'hh'"}));
    CHECK(config.program == "hh");
    CHECK(config.program_arguments == std::vector<std::string>({"-h", "/path/to/run.sh"}));

    errors.clear();
    CHECK(!Parse({"npu-compute", "--section", "Invalid", "--replay-mode", "invalid", "--help"}, &config, &errors));
    CHECK(
        errors ==
        std::vector<std::string>({"unknown section: Invalid", "--replay-mode currently only accepts kernel"}));
    CHECK(config.show_help);
    return 0;
}

int TestHelpAcceptsListSectionsCombination()
{
    const std::vector<std::vector<std::string>> arguments = {
        {"npu-compute", "--list-sections", "--help"},
        {"npu-compute", "--help", "--list-sections"},
    };
    for (const auto& argument : arguments) {
        CliConfig config;
        std::vector<std::string> errors;
        CHECK(Parse(argument, &config, &errors));
        CHECK(errors.empty());
        CHECK(config.show_help);
        CHECK(config.list_sections);
    }
    return 0;
}

int TestHelpMatchingAndProgramBoundary()
{
    CliConfig config;
    std::vector<std::string> errors;
    CHECK(!Parse({"npu-compute", "--help=value"}, &config, &errors));
    CHECK(!errors.empty());

    errors.clear();
    CHECK(!Parse({"npu-compute", "-hh"}, &config, &errors));
    CHECK(!errors.empty());

    errors.clear();
    CHECK(Parse({"npu-compute", "--section", "Memory", "./app", "--bad-option", "--help", "-h"}, &config, &errors));
    CHECK(errors.empty());
    CHECK(!config.show_help);
    CHECK(config.program == "./app");
    CHECK(config.program_arguments == std::vector<std::string>({"--bad-option", "--help", "-h"}));
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
        TestInlineLongOptionValues() != 0 || TestForceOptionsAreRejected() != 0 || TestExistingCliBehavior() != 0 ||
        TestHelpWithoutErrors() != 0 || TestHelpReportsAllOptionErrors() != 0 ||
        TestHelpAcceptsListSectionsCombination() != 0 || TestHelpMatchingAndProgramBoundary() != 0 ||
        TestHelpText() != 0) {
        return 1;
    }
    return 0;
}
