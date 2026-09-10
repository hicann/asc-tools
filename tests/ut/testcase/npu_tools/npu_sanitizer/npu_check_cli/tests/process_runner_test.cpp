// Copyright (c) 2026 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#include "process_runner.h"
#include "../../common/tests/plog_capture.h"
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <fstream>
#include <sstream>

namespace npucheck {
TEST(ProcessRunnerTest, InternalDiagnosticsUsePlogWithoutContaminatingCheckOutput)
{
    const auto directory =
        boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("npu-check-plog-%%%%-%%%%");
    boost::filesystem::create_directories(directory);
    Options options;
    options.application = {"/bin/true"};
    options.handshakeTimeoutMs = 100;
    options.workDir = directory.string();
    options.logFile = (directory / "check.log").string();
    {
        std::ofstream previous(options.logFile);
        previous << "OLD_REPORT_MUST_BE_REPLACED\n";
    }
    PlogCapture capture;
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    const int result = RunApplication(options, "/missing/libnpu_check.so");
    const auto console = testing::internal::GetCapturedStdout() + testing::internal::GetCapturedStderr();
    std::ifstream file(options.logFile);
    std::ostringstream contents;
    contents << file.rdbuf();
    EXPECT_EQ(result, 125); // /bin/true does not initialize ACL or perform the handshake.
    EXPECT_NE(console.find("[CLI] outcome=infra_failed"), std::string::npos);
    EXPECT_NE(contents.str().find("handshake=missing"), std::string::npos);
    EXPECT_EQ(contents.str().find("OLD_REPORT_MUST_BE_REPLACED"), std::string::npos);
    for (const auto* internal : {"[INJECTION]", "[UDS]", "[CLI] session="}) {
        EXPECT_EQ(console.find(internal), std::string::npos);
        EXPECT_EQ(contents.str().find(internal), std::string::npos);
        EXPECT_NE(capture.Text().find(internal), std::string::npos);
    }
    EXPECT_FALSE(boost::filesystem::exists(directory / "npu_check.log"));
    boost::filesystem::remove_all(directory);
}
TEST(ProcessRunnerTest, InvalidLogPathDoesNotStartApplicationOrCreateDirectories)
{
    const auto directory =
        boost::filesystem::temp_directory_path() / boost::filesystem::unique_path("log-reject-%%%%-%%%%");
    boost::filesystem::create_directories(directory);
    const auto marker = directory / "started";
    Options options;
    options.application = {"/usr/bin/touch", marker.string()};
    options.workDir = directory.string();
    options.handshakeTimeoutMs = 100;
    for (const auto& path : {directory.string(), (directory / "missing/report.log").string()}) {
        options.logFile = path;
        EXPECT_EQ(RunApplication(options, "/missing/libnpu_check.so"), 125);
        EXPECT_FALSE(boost::filesystem::exists(marker));
    }
    EXPECT_FALSE(boost::filesystem::exists(directory / "missing"));
    boost::filesystem::remove_all(directory);
}
} // namespace npucheck
