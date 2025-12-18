/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

/*!
 * \file op_build.cpp
 * \brief
 */

#include <iostream>
#include <dlfcn.h>
#include "register/op_def.h"
#include "register/op_def_factory.h"
#include "op_generator_factory.h"
#include "ascendc_tool_log.h"
#include "op_build_error_codes.h"
#include "op_build_params.h"

namespace {
constexpr uint32_t ARG_NUM_LIB = 1;
constexpr uint32_t ARG_NUM_PATH = 2;
constexpr uint32_t ARG_NUM_VALID = 3;
constexpr uint32_t ARG_NUM_MAX = 16;
}

#ifndef UT_TEST
int main(int argc, char* argv[])
{
#else
int opbuild_main(int argc, std::vector<std::string> args)
{
    char* argv[ARG_NUM_MAX];
    for (int ind = 0; ind < args.size() && ind < argc; ind++) {
        argv[ind] = (char*)args[ind].c_str();
    }
#endif
    if (!opbuild::Params::GetInstance().Parse(argc, argv)) {
        return 1;
    }

    void* handle = dlopen(opbuild::Params::GetInstance().Required(ARG_NUM_LIB).c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        ASCENDLOGE("dlopen error : [%s] : %s", opbuild::Params::GetInstance().Required(ARG_NUM_LIB).c_str(), dlerror());
        return 1;
    }
    auto& allOps = ops::GeneratorFactory::FactoryGetAllOp();
    if (allOps.empty()) {
        dlclose(handle);
        ASCENDLOGE("No operator is registered!");
        return 1;
    }
    std::vector<std::string> stdOps = {};
    for (auto& op : allOps) {
        const std::string opType = op.GetString();
        if (!ops::IsVaildOpTypeName(opType)) {
            ASCENDLOGW("Optype: [%s] does not comply with the naming convention; it is recommended to \
use the PascalCase format.", opType.c_str());
        }
        stdOps.emplace_back(opType);
    }
    opbuild::Status result = ops::GeneratorFactory::SetGenPath(opbuild::Params::GetInstance().Required(ARG_NUM_PATH).c_str());
    if (result == opbuild::OPBUILD_FAILED) {
        dlclose(handle);
        ASCENDLOGE("set generate path failed!");
        return 1;
    }
    (void)ops::GeneratorFactory::SetCPUMode("--aicore");
    result = ops::GeneratorFactory::Build(stdOps);
    if (result == opbuild::OPBUILD_FAILED) {
        dlclose(handle);
        ASCENDLOGE("file generate failed!");
        return 1;
    }
    std::vector<std::string> errMessage = ops::GeneratorFactory::GetErrorMessage();
    if (errMessage.size() > 0U) {
        for (std::string& str : errMessage) {
            std::cerr << str << std::endl;
        }
        return 1;
    }
    return 0;
}
