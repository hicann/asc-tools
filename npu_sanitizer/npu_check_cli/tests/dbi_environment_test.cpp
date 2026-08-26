// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "dbi_environment.h"

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace npu::sanitizer::cli {
namespace {

class ScopedEnvironmentVariable {
public:
    explicit ScopedEnvironmentVariable(std::string name) : name_(std::move(name))
    {
        const char* value = std::getenv(name_.c_str());
        if (value != nullptr) {
            original_ = value;
            wasSet_ = true;
        }
    }

    ~ScopedEnvironmentVariable()
    {
        if (wasSet_) {
            (void)setenv(name_.c_str(), original_.c_str(), 1);
        } else {
            (void)unsetenv(name_.c_str());
        }
    }

private:
    std::string name_;
    std::string original_;
    bool wasSet_ = false;
};

TEST(DbiEnvironmentTest, PreservesResolvedSessionConfigurationAndCompilerArguments)
{
    ipc::ToolConfig config{};
    config.workDir = "/tmp/session";
    config.probeCacheDir = "/var/tmp/probe-cache";
    config.strict = false;
    config.keepTemp = true;
    config.compileOptions = {"-g", "-DVALUE=a b", ""};

    const auto entries = BuildDbiEnvironment(config);
    const std::map<std::string, std::string> environment(entries.begin(), entries.end());

    EXPECT_EQ(environment.at("NPU_CHECK_DBI_WORK_DIR"), "/tmp/session");
    EXPECT_EQ(environment.at("NPU_CHECK_DBI_CACHE_DIR"), "/var/tmp/probe-cache");
    EXPECT_EQ(environment.at("NPU_CHECK_DBI_STRICT"), "0");
    EXPECT_EQ(environment.at("NPU_CHECK_DBI_KEEP_TEMP"), "1");
    EXPECT_EQ(environment.at("NPU_CHECK_DBI_COMPILER_ARG_COUNT"), "3");
    EXPECT_EQ(environment.at("NPU_CHECK_DBI_COMPILER_ARG_0"), "-g");
    EXPECT_EQ(environment.at("NPU_CHECK_DBI_COMPILER_ARG_1"), "-DVALUE=a b");
    EXPECT_EQ(environment.at("NPU_CHECK_DBI_COMPILER_ARG_2"), "");
}

TEST(DbiEnvironmentTest, AppliesEntriesWithOverwriteAndPreservesEmptyValues)
{
    const std::string firstName = "NPU_CHECK_DBI_TEST_APPLY_FIRST";
    const std::string emptyName = "NPU_CHECK_DBI_TEST_APPLY_EMPTY";
    ScopedEnvironmentVariable restoreFirst(firstName);
    ScopedEnvironmentVariable restoreEmpty(emptyName);
    ASSERT_EQ(setenv(firstName.c_str(), "old", 1), 0);

    std::string error;
    ASSERT_TRUE(ApplyEnvironment({{firstName, "new"}, {emptyName, ""}}, error)) << error;

    ASSERT_NE(std::getenv(firstName.c_str()), nullptr);
    EXPECT_STREQ(std::getenv(firstName.c_str()), "new");
    ASSERT_NE(std::getenv(emptyName.c_str()), nullptr);
    EXPECT_STREQ(std::getenv(emptyName.c_str()), "");
}

TEST(DbiEnvironmentTest, ReportsVariableAndSystemErrorWhenSetenvFails)
{
    const std::string invalidName = "NPU_CHECK_DBI_INVALID=NAME";
    std::string error;

    EXPECT_FALSE(ApplyEnvironment({{invalidName, "value"}}, error));
    EXPECT_NE(error.find(invalidName), std::string::npos);
    EXPECT_NE(error.find(std::strerror(EINVAL)), std::string::npos);
}

} // namespace
} // namespace npu::sanitizer::cli
