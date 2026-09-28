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

#include "config/option_registry.h"
#include "config/config.h"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace npucompute::cli;
#define CHECK(expression)                                                               \
    do {                                                                                \
        if (!(expression)) {                                                            \
            std::fprintf(stderr, "line %d: check failed: %s\n", __LINE__, #expression); \
            return 1;                                                                   \
        }                                                                               \
    } while (false)

OptionSpec Flag(std::string name, char alias = '\0')
{
    OptionSpec spec;
    spec.long_name = std::move(name);
    spec.short_name = alias;
    spec.apply = [](const std::string&, CliConfig&, std::vector<std::string>&) {};
    return spec;
}
OptionSpec Value(std::string name, char alias = '\0')
{
    auto spec = Flag(std::move(name), alias);
    spec.arity = OptionArity::RequiredValue;
    spec.missing_message = "missing value";
    return spec;
}
OptionRegistry Options()
{
    OptionRegistryBuilder builder;
    auto help = Flag("--help", 'h');
    help.protect_from_value = true;
    builder.Add(std::move(help));
    builder.Add(Value("--export", 'o'));
    return builder.Build();
}

int TestOccurrenceTracker()
{
    OccurrenceTracker tracker(2);
    CHECK(tracker.Count(0) == 0 && tracker.Count(1) == 0);
    CHECK(tracker.Record(1) == 1);
    CHECK(tracker.Record(1) == 2);
    CHECK(tracker.Record(0) == 1);
    CHECK(tracker.Count(0) == 1 && tracker.Count(1) == 2);
    bool rejected = false;
    try {
        tracker.Record(2);
    } catch (const std::out_of_range&) {
        rejected = true;
    }
    CHECK(rejected);
    return 0;
}

int TestRegisteredHelp()
{
    OptionRegistryBuilder builder;
    auto verbose = Flag("--verbose", 'v');
    verbose.help = "Show 100% of details.\nSecond line.";
    builder.Add(verbose);
    const auto first = builder.Build();
    auto output = Value("--save-to", 's');
    output.help = "Save results.";
    builder.Add(output);
    auto quiet = Flag("--quiet");
    quiet.help = "Suppress output.";
    builder.Add(quiet);
    CHECK(
        first.HelpText() == "  -v, --verbose              Show 100% of details.\n"
                            "                             Second line.\n");
    CHECK(
        builder.Build().HelpText() == "  -v, --verbose              Show 100% of details.\n"
                                      "                             Second line.\n\n"
                                      "  -s, --save-to arg          Save results.\n\n"
                                      "      --quiet                Suppress output.\n");
    CHECK(OptionRegistryBuilder{}.Build().HelpText().empty());
    return 0;
}

int TestParsingRegisteredExtensions()
{
    OptionRegistryBuilder builder;
    auto usage = Flag("--usage", 'u');
    usage.protect_from_value = true;
    usage.apply = [](const std::string&, CliConfig& config, std::vector<std::string>&) { config.show_help = true; };
    builder.Add(usage);
    auto save = Value("--save-to", 's');
    save.repeat = RepeatPolicy::Once;
    save.apply = [](const std::string& value, CliConfig& config, std::vector<std::string>&) {
        config.export_path = value;
    };
    builder.Add(save);
    const auto registry = builder.Build();
    CliConfig config;
    std::vector<std::string> errors;
    auto parse = [&](std::vector<std::string> args) {
        args.insert(args.begin(), "test-cli");
        std::vector<char*> pointers;
        for (auto& arg : args) {
            pointers.push_back(arg.data());
        }
        registry.Parse(pointers.size(), pointers.data(), config, errors);
    };
    parse({"--usage", "-sresult", "--", "--app", "--usage", "--", "", "two words"});
    CHECK(errors.empty() && config.show_help && config.export_path == "result");
    CHECK(config.program == "--app");
    CHECK(config.program_arguments == std::vector<std::string>({"--usage", "--", "", "two words"}));
    parse({"--save-to=", "./app", "one", "two"});
    CHECK(errors.empty() && !config.show_help && config.export_path == "");
    CHECK(config.program == "./app" && config.program_arguments == std::vector<std::string>({"one", "two"}));
    parse({"-s", "-u", "--save-to", "result"});
    CHECK(errors == std::vector<std::string>({"missing value", "--save-to may only be specified once"}));
    CHECK(config.show_help && !config.export_path);
    parse({"-sfirst", "--save-to", "--", "./app"});
    CHECK(errors == std::vector<std::string>{"missing value"});
    CHECK(config.export_path == "first" && config.program == "./app");
    parse({"-sfirst", "--save-to=second"});
    CHECK(errors == std::vector<std::string>{"--save-to may only be specified once"});
    CHECK(config.export_path == "first");
    parse({});
    CHECK(errors.empty() && !config.export_path && !config.show_help && config.program.empty());
    return 0;
}

int TestRegistrationValidation()
{
    OptionRegistryBuilder builder;
    builder.Add(Flag("--help", 'h'));
    for (auto spec :
         {Flag("--help"), Flag("--other", 'h'), Flag("--"), Flag("-bad"), Flag("--bad=value"), Flag("--bad name"),
          Flag("--bad", '-')}) {
        bool rejected = false;
        try {
            builder.Add(std::move(spec));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        CHECK(rejected);
        CHECK(builder.Build().Options().size() == 1);
    }
    auto missing_handler = Flag("--new");
    missing_handler.apply = {};
    auto missing_diagnostic = Value("--new");
    missing_diagnostic.missing_message.clear();
    auto invalid_protection = Value("--new");
    invalid_protection.protect_from_value = true;
    for (auto spec : {missing_handler, missing_diagnostic, invalid_protection}) {
        bool rejected = false;
        try {
            builder.Add(std::move(spec));
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        CHECK(rejected);
    }
    builder.Add(Value("--valid", 'v'));
    CHECK(builder.Build().Options().size() == 2);
    return 0;
}

int TestOwnershipAndIsolation()
{
    OptionRegistryBuilder builder;
    auto spec = Value("--output", 'o');
    spec.help = "owned help";
    builder.Add(spec);
    auto first = builder.Build();
    spec.long_name = "--changed";
    spec.help.clear();
    builder.Add(Flag("--verbose", 'v'));
    auto second = builder.Build();
    CHECK(first.Options().size() == 1);
    CHECK(first.Options()[0].help == "owned help");
    CHECK(first.Classify("--verbose").kind == TokenKind::UnknownOption);
    CHECK(second.Classify("--verbose").kind == TokenKind::Option);
    auto independent = OptionRegistryBuilder{}.Build();
    CHECK(independent.Classify("--output").kind == TokenKind::UnknownOption);
    CHECK(first.Classify("-o").match->option_id == first.Classify("--output").match->option_id);
    CHECK(second.Options()[0].long_name == "--output");
    CHECK(second.Options()[1].long_name == "--verbose");
    return 0;
}

int TestClassification()
{
    const auto registry = Options();
    CHECK(registry.Classify("--").kind == TokenKind::Separator);
    for (const std::string text : {"./app", "app", "-", ""}) {
        auto token = registry.Classify(text);
        CHECK(token.kind == TokenKind::Application);
        CHECK(!token.match);
    }
    for (const std::string text : {"--unknown", "--hel", "-hh", "-ho", "---"}) {
        CHECK(registry.Classify(text).kind == TokenKind::UnknownOption);
        CHECK(!registry.Classify(text).match);
    }
    for (const std::string text : {"--help", "-h"}) {
        auto token = registry.Classify(text);
        CHECK(token.kind == TokenKind::Option);
        CHECK(token.match->option_id == 0);
        CHECK(!token.match->has_inline_value);
    }
    for (const std::string text : {"--export", "-o"}) {
        auto token = registry.Classify(text);
        CHECK(token.match->option_id == 1);
        CHECK(!token.match->has_inline_value);
    }
    for (const std::string text : {"--export=two words", "-otwo words"}) {
        auto token = registry.Classify(text);
        CHECK(token.match->option_id == 1);
        CHECK(token.match->has_inline_value);
        CHECK(token.match->inline_value == "two words");
    }
    auto empty = registry.Classify("--export=");
    CHECK(empty.match->has_inline_value && empty.match->inline_value.empty());
    CHECK(registry.Classify("--export=a=b").match->inline_value == "a=b");
    CHECK(registry.Classify("-o--").match->inline_value == "--");
    return 0;
}

int TestValueBoundaries()
{
    const auto registry = Options();
    const auto match = *registry.Classify("--export").match;
    for (const std::string text : {"--", "--help", "-h"}) {
        std::string storage = text;
        char* argv[] = {storage.data()};
        ArgumentCursor cursor(1, argv, 0);
        auto value = registry.ReadValue(match, cursor);
        CHECK(value.state == ValueState::Missing);
        CHECK(cursor.Position() == 0 && cursor.Peek() == text);
    }
    for (const std::string text : {"--unknown", "--export", "--help=value", "-hh", "file", "", "-"}) {
        std::string storage = text;
        char* argv[] = {storage.data()};
        ArgumentCursor cursor(1, argv, 0);
        auto value = registry.ReadValue(match, cursor);
        CHECK(value.state == ValueState::Present && value.text == text);
        CHECK(cursor.Position() == 1 && !cursor.HasNext());
    }
    ArgumentCursor exhausted(0, nullptr, 0);
    CHECK(registry.ReadValue(match, exhausted).state == ValueState::Missing);
    char* null_argument[] = {nullptr};
    ArgumentCursor null_cursor(1, null_argument, 0);
    auto value = registry.ReadValue(match, null_cursor);
    CHECK(value.state == ValueState::Present && value.text.empty());
    CHECK(null_cursor.Position() == 1);
    return 0;
}

int TestInlineAndFlagDoNotConsume()
{
    const auto registry = Options();
    std::string next = "./app";
    char* argv[] = {next.data()};
    for (const std::string text : {"--export=", "--export=--", "-o--", "--help", "-h"}) {
        ArgumentCursor cursor(1, argv, 0);
        const auto match = *registry.Classify(text).match;
        const auto value = registry.ReadValue(match, cursor);
        CHECK(cursor.Position() == 0 && cursor.Peek() == "./app");
        CHECK(value.state == (match.has_inline_value ? ValueState::Present : ValueState::None));
        CHECK(value.text == match.inline_value);
    }
    return 0;
}

int TestRegisteredProtectionAndHandlers()
{
    OptionRegistryBuilder builder;
    auto usage = Flag("--usage", 'u');
    usage.protect_from_value = true;
    usage.apply = [](const std::string&, CliConfig& config, std::vector<std::string>&) { config.show_help = true; };
    builder.Add(usage);
    auto output = Value("--save-to", 's');
    output.repeat = RepeatPolicy::Once;
    output.on_seen = [](CliConfig& config) { config.replay_mode_specified = true; };
    output.apply = [](const std::string& value, CliConfig& config, std::vector<std::string>&) {
        config.export_path = value;
    };
    builder.Add(output);
    const auto registry = builder.Build();
    for (const std::string text : {"--usage", "-u"}) {
        std::string storage = text;
        char* argv[] = {storage.data()};
        ArgumentCursor cursor(1, argv, 0);
        CHECK(registry.ReadValue(*registry.Classify("-s").match, cursor).state == ValueState::Missing);
        CHECK(cursor.Position() == 0);
    }
    CliConfig config;
    std::vector<std::string> errors;
    ArgumentCursor cursor(0, nullptr, 0);
    auto token = registry.Classify("--save-to=result.npu-rep");
    const auto& spec = registry.Options()[token.match->option_id];
    CHECK(spec.repeat == RepeatPolicy::Once);
    spec.on_seen(config);
    spec.apply(registry.ReadValue(*token.match, cursor).text, config, errors);
    registry.Options()[0].apply("", config, errors);
    CHECK(config.show_help && config.replay_mode_specified && config.export_path == "result.npu-rep");
    CHECK(errors.empty());
    return 0;
}
} // namespace

static int RunSuiteMain()
{
    return TestParsingRegisteredExtensions() || TestRegisteredHelp() || TestOccurrenceTracker() ||
           TestRegistrationValidation() || TestOwnershipAndIsolation() || TestClassification() ||
           TestValueBoundaries() || TestInlineAndFlagDoNotConsume() || TestRegisteredProtectionAndHandlers();
}

TEST(NpuComputeOptionRegistry, Main) { ASSERT_EQ(RunSuiteMain(), 0); }

TEST(NpuComputeOptionRegistry, RejectsFlagValuesWithoutCallingHandlers)
{
    using namespace npucompute::cli;
    unsigned seen = 0;
    unsigned applied = 0;
    OptionRegistryBuilder builder;
    auto flag = Flag("--custom-flag");
    flag.repeat = RepeatPolicy::Once;
    flag.on_seen = [&](CliConfig&) { ++seen; };
    flag.apply = [&](const std::string&, CliConfig&, std::vector<std::string>&) { ++applied; };
    builder.Add(flag);
    const auto registry = builder.Build();
    CliConfig config;
    std::vector<std::string> errors;
    std::vector<std::string> args = {"test", "--custom-flag=x", "--custom-flag=", "--custom-flag", "--unknown=x", "--",
                                     "app",  "--custom-flag=x"};
    std::vector<char*> argv;
    for (auto& arg : args) {
        argv.push_back(arg.data());
    }
    registry.Parse(argv.size(), argv.data(), config, errors);
    EXPECT_EQ(
        errors, (std::vector<std::string>{
                    "--custom-flag does not take a value.", "--custom-flag does not take a value.",
                    "--custom-flag may only be specified once",
                    "unknown option '--unknown=x'. Use --help to see supported options."}));
    EXPECT_EQ(seen, 1U);
    EXPECT_EQ(applied, 0U);
    EXPECT_EQ(config.program, "app");
    EXPECT_EQ(config.program_arguments, std::vector<std::string>{"--custom-flag=x"});
}
