/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "injection_path.h"

#include <cstdio>
#include <fstream>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <unistd.h>

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

bool WritePlaceholder(const boost::filesystem::path& path)
{
    boost::system::error_code error;
    boost::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output(path.string());
    output << "placeholder";
    return output.good();
}

} // namespace

int main()
{
    using npu_compute::compute_launcher::ResolveInjectionLibraryPath;

    boost::system::error_code error;
    const boost::filesystem::path executable = boost::filesystem::canonical("/proc/self/exe", error);
    CHECK(!error);
    const boost::filesystem::path executableDirectory = executable.parent_path();
    const boost::filesystem::path installedLibrary = executableDirectory / "../lib64/libnpu-compute.so";

    boost::filesystem::remove(installedLibrary, error);
    CHECK(WritePlaceholder(installedLibrary));

    std::string resolved;
    std::string resolveError;
    CHECK(ResolveInjectionLibraryPath(&resolved, &resolveError));
    CHECK(resolved == boost::filesystem::canonical(installedLibrary).string());

    boost::filesystem::remove(installedLibrary, error);
    return 0;
}
