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
 * \file op_generator_factory.h
 * \brief
 */

#ifndef GENERATOR_FACTORY_H
#define GENERATOR_FACTORY_H

#include <functional>
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include "op_generator.h"
#include "register/op_def.h"
#include "register/op_def_factory.h"
#include "op_build_error_codes.h"

namespace ops {
using GBuilder = std::function<uint32_t(std::vector<std::string>&)>;

class GeneratorFactory {
public:
    static void AddBuilder(const char* name, GBuilder builder);
    static opbuild::Status Build(std::vector<std::string>& ops);
    static opbuild::Status SetGenPath(const std::string& path);
    static const std::vector<std::string>& GetErrorMessage();
    static std::vector<ge::AscendString>& FactoryGetAllOp();
    static opbuild::Status SetCPUMode(const std::string& arg);
};
}
#endif
