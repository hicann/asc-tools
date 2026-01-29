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
 * \file op_cpu_cfg_generator.h
 * \brief
 */

#ifndef STUB_OP_CFG_GENERATOR_H
#define STUB_OP_CFG_GENERATOR_H

#include <map>
#include "op_generator.h"
#include "register/op_def.h"
#include "op_build_error_codes.h"

namespace ops {
class CPUCfgGenerator : public Generator {
public:
    std::string GetDataTypeName(const ge::DataType& type) const;
    void GetParamFormats(std::vector<ge::Format>& formats, std::string& fmtstr) const;
    void GetParamDataTypes(std::vector<ge::DataType>& types, std::string& tpstr) const;
    void GenParamInfo(std::ofstream& outfile, std::vector<OpParamDef>& param, bool isOutput) const;

    void GenConfigInfo(std::ofstream& outfile, const std::map<ge::AscendString, ge::AscendString>& config) const;

    explicit CPUCfgGenerator(std::vector<std::string>& ops);
    opbuild::Status GenerateCode(void) override;
    ~CPUCfgGenerator() override = default;
};
}

#endif
