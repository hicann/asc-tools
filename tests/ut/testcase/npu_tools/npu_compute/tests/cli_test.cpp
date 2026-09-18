/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <gtest/gtest.h>

#include "config/config.h"
#include "launch/launcher.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int Check(bool condition, const char* expression, int line)
{
    if (condition) {
        return 0;
    }
    ADD_FAILURE_AT(__FILE__, line) << expression;
    return 1;
}

#define CHECK(expression)                               \
    do {                                                \
        if (Check((expression), #expression, __LINE__)) \
            return 1;                                   \
    } while (false)

using npucompute::cli::CliConfig;
using npucompute::cli::ParseCli;
using npucompute::cli::PrintUsage;

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
    CHECK(Parse({"npu-compute", "--section", "Memory", "./app"}, &config, &errors));
    CHECK(!config.export_path);
    CHECK(!config.import_path);
    CHECK(!Parse({"npu-compute", "--section", "Memory", "--export", "", "./app"}, &config, &errors));
    CHECK(errors == std::vector<std::string>({"--export requires a non-empty output path."}));
    CHECK(!config.export_path);
    return 0;
}

int TestPipelineOption()
{
    CliConfig config;
    std::vector<std::string> errors;
    CHECK(Parse({"npu-compute", "--section", "Pipeline", "./app"}, &config, &errors));
    CHECK(errors.empty());
    CHECK(config.collect_pipeline);
    CHECK(config.sections == std::vector<std::string>{"Pipeline"});
    CHECK(Parse({"npu-compute", "--section=Pipeline", "--section", "Memory", "./app"}, &config, &errors));
    CHECK(config.collect_pipeline);
    CHECK(Parse(
        {"npu-compute", "--section", "Pipeline", "--section", "ArithmeticUtilization", "--section",
         "ResourceConflictRatio", "./app"},
        &config, &errors));
    CHECK(config.collect_pipeline);
    CHECK(config.sections == std::vector<std::string>({"Pipeline", "ArithmeticUtilization", "ResourceConflictRatio"}));
    CHECK(!Parse({"npu-compute", "--pipeline", "--section", "Memory", "./app"}, &config, &errors));
    CHECK(errors == std::vector<std::string>({"unknown option '--pipeline'. Use --help to see supported options."}));
    return 0;
}

int TestBusinessExitCodes()
{
    CHECK(npucompute::cli::kUsageErrorExitCode == 2);
    CHECK(npucompute::cli::kCollectionErrorExitCode == 3);
    CHECK(npucompute::cli::kReportErrorExitCode == 4);
    CHECK(npucompute::cli::kInternalErrorExitCode == 5);
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
    CHECK(Parse({"npu-compute", "--import", "old.npu-rep"}, &config, &errors));
    CHECK(config.import_path == "old.npu-rep");
    CHECK(!config.export_path);
    CHECK(!Parse({"npu-compute", "--import", ""}, &config, &errors));
    CHECK(errors == std::vector<std::string>({"--import requires a non-empty input report file path."}));
    CHECK(!config.import_path);
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
        {"npu-compute",
         "--section",
         "PipeUtilization",
         "--section",
         "Memory",
         "--section",
         "MemoryL0",
         "--section",
         "MemoryUB",
         "--section",
         "L2Cache",
         "--section",
         "ArithmeticUtilization",
         "--section",
         "ResourceConflictRatio",
         "--section",
         "ArithmeticUtilization",
         "./app",
         "--export",
         "app-owned",
         "--force-overwrite"},
        &config, &errors);
    if (!parsed) {
        for (const std::string& error : errors) {
            std::fprintf(stderr, "APP argv parse error: %s\n", error.c_str());
        }
    }
    CHECK(parsed);
    CHECK(
        config.sections == std::vector<std::string>(
                               {"PipeUtilization", "Memory", "MemoryL0", "MemoryUB", "L2Cache", "ArithmeticUtilization",
                                "ResourceConflictRatio"}));
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
    CHECK(errors == std::vector<std::string>({"unknown option '--bad-option'. Use --help to see supported options."}));
    CHECK(config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "--section", "Invalid", "--help"}, &config, &errors));
    CHECK(
        errors == std::vector<std::string>({"unsupported section name 'Invalid'. Names are case-sensitive; use "
                                            "--list-sections to see supported names."}));
    CHECK(config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "--section", "--help"}, &config, &errors));
    CHECK(
        errors ==
        std::vector<std::string>({"--section requires a section name. Use --list-sections to see supported names."}));
    CHECK(config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "--bad-one", "--bad-two", "--help"}, &config, &errors));
    CHECK(
        errors == std::vector<std::string>(
                      {"unknown option '--bad-one'. Use --help to see supported options.",
                       "unknown option '--bad-two'. Use --help to see supported options."}));
    CHECK(config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "--bad-one", "--bad-two"}, &config, &errors));
    CHECK(
        errors == std::vector<std::string>(
                      {"unknown option '--bad-one'. Use --help to see supported options.",
                       "unknown option '--bad-two'. Use --help to see supported options."}));
    CHECK(!config.show_help);

    errors.clear();
    CHECK(!Parse({"npu-compute", "hh", "-h", "/path/to/run.sh"}, &config, &errors));
    CHECK(
        errors ==
        std::vector<std::string>({"collection requires at least one --set or --section before program 'hh'."}));
    CHECK(config.program == "hh");
    CHECK(config.program_arguments == std::vector<std::string>({"-h", "/path/to/run.sh"}));

    errors.clear();
    CHECK(!Parse({"npu-compute", "--section", "Invalid", "--replay-mode", "invalid", "--help"}, &config, &errors));
    CHECK(
        errors == std::vector<std::string>(
                      {"unsupported section name 'Invalid'. Names are case-sensitive; use --list-sections to see "
                       "supported names.",
                       "unsupported replay mode 'invalid'. Supported value: kernel."}));
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

