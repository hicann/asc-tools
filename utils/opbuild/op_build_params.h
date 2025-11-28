/*
 * This program is free software, you can redistribute it and/or modify it.
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This file is a part of the CANN Open Software.
 * Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/*!
 * \file op_cfg_generator.h
 * \brief
 */

#ifndef OP_BUILD_PARAMS_H_
#define OP_BUILD_PARAMS_H_
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace opbuild {
class Params {
public:
    static Params& GetInstance();
    bool Parse(int argc, char* argv[]);
    const std::string Optional(const std::string& key);
    const std::string Required(uint32_t index);

private:
    bool Check(std::string param) const;
    std::map<std::string, std::string> optionParams_;
    std::vector<std::string> requiredParams_;
};
} // end for namespace opbuild
#endif // endif for OP_BUILD_PARAMS_H_
