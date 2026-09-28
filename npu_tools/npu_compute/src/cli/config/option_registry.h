/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_TOOLS_NPU_COMPUTE_SRC_CLI_CONFIG_OPTION_REGISTRY_H
#define NPU_TOOLS_NPU_COMPUTE_SRC_CLI_CONFIG_OPTION_REGISTRY_H

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace npucompute::cli {
struct CliConfig;
enum class OptionArity { Flag, RequiredValue };
enum class RepeatPolicy { Allow, Once };
enum class TokenKind { Separator, Option, Application, UnknownOption };
enum class ValueState { None, Missing, Present };

struct OptionValue {
    ValueState state = ValueState::None;
    std::string text;
};
struct OptionSpec {
    std::string long_name;
    char short_name = '\0';
    OptionArity arity = OptionArity::Flag;
    RepeatPolicy repeat = RepeatPolicy::Allow;
    bool protect_from_value = false;
    std::string missing_message;
    std::function<void(CliConfig&)> on_seen;
    std::function<void(const std::string&, CliConfig&, std::vector<std::string>&)> apply;
    std::string help;
};
struct OptionMatch {
    std::size_t option_id;
    bool has_inline_value = false;
    std::string inline_value;
};
struct ClassifiedToken {
    TokenKind kind;
    std::optional<OptionMatch> match;
};

// The caller consumes the option token before asking ReadValue to read its value.
class ArgumentCursor {
public:
    ArgumentCursor(int argc, char** argv, int start = 1) : argc_(argc), argv_(argv), position_(start) {}
    bool HasNext() const { return position_ < argc_; }
    int Position() const { return position_; }
    std::string Peek() const;
    std::string Take();

private:
    int argc_;
    char** argv_;
    int position_;
};

class OccurrenceTracker {
public:
    explicit OccurrenceTracker(std::size_t option_count) : counts_(option_count, 0) {}
    unsigned Record(std::size_t option_id) { return ++counts_.at(option_id); }
    unsigned Count(std::size_t option_id) const { return counts_.at(option_id); }

private:
    std::vector<unsigned> counts_;
};

class OptionRegistryBuilder;
class OptionRegistry {
public:
    const std::vector<OptionSpec>& Options() const { return options_; }
    // Resets the output state and parses tool options followed by the application.
    // Business combination validation belongs to the caller.
    void Parse(int argc, char** argv, CliConfig& config, std::vector<std::string>& errors) const;
    std::string HelpText() const;
    ClassifiedToken Classify(const std::string& text) const;
    OptionValue ReadValue(const OptionMatch& match, ArgumentCursor& cursor) const;

private:
    friend class OptionRegistryBuilder;
    explicit OptionRegistry(std::vector<OptionSpec> options);
    std::optional<OptionMatch> Match(const std::string& text) const;
    bool ProtectsValue(const std::string& text) const;
    std::vector<OptionSpec> options_;
    std::unordered_map<std::string, std::size_t> names_;
    std::unordered_map<char, std::size_t> short_names_;
};

// Invalid definitions throw std::invalid_argument before modifying the builder.
// Build returns an owning snapshot; subsequent Add calls cannot change it.
class OptionRegistryBuilder {
public:
    void Add(OptionSpec option);
    OptionRegistry Build() const;

private:
    std::vector<OptionSpec> options_;
};
} // namespace npucompute::cli
#endif // NPU_TOOLS_NPU_COMPUTE_SRC_CLI_CONFIG_OPTION_REGISTRY_H
