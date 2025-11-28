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
 * \file op_aclnn_fallback_generator.h
 * \brief
 */

#ifndef ACLNN_FALLBACK_GENERATOR_H
#define ACLNN_FALLBACK_GENERATOR_H

#include <fstream>
#include <cstring>
#include "register/op_def.h"
#include "register/op_def_factory.h"
#include "op_generator.h"
#include "op_generator_factory.h"

namespace ops {
class AclnnFallBackGenerator : public Generator {
public:
    explicit AclnnFallBackGenerator(std::vector<std::string> &ops);
    ~AclnnFallBackGenerator() override = default;
    opbuild::Status GenerateCode(void) override;
private:
    void AclnnGenFallBack(OpDef &opDef) const;
    void AclnnGenFallBackImpl(OpDef &opDef, std::ofstream &outfile) const;
    void AclnnGenFallBackImplStart(std::ofstream &outfile) const;
    void AclnnGenFallBackImplFunc(
        OpDef &opDef, std::ofstream &outfile, std::vector<size_t> &hostInputs, const std::vector<size_t> &refIndex) const;
    void AclnnGenGetInputTensor(std::vector<OpParamDef> &param, std::ofstream &outfile) const;
    void AclnnGenGetOutputTensor(
        std::vector<OpParamDef> &param, const std::vector<size_t> &refIndex, std::ofstream &outfile) const;
    void AclnnGenGetAttrs(std::vector<OpAttrDef> &attrs, std::ofstream &outfile) const;
    void AclnnGenGetAttrsImpl(std::string &name, int32_t type, std::ofstream &outfile) const;
    void AclnnGenGetOpApiFuncHandle(OpDef &opDef, std::ofstream &outfile) const;
    void AclnnGenCovertAttr(std::vector<OpAttrDef> &attrs, std::ofstream &outfile) const;
    void AclnnGenCovertTensor(std::vector<OpParamDef> &param, std::ofstream &outfile, std::vector<size_t> &hostInputs,
        const std::vector<size_t> refIndex, bool isInput) const;
    void GetHostInputs(std::vector<size_t> &hostInputs, bool isInput, size_t index) const;
    void AclnnGenHostInputs(std::vector<size_t> hostInputs, std::ofstream &outfile) const;
    void AclnnGenValueDependTensor(OpParamDef &param, std::ofstream &outfile) const;
    void AclnnGenCallGetWorkspaceSizeFunc(OpDef &opDef, const std::vector<size_t> refIndex, std::ofstream &outfile) const;
    void AclnnGenOpapiFuncIoName(
        std::vector<OpParamDef> &param, const std::vector<size_t> refIndex, bool isOutput, std::ofstream &outfile) const;
    void AclnnGenOpapiFuncAttrName(std::vector<OpAttrDef> &attrs, std::ofstream &outfile) const;
    void AclnnGenCallApiFunc(OpDef &opDef, std::ofstream &outfile, const std::vector<size_t> &refIndex) const;
    void AclnnGenFallBackFunc(OpDef &opDef, std::ofstream &outfile) const;
    void AclnnGenFallBackGetWorkspaceSizeFunc(OpDef &opDef, std::ofstream &outfile, std::vector<size_t> &refIndex) const;
    bool IsRef(std::vector<OpParamDef> &outputs, ge::AscendString name, std::vector<size_t> &refIndex) const;
    void AclnnGenFallBackInputsFunc(OpDef &opDef, std::vector<size_t> &refIndex, std::ofstream &outfile) const;
    void AclnnGenFallBackOutputsFunc(OpDef &opDef, std::vector<size_t> &refIndex, std::ofstream &outfile) const;
    void HasOutputShapeDepend(std::vector<OpParamDef> &outputs, bool &hasOutputShapeDepend) const;
    void AclnnGenValueDependIoFunc(OpParamDef param, std::ofstream &outfile) const;
    void AclnnGenFallBackAttrFunc(std::vector<OpAttrDef> &attrs, std::ofstream &outfile) const;
    void AclnnGenFallBackAttrFuncImpl(const OpAttrDef &attr, const int32_t type, std::ofstream &outfile) const;
    void AclnnGenDestroyImpl(
        std::vector<OpParamDef> &param, std::ofstream &outfile, const std::vector<size_t> &refIndex, bool isInput) const;
    void AclnnDestroyValueDependTensor(OpParamDef &param, std::ofstream &outfile) const;
    void AclnnGenFallbackCheckInfo(std::ofstream &outfile) const;
    void AclnnGenDestroyAttr(std::vector<OpAttrDef> &attrs, std::ofstream &outfile) const;
};
} // namespace ops
#endif