int TestMissingValueAndDuplicateState()
{
    CliConfig config;
    std::vector<std::string> errors;
    for (const std::string option : {"--import", "--export"}) {
        CHECK(!Parse({"npu-compute", option, "--help", option, "file.npu-rep"}, &config, &errors));
        CHECK(errors.size() == 2);
        CHECK(errors[1] == option + " may only be specified once");
        CHECK(!config.import_path && !config.export_path);
    }
    CHECK(!Parse({"npu-compute", "--replay-mode", "--help"}, &config, &errors));
    CHECK(config.replay_mode_specified);
    CHECK(errors == std::vector<std::string>({"--replay-mode requires a mode. Supported value: kernel."}));
    CHECK(!Parse({"npu-compute", "--replay-mode", "--help", "--replay-mode", "kernel"}, &config, &errors));
    CHECK(config.replay_mode_specified);
    CHECK(errors.size() == 2);
    CHECK(errors[1] == "--replay-mode may only be specified once");
    CHECK(!Parse({"npu-compute", "--replay-mode=", "--replay-mode", "kernel"}, &config, &errors));
    CHECK(errors.size() == 2);
    CHECK(errors[1] == "--replay-mode may only be specified once");
    return 0;
}

int TestNullArgumentsAndStateReset()
{
    CliConfig config;
    std::vector<std::string> errors;
    char name[] = "npu-compute";
    char section[] = "--section";
    char memory[] = "Memory";
    char help[] = "--help";
    char* missing_value[] = {name, section, nullptr};
    CHECK(!ParseCli(3, missing_value, &config, &errors));
    CHECK(
        errors ==
        std::vector<std::string>({"--section requires a section name. Use --list-sections to see supported names."}));
    char* empty_program[] = {name, section, memory, nullptr, help};
    CHECK(!ParseCli(5, empty_program, &config, &errors));
    CHECK(!config.show_help);
    CHECK(config.program.empty());
    CHECK(config.program_arguments == std::vector<std::string>({"--help"}));
    CHECK(Parse(
        {"npu-compute", "--section", "Memory", "--section", "L2Cache", "--section", "Memory", "app"}, &config,
        &errors));
    CHECK(config.sections == std::vector<std::string>({"Memory", "L2Cache"}));
    CHECK(Parse({"npu-compute", "--help"}, &config, &errors));
    CHECK(config.show_help && errors.empty());
    CHECK(config.sections.empty() && config.program.empty() && config.program_arguments.empty());
    CHECK(!config.import_path && !config.export_path && !config.replay_mode_specified && !config.list_sections);
    return 0;
}

