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
 * \file op_build_params.cpp
 * \brief
 */


#include "op_build_params.h"
#include <cstring>
#include <unordered_set>
#include "ascendc_tool_log.h"

namespace opbuild {
constexpr uint32_t ARG_NUM_BIN = 0;
constexpr uint32_t ARG_NUM_VALID = 2;

Params& Params::GetInstance()
{
    static Params params;
    return params;
}

const std::string Params::Optional(const std::string& key)
{
    auto iter = optionParams_.find(key);
    if (iter == optionParams_.end()) {
        return "";
    }
    return iter->second;
}

const std::string Params::Required(uint32_t index)
{
    if (index >= requiredParams_.size()) {
        return "";
    }
    return requiredParams_[index];
}

bool Params::Parse(int argc, char* argv[])
{
    constexpr size_t prefixLen = 2;
    for (int i = 0; i < argc; i++) {
        ASCENDLOGI("Start to parse current argv %s", argv[i]);
        std::string arg = argv[i];
        if (arg.substr(0, prefixLen) == "--") {
            std::string key = "";
            std::string val = "";
            if (arg.find("=") != std::string::npos) {
                size_t indexAddr = arg.find("=");
                key = arg.substr(prefixLen, indexAddr - prefixLen);
                val = arg.substr(indexAddr + 1);
                if (val == "") {
                    ASCENDLOGE("invalid param value: %s", argv[i]);
                    return false;
                }
                ASCENDLOGI("Set optional params %s to %s", key.c_str(), val.c_str());
            } else {
                key = "cpu_mode";
                val = arg;
                static const std::unordered_set<std::string> validKey = {
                    "--aicpu",
                    "--aicore",
                    "--hostcpu"
                };
                if (validKey.find(val) == validKey.end()) {
                    ASCENDLOGE("Invalid key: %s, must be one of: --aicpu, --aicore, --hostcpu", val.c_str());
                    return false;
                }
                ASCENDLOGI("Set optional params %s", val.c_str());
            }
            optionParams_[key] = val;
        } else {
            ASCENDLOGI("Set required params %lu to %s", requiredParams_.size(), argv[i]);
            requiredParams_.push_back(argv[i]);
        }
    }
    return Check(std::string(argv[ARG_NUM_BIN]));
}

bool Params::Check(std::string param) const {
    if (requiredParams_.size() < ARG_NUM_VALID) {
        ASCENDLOGE("usage: %s <op shared library> <output directory>", param.c_str());
        return false;
    }
    return true;
}

}
