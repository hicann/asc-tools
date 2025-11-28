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
 * \file op_generator.cpp
 * \brief
 */

#include "op_generator.h"
#include <mutex>
#include <unordered_set>
#include "ascendc_tool_log.h"

namespace ops {
static std::string g_outputPath = ".";
static std::string g_cpuArg = "--aicore";
static std::vector<std::string> g_errorInfo;
std::mutex g_opbuildMtx;
std::mutex g_opbuildCPUMtx;

Generator::Generator(std::vector<std::string>& ops)
{
    this->opmodels_ = ops;
}

bool IsVaildOpTypeName(const std::string& opType) {
    if (opType.empty()) {
        return false;
    }
    // the first letter must be capitalized
    if (!isupper(static_cast<unsigned char>(opType[0]))) {
        ASCENDLOGW("Optype: [%s] does not comply with the naming convention; \
The first letter must be capitalized", opType.c_str());
        return false;
    }

    for (size_t i = 1; i < opType.size(); ++i) {
        char character = opType[i];
        if (!std::isalnum(static_cast<unsigned char>(character))) {
            ASCENDLOGW("Optype: [%s] does not comply with the naming convention; \
There are characters [%c], that are neither numbers nor letters.", opType.c_str(), character);
            return false;
        }
    }
    return true;
}

opbuild::Status Generator::SetGenPath(const std::string& path)
{
    if (path.empty()) {
        ASCENDLOGE("SetPath is empty");
        return opbuild::OPBUILD_FAILED;
    }
    const std::lock_guard<std::mutex> lock(g_opbuildMtx);
    g_outputPath = path;
    return opbuild::OPBUILD_SUCCESS;
}

void Generator::GetGenPath(std::string& path)
{
    const std::lock_guard<std::mutex> lock(g_opbuildMtx);
    path = g_outputPath;
}

opbuild::Status Generator::SetCPUMode(const std::string& arg)
{
    if (arg.empty()) {
        ASCENDLOGE("arg is empty");
        return opbuild::OPBUILD_FAILED;
    }
    static const std::unordered_set<std::string> validCpuModes = {
        "--aicpu",
        "--aicore",
        "--hostcpu"
    };
    if (validCpuModes.find(arg) == validCpuModes.end()) {
        ASCENDLOGE("Invalid CPU mode: %s, must be one of: --aicpu, --aicore, --hostcpu", arg.c_str());
        return opbuild::OPBUILD_FAILED;
    }
    const std::lock_guard<std::mutex> lock(g_opbuildCPUMtx);
    g_cpuArg = arg;
    return opbuild::OPBUILD_SUCCESS;
}
 
void Generator::GetCPUMode(std::string& arg)
{
    const std::lock_guard<std::mutex> lock(g_opbuildCPUMtx);
    arg = g_cpuArg;
}

void Generator::SetErrorMessage(const std::string& info)
{
    const std::lock_guard<std::mutex> lock(g_opbuildMtx);
    g_errorInfo.push_back(info);
}

const std::vector<std::string>& Generator::GetErrorMessage()
{
    const std::lock_guard<std::mutex> lock(g_opbuildMtx);
    return g_errorInfo;
}

const std::vector<std::string>& Generator::GetAllOp(void)
{
    return this->opmodels_;
}

opbuild::Status Generator::GenerateCode(void)
{
    return opbuild::OPBUILD_SUCCESS;
}

/**
 * 将aclnnOpType转换成全大写的宏名称，遇到大写字符且上一个字符是小写，中间加_分割，连续大写的场景不做处理
 */
std::string GenerateMacros(const std::string& str)
{
    std::string result;
    for (std::size_t i = 0; i < str.size(); ++i) {
        if (std::isupper(str[i])) {
            if (i > 0 && std::islower(str[i - 1])) {
                result += '_';
            }
        }

        result += std::toupper(str[i]);
    }
    return result;
}

/**
 * 将OpType转换全小写的文件名，遇到大写字符加_分割
 */
std::string ConvertToSnakeCase(const std::string& ins)
{
    if (ins.empty()) {
        ASCENDLOGE("OpType is empty");
        return "";
    }
    std::string str = "";
    str = std::tolower(ins[0]);
    for (std::size_t i = 1; i < ins.length(); i++) {
        if (std::isupper(ins[i])) {
            if (ins[i - 1] == '_') {
                str += std::tolower(ins[i]);
                continue;
            }
            if (!std::isupper(ins[i - 1])) {
                str += '_';
            } else if (std::isupper(ins[i - 1]) && ((i + 1) < ins.length()) && std::islower(ins[i + 1])) {
                str += '_';
            }
            str += std::tolower(ins[i]);
        } else {
            str += ins[i];
        }
    }
    return str;
}
}
