// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <set>
#include <string>

namespace {

bool EndsWith(const std::string& value, const std::string& suffix)
{
    return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) {
        return 2;
    }
    const std::string tool = std::filesystem::path(argv[0]).filename().string();
    const char* logPath = std::getenv("DBI_FAKE_LOG");
    if (logPath == nullptr || logPath[0] == '\0') {
        return 2;
    }
    std::ofstream log(logPath, std::ios::app);
    if (!log) {
        return 2;
    }
    log << tool;
    for (int index = 1; index < argc; ++index) {
        log << " <" << argv[index] << '>';
    }
    log << '\n';

    if (tool == "llvm-objdump") {
        const std::string input = argc > 1 ? argv[argc - 1] : "";
        if (EndsWith(input, "group.o")) {
            std::ifstream artifact(input);
            const std::string content{std::istreambuf_iterator<char>(artifact), std::istreambuf_iterator<char>()};
            const std::regex symbolPattern("__sanitizer_report_[A-Za-z0-9_]+");
            std::set<std::string> symbols;
            for (std::sregex_iterator it(content.begin(), content.end(), symbolPattern), end; it != end; ++it) {
                symbols.insert(it->str());
            }
            for (const std::string& symbol : symbols) {
                std::cout << "00000000 w F .text.probe 00000010 " << symbol << '\n';
            }
        } else if (EndsWith(input, "probe.o")) {
            std::cout << "00000000 w F .text.probe 00000010 __sanitizer_report_probe\n";
        } else {
            std::cout << "00000000 g F .text.kernel 00000010 FullFlowKernel\n";
        }
        return 0;
    }

    std::string output;
    std::string source;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "-o" && index + 1 < argc) {
            output = argv[++index];
        } else if (argument == "-c" && index + 1 < argc) {
            source = argv[++index];
        } else if (argument.rfind("-o=", 0) == 0) {
            output = argument.substr(3);
        }
    }
    if (output.empty()) {
        return 2;
    }
    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(output).parent_path(), error);
    if (error) {
        return 2;
    }
    std::ofstream artifact(output, std::ios::binary | std::ios::trunc);
    if (tool == "bisheng" && !source.empty()) {
        std::ifstream generated(source, std::ios::binary);
        artifact << generated.rdbuf();
    } else {
        artifact << "fake-" << tool << '\n';
    }
    return artifact.good() ? 0 : 2;
}
