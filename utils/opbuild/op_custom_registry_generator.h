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
 * \file op_custom_registry_generator.h
 * \brief
 */

#ifndef STUB_OP_CUSTOM_REGISTRY_GENERATOR_H
#define STUB_OP_CUSTOM_REGISTRY_GENERATOR_H
#include <fstream>
#include <map>
#include <sstream>
#include "op_generator.h"
#include "op_generator_factory.h"
#include "register/op_def.h"
#include "register/op_def_factory.h"
#include "op_build_error_codes.h"

namespace ops {
class OpCustomGenerator : public Generator {
public:
    explicit OpCustomGenerator(std::vector<std::string>& ops);
    opbuild::Status GenerateCode(void) override;
    ~OpCustomGenerator() override = default;
private:
    std::string fileGenPath = "";
    void OpCustomGenHead(std::ofstream& outfile) const;
    void OpCustomGenMacro(std::ofstream& outfile) const;
    void OpCustomGenUtilsFunc(std::ofstream& outfile) const;
    void OpCustomGenGetBasePathFunc(std::ofstream& outfile) const;
    void OpCustomGenWirteFileFunc(std::ofstream& outfile) const;
    void OpCustomGenSymlinkFunc(std::ofstream& outfile) const;
    void OpCustomGenSymArchFunc(std::ofstream& outfile) const;
    void OpCustomGenRegistryfirstFunc(std::ofstream& outfile) const;
    void OpCustomGenRegistrylastFunc(std::ofstream& outfile) const;
    void OpCustomGenRemoveDirFunc(std::ofstream& outfile) const;
    void OpCustomGen(std::ofstream& outfile, const char* vendorName) const;
    void OpCustomGenDestryFunc(std::ofstream& outfile) const;
};
}
#endif