int TestSets()
{
    CliConfig config;
    std::vector<std::string> errors;
    const std::vector<std::string> basic = {"Pipeline", "PipeUtilization",      "Memory", "MemoryL0", "MemoryUB",
                                            "L2Cache",  "ArithmeticUtilization"};
    auto full = basic;
    full.push_back("ResourceConflictRatio");
    CHECK(Parse({"npu-compute", "--set", "basic", "./app"}, &config, &errors));
    CHECK(config.sections == basic && config.collect_pipeline);
    CHECK(Parse({"npu-compute", "./app"}, &config, &errors));
    CHECK(config.sets == std::vector<std::string>({"basic"}));
    CHECK(config.sections == basic && config.collect_pipeline);
    CHECK(Parse({"npu-compute", "--set=full", "--set", "basic", "--section", "Memory", "./app"}, &config, &errors));
    CHECK(config.sections == full);
    auto reordered = basic;
    reordered.erase(reordered.begin() + 2);
    reordered.insert(reordered.begin(), "Memory");
    CHECK(Parse({"npu-compute", "--section", "Memory", "--set", "basic", "./app"}, &config, &errors));
    CHECK(config.sections == reordered);
    CHECK(Parse(
        {"npu-compute", "--set", "basic", "--section", "ResourceConflictRatio", "--replay-mode=kernel", "-o",
         "x.npu-rep", "./app", "--set", "full"},
        &config, &errors));
    CHECK(config.sections == full);
    CHECK(config.program_arguments == std::vector<std::string>({"--set", "full"}));
    CHECK(Parse({"npu-compute", "--list-sets"}, &config, &errors));
    for (const auto& args : std::vector<std::vector<std::string>>{
             {"--set"},
             {"--set="},
             {"--set", ""},
             {"--set", "Basic"},
             {"--set", "basic,full"},
             {"--set", "basic"},
             {"--set", "basic", "--import", "x.npu-rep"},
             {"--list-sets", "--list-sets"},
             {"--list-sets=basic"},
             {"--list-sets", "--list-sections"},
             {"--list-sets", "--set", "basic"},
             {"--list-sets", "--section", "Memory"},
             {"--list-sets", "--replay-mode", "kernel"},
             {"--list-sets", "-o", "x.npu-rep"},
             {"--list-sets", "--import", "x.npu-rep"},
             {"--list-sets", "./app"}}) {
        auto command = args;
        command.insert(command.begin(), "npu-compute");
        CHECK(!Parse(command, &config, &errors));
        CHECK(!errors.empty());
    }
    CHECK(Parse({"npu-compute", "-h", "--set", "basic"}, &config, &errors));
    CHECK(Parse({"npu-compute", "--list-sets", "--help"}, &config, &errors));
    CHECK(!Parse({"npu-compute", "--help", "--set", "invalid"}, &config, &errors));
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

static int RunSuiteMain()
{
    if (TestSets() != 0 || TestBusinessExitCodes() != 0 || TestCollectionExport() != 0 || TestPipelineOption() != 0 ||
        TestImportExportParsing() != 0 || TestInlineLongOptionValues() != 0 || TestForceOptionsAreRejected() != 0 ||
        TestExistingCliBehavior() != 0 || TestHelpWithoutErrors() != 0 || TestHelpReportsAllOptionErrors() != 0 ||
        TestHelpAcceptsListSectionsCombination() != 0 || TestHelpMatchingAndProgramBoundary() != 0 ||
        TestMissingValueAndDuplicateState() != 0 || TestNullArgumentsAndStateReset() != 0 || TestHelpText() != 0) {
        return 1;
    }
    return 0;
}

TEST(NpuComputeCli, Main) { ASSERT_EQ(RunSuiteMain(), 0) << "NpuComputeCli reported failure"; }
