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

#include <getopt.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <string>

namespace npu_compute::compute_launcher {
namespace {

constexpr int kOptionSection = 1000;
constexpr int kOptionListSections = 1001;
constexpr int kOptionReplayMode = 1002;

constexpr std::array<const char*, 5> kSupportedSections = {
    "PipeUtilization", "Memory", "MemoryL0", "MemoryUB", "L2Cache",
};

constexpr option kLongOptions[] = {
    {"help", no_argument, nullptr, 'h'},
    {"section", required_argument, nullptr, kOptionSection},
    {"list-sections", no_argument, nullptr, kOptionListSections},
    {"replay-mode", required_argument, nullptr, kOptionReplayMode},
    {"import", required_argument, nullptr, 'i'},
    {"export", required_argument, nullptr, 'o'},
    {nullptr, 0, nullptr, 0},
};

bool Fail(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool IsSupportedSection(const std::string& section)
{
    return std::find(kSupportedSections.begin(), kSupportedSections.end(), section) != kSupportedSections.end();
}

bool IsExactLongOption(const char* argument)
{
    if (argument == nullptr || argument[0] != '-' || argument[1] != '-' || argument[2] == '\0') {
        return true;
    }
    if (std::strcmp(argument, "--") == 0) {
        return true;
    }

    const char* name = argument + 2;
    const char* separator = std::strchr(name, '=');
    const std::size_t name_length =
        separator == nullptr ? std::strlen(name) : static_cast<std::size_t>(separator - name);
    for (const option& candidate : kLongOptions) {
        if (candidate.name == nullptr) {
            break;
        }
        if (std::strlen(candidate.name) == name_length && std::strncmp(candidate.name, name, name_length) == 0) {
            return true;
        }
    }
    return false;
}

bool IsHelpOption(const char* argument)
{
    return argument != nullptr && (std::strcmp(argument, "-h") == 0 || std::strcmp(argument, "--help") == 0);
}

bool RequiresSeparateValue(const char* argument)
{
    if (argument == nullptr) {
        return false;
    }
    return std::strcmp(argument, "--section") == 0 || std::strcmp(argument, "--replay-mode") == 0 ||
           std::strcmp(argument, "--import") == 0 || std::strcmp(argument, "--export") == 0 ||
           std::strcmp(argument, "-i") == 0 || std::strcmp(argument, "-o") == 0;
}

bool HasHelpBeforeProgram(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        const char* argument = argv[index];
        if (IsHelpOption(argument)) {
            return true;
        }
        if (argument == nullptr || argument[0] != '-' || argument[1] == '\0' || std::strcmp(argument, "--") == 0) {
            return false;
        }
        if (RequiresSeparateValue(argument) && index + 1 < argc) {
            if (IsHelpOption(argv[index + 1])) {
                return true;
            }
            ++index;
        }
    }
    return false;
}

bool AddSection(const char* value, CliConfig* config, std::string* error)
{
    const std::string section = value == nullptr ? "" : value;
    if (!IsSupportedSection(section)) {
        return Fail("unknown section: " + section, error);
    }
    if (std::find(config->sections.begin(), config->sections.end(), section) == config->sections.end()) {
        config->sections.push_back(section);
    }
    return true;
}

bool HasProgram(const CliConfig& config) { return !config.program.empty(); }

bool ValidateCombinations(const CliConfig& config, std::string* error)
{
    const bool has_replay = config.replay_mode_specified;
    const bool has_import = config.import_path.has_value();
    const bool has_export = config.export_path.has_value();
    const bool has_sections = !config.sections.empty();
    const bool has_program = HasProgram(config);

    if (config.show_help) {
        if (config.list_sections || has_replay || has_import || has_export || has_sections || has_program) {
            return Fail("--help cannot be combined with other options or a program", error);
        }
        return true;
    }

    if (config.list_sections) {
        if (has_replay || has_import || has_export || has_sections || has_program) {
            return Fail("--list-sections cannot be combined with other options or a program", error);
        }
        return true;
    }

    if (has_import) {
        if (has_replay || has_sections || has_program) {
            return Fail("--import can only be combined with --export", error);
        }
        return true;
    }

    if (has_export && !has_sections && !has_program) {
        return Fail("--export requires --import or a collection command", error);
    }
    if (!has_sections) {
        return Fail("at least one --section is required for collection", error);
    }
    if (!has_program) {
        return Fail("program is required for collection", error);
    }
    return true;
}

} // namespace

bool ParseCli(int argc, char** argv, CliConfig* config, std::string* error)
{
    if (config == nullptr) {
        return Fail("internal error: config is null", error);
    }
    *config = CliConfig{};

    if (HasHelpBeforeProgram(argc, argv)) {
        config->show_help = true;
        return true;
    }

    bool help_specified = false;
    bool list_sections_specified = false;
    opterr = 0;
    optind = 1;

    while (true) {
        if (optind < argc && !IsExactLongOption(argv[optind])) {
            return Fail("unknown option: " + std::string(argv[optind]), error);
        }
        const int parsed = getopt_long(argc, argv, "+hi:o:", kLongOptions, nullptr);
        if (parsed == -1) {
            break;
        }
        switch (parsed) {
            case 'h':
                if (help_specified) {
                    return Fail("--help may only be specified once", error);
                }
                help_specified = true;
                config->show_help = true;
                break;
            case 'i':
                if (config->import_path.has_value()) {
                    return Fail("--import may only be specified once", error);
                }
                if (optarg == nullptr || optarg[0] == '\0') {
                    return Fail("--import requires a non-empty path", error);
                }
                config->import_path = optarg;
                break;
            case 'o':
                if (config->export_path.has_value()) {
                    return Fail("--export may only be specified once", error);
                }
                if (optarg == nullptr || optarg[0] == '\0') {
                    return Fail("--export requires a non-empty path", error);
                }
                config->export_path = optarg;
                break;
            case kOptionSection:
                if (!AddSection(optarg, config, error)) {
                    return false;
                }
                break;
            case kOptionListSections:
                if (list_sections_specified) {
                    return Fail("--list-sections may only be specified once", error);
                }
                list_sections_specified = true;
                config->list_sections = true;
                break;
            case kOptionReplayMode:
                if (config->replay_mode_specified) {
                    return Fail("--replay-mode may only be specified once", error);
                }
                config->replay_mode_specified = true;
                if (optarg == nullptr || std::strcmp(optarg, "kernel") != 0) {
                    return Fail("--replay-mode currently only accepts kernel", error);
                }
                config->replay_mode = ReplayMode::Kernel;
                break;
            case '?':
            default: {
                const char* argument = optind > 0 && optind <= argc ? argv[optind - 1] : nullptr;
                return Fail(
                    "unknown option or missing value: " + std::string(argument == nullptr ? "" : argument), error);
            }
        }
    }

    if (optind > 1 && std::strcmp(argv[optind - 1], "--") == 0) {
        return Fail("-- is not supported; place the program directly after tool options", error);
    }

    if (optind < argc) {
        config->program = argv[optind++];
        while (optind < argc) {
            config->program_arguments.emplace_back(argv[optind++]);
        }
    }

    return ValidateCombinations(*config, error);
}

const char* ReplayModeName(ReplayMode mode)
{
    switch (mode) {
        case ReplayMode::Kernel:
            return "kernel";
    }
    return "kernel";
}

void PrintUsage(FILE* stream, const char* program)
{
    if (stream == nullptr) {
        return;
    }
    std::fprintf(
        stream,
        "Usage: %s [options] [program] [program-arguments]\n"
        "Options:\n"
        "  -h, --help                 Display this help and exit\n"
        "      --section <id>         Add a section; may be repeated\n"
        "      --list-sections        List supported section IDs and exit\n"
        "      --replay-mode <mode>   Replay mode; currently kernel only\n"
        "  -i, --import <repo>        Read an existing repo\n"
        "  -o, --export <repo>        Write a collection or imported repo\n",
        program == nullptr ? "npu-compute" : program);
}

void PrintSections(FILE* stream)
{
    if (stream == nullptr) {
        return;
    }
    for (const char* section : kSupportedSections) {
        std::fprintf(stream, "%s\n", section);
    }
}

} // namespace npu_compute::compute_launcher
