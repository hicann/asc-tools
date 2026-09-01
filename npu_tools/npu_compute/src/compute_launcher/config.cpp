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

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace npu_compute::compute_launcher {
namespace {

constexpr std::array<const char*, 5> kSupportedSections = {
    "PipeUtilization", "Memory", "MemoryL0", "MemoryUB", "L2Cache",
};

void AddError(const std::string& message, std::vector<std::string>* errors) { errors->push_back(message); }

bool IsSupportedSection(const std::string& section)
{
    return std::find(kSupportedSections.begin(), kSupportedSections.end(), section) != kSupportedSections.end();
}

bool IsHelpOption(const std::string& argument) { return argument == "-h" || argument == "--help"; }

bool MatchValueOption(
    const std::string& argument, const std::string& long_option, char short_option, bool* inline_value,
    std::string* value)
{
    if (argument == long_option || (short_option != '\0' && argument == std::string("-") + short_option)) {
        *inline_value = false;
        value->clear();
        return true;
    }
    const std::string long_prefix = long_option + "=";
    if (argument.rfind(long_prefix, 0) == 0) {
        *inline_value = true;
        *value = argument.substr(long_prefix.size());
        return true;
    }
    if (short_option != '\0') {
        const std::string short_prefix = std::string("-") + short_option;
        if (argument.rfind(short_prefix, 0) == 0 && argument.size() > short_prefix.size()) {
            *inline_value = true;
            *value = argument.substr(short_prefix.size());
            return true;
        }
    }
    return false;
}

bool ReadOptionValue(
    const std::string& option, bool inline_value, std::string* value, int argc, char** argv, int* index,
    std::vector<std::string>* errors)
{
    if (inline_value) {
        return true;
    }
    if (*index + 1 >= argc || IsHelpOption(argv[*index + 1] == nullptr ? "" : argv[*index + 1])) {
        AddError(option + " requires a value", errors);
        return false;
    }
    *value = argv[++(*index)] == nullptr ? "" : argv[*index];
    return true;
}

void AddSection(const std::string& section, CliConfig* config, std::vector<std::string>* errors)
{
    if (section.empty()) {
        AddError("--section requires a value", errors);
    } else if (!IsSupportedSection(section)) {
        AddError("unknown section: " + section, errors);
    } else if (std::find(config->sections.begin(), config->sections.end(), section) == config->sections.end()) {
        config->sections.push_back(section);
    }
}

void ValidateCombinations(const CliConfig& config, std::vector<std::string>* errors)
{
    if (config.show_help || !errors->empty()) {
        return;
    }
    if (config.list_sections) {
        if (config.replay_mode_specified || config.import_path.has_value() || config.export_path.has_value() ||
            !config.sections.empty() || !config.program.empty()) {
            AddError("--list-sections cannot be combined with other options or a program", errors);
        }
        return;
    }
    if (config.import_path.has_value()) {
        if (config.replay_mode_specified || !config.sections.empty() || !config.program.empty()) {
            AddError("--import can only be combined with --export", errors);
        }
        return;
    }
    if (config.export_path.has_value() && config.sections.empty() && config.program.empty()) {
        AddError("--export requires --import or a collection command", errors);
        return;
    }
    if (config.sections.empty()) {
        if (config.program.empty()) {
            AddError("at least one --section is required for collection", errors);
        } else {
            AddError("missing required --section option before program '" + config.program + "'", errors);
        }
    }
    if (config.program.empty()) {
        AddError("program is required for collection", errors);
    }
}

} // namespace

bool ParseCli(int argc, char** argv, CliConfig* config, std::vector<std::string>* errors)
{
    if (errors == nullptr) {
        return false;
    }
    errors->clear();
    if (config == nullptr) {
        AddError("internal error: config is null", errors);
        return false;
    }
    *config = CliConfig{};

    bool list_sections_specified = false;
    bool import_specified = false;
    bool export_specified = false;
    int index = 1;
    for (; index < argc; ++index) {
        const std::string argument = argv[index] == nullptr ? "" : argv[index];
        if (IsHelpOption(argument)) {
            config->show_help = true;
            continue;
        }
        if (argument == "--list-sections") {
            if (list_sections_specified) {
                AddError("--list-sections may only be specified once", errors);
            } else {
                list_sections_specified = true;
                config->list_sections = true;
            }
            continue;
        }
        bool inline_value = false;
        std::string value;
        if (MatchValueOption(argument, "--section", '\0', &inline_value, &value)) {
            if (ReadOptionValue("--section", inline_value, &value, argc, argv, &index, errors)) {
                AddSection(value, config, errors);
            }
            continue;
        }
        if (MatchValueOption(argument, "--replay-mode", '\0', &inline_value, &value)) {
            if (!ReadOptionValue("--replay-mode", inline_value, &value, argc, argv, &index, errors)) {
                continue;
            }
            if (config->replay_mode_specified) {
                AddError("--replay-mode may only be specified once", errors);
                continue;
            }
            config->replay_mode_specified = true;
            if (value != "kernel") {
                AddError("--replay-mode currently only accepts kernel", errors);
            }
            continue;
        }
        if (MatchValueOption(argument, "--import", 'i', &inline_value, &value)) {
            if (!ReadOptionValue("--import", inline_value, &value, argc, argv, &index, errors)) {
                import_specified = true;
                continue;
            }
            if (import_specified) {
                AddError("--import may only be specified once", errors);
            } else if (value.empty()) {
                AddError("--import requires a non-empty path", errors);
            } else {
                config->import_path = value;
            }
            import_specified = true;
            continue;
        }
        if (MatchValueOption(argument, "--export", 'o', &inline_value, &value)) {
            if (!ReadOptionValue("--export", inline_value, &value, argc, argv, &index, errors)) {
                export_specified = true;
                continue;
            }
            if (export_specified) {
                AddError("--export may only be specified once", errors);
            } else if (value.empty()) {
                AddError("--export requires a non-empty path", errors);
            } else {
                config->export_path = value;
            }
            export_specified = true;
            continue;
        }
        if (argument == "--") {
            AddError("-- is not supported; place the program directly after tool options", errors);
            continue;
        }
        if (argument.size() > 1 && argument[0] == '-') {
            AddError("unknown option: " + argument, errors);
            continue;
        }

        config->program = argument;
        ++index;
        for (; index < argc; ++index) {
            config->program_arguments.emplace_back(argv[index] == nullptr ? "" : argv[index]);
        }
        break;
    }

    ValidateCombinations(*config, errors);
    return errors->empty();
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
