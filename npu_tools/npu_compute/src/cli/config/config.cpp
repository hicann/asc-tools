/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "config/config.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace npucompute::cli {
namespace {

constexpr std::array<const char*, 6> kSupportedSections = {
    "PipeUtilization", "Memory", "MemoryL0", "MemoryUB", "L2Cache", "Pipeline",
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

void AddSection(
    const std::string& section, const char* missing_message, CliConfig* config, std::vector<std::string>* errors)
{
    if (section.empty()) {
        AddError(missing_message, errors);
    } else if (!IsSupportedSection(section)) {
        const std::string hint = section.find(',') != std::string::npos ?
                                     "Specify each group separately, for example: --section Memory --section L2Cache." :
                                     "Names are case-sensitive; use --list-sections to see supported names.";
        AddError("unsupported section name '" + section + "'. " + hint, errors);
    } else if (std::find(config->sections.begin(), config->sections.end(), section) == config->sections.end()) {
        config->sections.push_back(section);
        if (section == "Pipeline") {
            config->collect_pipeline = true;
        }
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
            AddError("use --list-sections as a standalone command.", errors);
        }
        return;
    }
    if (config.import_path.has_value()) {
        if (config.replay_mode_specified || !config.sections.empty() || !config.program.empty()) {
            AddError("--import cannot be combined with --section, --replay-mode or a target program.", errors);
        }
        return;
    }
    if (config.export_path.has_value() && config.sections.empty() && config.program.empty()) {
        AddError("--export requires a collection command or --import.", errors);
        return;
    }
    if (config.sections.empty()) {
        if (config.program.empty()) {
            AddError("collection requires at least one --section before the program.", errors);
        } else {
            AddError("collection requires at least one --section before program '" + config.program + "'.", errors);
        }
    }
    if (config.program.empty()) {
        AddError("collection requires a target program after the tool options.", errors);
    }
}

enum class OptionArity { Flag, RequiredValue };
enum class ValueState { None, Missing, Present };
struct OptionValue {
    ValueState state = ValueState::None;
    std::string text;
};
struct ParseContext {
    CliConfig* config;
    std::vector<std::string>* errors;
    bool list_sections_specified = false;
    bool import_specified = false;
    bool export_specified = false;
};
struct OptionSpec {
    const char* long_name;
    char short_name;
    OptionArity arity;
    const char* missing_message;
    void (*handler)(const OptionSpec&, const OptionValue&, ParseContext&);
};
void HandleHelp(const OptionSpec&, const OptionValue&, ParseContext& context) { context.config->show_help = true; }
void HandleSection(const OptionSpec& spec, const OptionValue& value, ParseContext& context)
{
    if (value.state != ValueState::Missing) {
        AddSection(value.text, spec.missing_message, context.config, context.errors);
    }
}
void HandleListSections(const OptionSpec&, const OptionValue&, ParseContext& context)
{
    if (context.list_sections_specified) {
        AddError("--list-sections may only be specified once", context.errors);
    } else {
        context.list_sections_specified = true;
        context.config->list_sections = true;
    }
}

void HandleReplayMode(const OptionSpec& spec, const OptionValue& value, ParseContext& context)
{
    if (value.state == ValueState::Missing) {
        context.config->replay_mode_specified = true;
        return;
    }
    if (context.config->replay_mode_specified) {
        AddError("--replay-mode may only be specified once", context.errors);
        return;
    }
    context.config->replay_mode_specified = true;
    if (value.text != "kernel") {
        if (value.text.empty()) {
            AddError(spec.missing_message, context.errors);
        } else {
            AddError("unsupported replay mode '" + value.text + "'. Supported value: kernel.", context.errors);
        }
    }
}

void HandleImport(const OptionSpec&, const OptionValue& value, ParseContext& context)
{
    if (value.state == ValueState::Missing) {
        context.import_specified = true;
        return;
    }
    if (context.import_specified) {
        AddError("--import may only be specified once", context.errors);
    } else if (value.text.empty()) {
        AddError("--import requires a non-empty input report file path.", context.errors);
    } else {
        context.config->import_path = value.text;
    }
    context.import_specified = true;
}

void HandleExport(const OptionSpec&, const OptionValue& value, ParseContext& context)
{
    if (value.state == ValueState::Missing) {
        context.export_specified = true;
        return;
    }
    if (context.export_specified) {
        AddError("--export may only be specified once", context.errors);
    } else if (value.text.empty()) {
        AddError("--export requires a non-empty output path.", context.errors);
    } else {
        context.config->export_path = value.text;
    }
    context.export_specified = true;
}

const std::array<OptionSpec, 6> kOptions = {{
    {"--help", 'h', OptionArity::Flag, nullptr, HandleHelp},
    {"--list-sections", '\0', OptionArity::Flag, nullptr, HandleListSections},
    {"--section", '\0', OptionArity::RequiredValue,
     "--section requires a section name. Use --list-sections to see supported names.", HandleSection},
    {"--replay-mode", '\0', OptionArity::RequiredValue, "--replay-mode requires a mode. Supported value: kernel.",
     HandleReplayMode},
    {"--import", 'i', OptionArity::RequiredValue, "--import requires an input report file path.", HandleImport},
    {"--export", 'o', OptionArity::RequiredValue, "--export requires an output path: a report file or directory.",
     HandleExport},
}};
class CliParser {
public:
    CliParser(CliConfig* config, std::vector<std::string>* errors) : context_{config, errors} {}
    void Parse(int argc, char** argv)
    {
        for (int index = 1; index < argc; ++index) {
            const std::string argument = Argument(argv[index]);
            bool inline_value = false;
            OptionValue value;
            const OptionSpec* spec = FindOption(argument, &inline_value, &value.text);
            if (spec != nullptr) {
                if (spec->arity == OptionArity::RequiredValue) {
                    value.state = ValueState::Present;
                    if (!inline_value) {
                        if (index + 1 >= argc || IsHelpOption(Argument(argv[index + 1]))) {
                            value.state = ValueState::Missing;
                            AddError(spec->missing_message, context_.errors);
                        } else {
                            value.text = Argument(argv[++index]);
                        }
                    }
                }
                spec->handler(*spec, value, context_);
                continue;
            }
            if (argument == "--") {
                AddError("-- is not supported; place the program directly after tool options", context_.errors);
                continue;
            }
            if (argument.size() > 1 && argument[0] == '-') {
                AddError("unknown option '" + argument + "'. Use --help to see supported options.", context_.errors);
                continue;
            }
            context_.config->program = argument;
            while (++index < argc) {
                context_.config->program_arguments.emplace_back(Argument(argv[index]));
            }
            break;
        }
    }

private:
    static std::string Argument(const char* value) { return value == nullptr ? "" : value; }
    static const OptionSpec* FindOption(const std::string& argument, bool* inline_value, std::string* value)
    {
        for (const auto& spec : kOptions) {
            if (spec.arity == OptionArity::Flag) {
                if (argument == spec.long_name ||
                    (spec.short_name != '\0' && argument == std::string("-") + spec.short_name)) {
                    return &spec;
                }
            } else if (MatchValueOption(argument, spec.long_name, spec.short_name, inline_value, value)) {
                return &spec;
            }
        }
        return nullptr;
    }
    ParseContext context_;
};

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

    CliParser parser(config, errors);
    parser.Parse(argc, argv);

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
        "\n"
        "Options:\n"
        "  -h, --help                 Show help information.\n"
        "\n"
        "      --list-sections        List supported section names.\n"
        "\n"
        "      --section arg          Select a metric group by name (case-sensitive).\n"
        "                             Use --list-sections to see supported names.\n"
        "                             Required for collection.\n"
        "                             No section is selected by default.\n"
        "                             Specify different groups separately:\n"
        "                               --section Memory --section L2Cache\n"
        "\n"
        "      --replay-mode arg      Kernel replay mode.\n"
        "                             Value: kernel. Default: kernel.\n"
        "\n"
        "  -i, --import arg           Unpack a npu-compute report file (.npu-rep).\n"
        "                             Use alone or with --export; no target program.\n"
        "\n"
        "  -o, --export arg           Specify the output path.\n"
        "                             Collection: a new .npu-rep file or an existing\n"
        "                               directory. The parent directory must exist.\n"
        "                             Import: an existing directory for unpacking.\n"
        "                             Default location: current directory.\n"
        "                             Existing files are not overwritten.\n",
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

} // namespace npucompute::cli
