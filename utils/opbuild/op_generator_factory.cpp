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
 * \file op_generator_factory.cpp
 * \brief
 */

#include "op_generator_factory.h"
#include <cstring>
#include "ascendc_tool_log.h"
#include "mmpa/mmpa_api.h"

namespace ops {
static std::map<std::string, GBuilder> g_generatorBuilder;

void GeneratorFactory::AddBuilder(const char* name, GBuilder builder)
{
    std::string key(name);
    g_generatorBuilder.emplace(name, builder);
}

opbuild::Status GeneratorFactory::Build(std::vector<std::string>& ops)
{
    size_t length;
    std::string cpuArg;
    const char* generatorName = "cpu_cfg";
    length = strlen(generatorName);
    Generator::GetCPUMode(cpuArg);
    for (auto it : g_generatorBuilder) {
        if (cpuArg == "--aicpu" || cpuArg == "--hostcpu") {
            if (strncmp(it.first.c_str(), generatorName, length) != 0) {
                continue;
            }
        } else if (cpuArg == "--aicore") {
            if (strncmp(it.first.c_str(), generatorName, length) == 0) {
                continue;
            }
        }
        ASCENDLOGI("Generator: %s", it.first.c_str());
        if (it.second(ops) == opbuild::OPBUILD_FAILED) {
            ASCENDLOGE("%s build error !", it.first.c_str());
            return opbuild::OPBUILD_FAILED;
        }
    }
    return opbuild::OPBUILD_SUCCESS;
}

opbuild::Status GeneratorFactory::SetGenPath(const std::string& path)
{
    return Generator::SetGenPath(path);
}

opbuild::Status GeneratorFactory::SetCPUMode(const std::string& arg)
{
    return Generator::SetCPUMode(arg);
}

std::vector<ge::AscendString>& GeneratorFactory::FactoryGetAllOp()
{
    return OpDefFactory::GetAllOp();
}

const std::vector<std::string>& GeneratorFactory::GetErrorMessage()
{
    return Generator::GetErrorMessage();
}
}
