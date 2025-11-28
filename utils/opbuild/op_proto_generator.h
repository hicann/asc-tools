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
 * \file op_proto_generator.h
 * \brief
 */

#ifndef STUB_OP_PROTO_GENERATOR_H
#define STUB_OP_PROTO_GENERATOR_H
#include <set>
#include "op_generator.h"
#include "op_generator_factory.h"
#include "register/op_def.h"
#include "register/op_def_factory.h"
#include "op_build_error_codes.h"

namespace ops {
class OpProtoGenerator : public Generator {
public:
    explicit OpProtoGenerator(std::vector<std::string>& ops);
    opbuild::Status GenerateCode(void) override;
    opbuild::Status GenerateCodeSeparate(void);
    void GenOpRegImplDeclBegin(std::ofstream& outfile, bool isOpProtoH, std::string fileName) const;
    void GenOpRegImplDeclEnd(std::ofstream& outfile, bool isOpProtoH) const;
    void GenOpRegImplDeclOp(OpDef& opDef, std::ofstream& outfile) const;
    void GenOpRegImpl(
        OpDef& opDef,
        std::ofstream& outfile,
        const std::vector<std::pair<int32_t, DependScope>>& valueDependIndexList,
        const std::vector<int32_t>& outShapeDependIndexList) const;
    std::string GenOpShapeInferFunc(OpDef& opDef, std::ofstream& outfile) const;
    std::vector<std::pair<int32_t, DependScope>> GetInputDataDependIndexList(OpDef& opDef) const;
    std::vector<int32_t> GetOutputShapeDependOnComputeIndexList(OpDef& opDef) const;

    ~OpProtoGenerator() override = default;
private:
    using FollowMap = std::map<ge::AscendString, std::vector<std::pair<ge::AscendString, OpDef::PortStat>>>;
    using TypeMap = std::map<std::string, std::set<ge::DataType>>;
    void GenTypeMap(OpDef& opDef, TypeMap& typeMap) const;
    void GenOpComment(OpDef& opDef, std::ofstream& outfile) const;
    std::string GenCommentLine(const std::string& str) const;
    std::string GenCommentFormat(const std::vector<std::string>& commentList) const;
    std::string GenOpCommentInput(OpDef& opDef) const;
    std::string GenOpCommentAttr(OpDef& opDef) const;
    std::string GenOpCommentOutput(OpDef& opDef) const;
    std::string GenOpCommentSee(OpDef& opDef) const;
    std::string GenOpCommentOverview(OpDef& opDef, ops::CommentSection section) const;
    std::string GenOpRegImplSection(
        std::string &indexInputList, std::string &indexInputListTiling, const std::string &opType) const;
    std::vector<std::string> GetOpCommentList(OpDef& opDef) const;
    void GenOpRegImplInputDecl(OpDef& opDef, std::ofstream& outfile, TypeMap& typeMap) const;
    void GenOpRegImplOutputDecl(OpDef& opDef, std::ofstream& outfile, TypeMap& typeMap) const;
    void GenOpRegImplTypeDecl(std::ofstream& outfile, TypeMap& typeMap) const;
    void GenOpRegImplAttrDecl(OpDef& opDef, std::ofstream& outfile) const;
    opbuild::Status GetFile(std::ofstream& fileH, std::ofstream& fileCC, const std::string& catg);
    std::string fileGenPath = "";
    std::set<std::string> protoCatgNames;
    const int gapSize = 2;
    const std::map<ops::CommentSection, std::string> sectionToHead = {
        {ops::CommentSection::BRIEF, "* @par brief\n"},
        {ops::CommentSection::CONSTRAINTS, "* @Attention Constraints:\n"},
        {ops::CommentSection::RESTRICTIONS, "* @par Restrictions:\n"},
        {ops::CommentSection::SEE, "* @see "},
        {ops::CommentSection::THIRDPARTYFWKCOMPAT, "* @par Third-party framework compatibility\n"}
    };
    const std::map<int, std::string> dtypeSuppMap = {
        { ge::DT_FLOAT, "ge::DT_FLOAT" },
        { ge::DT_FLOAT16, "ge::DT_FLOAT16" },
        { ge::DT_INT8, "ge::DT_INT8" },
        { ge::DT_INT16, "ge::DT_INT16" },
        { ge::DT_UINT16, "ge::DT_UINT16" },
        { ge::DT_UINT8, "ge::DT_UINT8" },
        { ge::DT_INT32, "ge::DT_INT32" },
        { ge::DT_INT64, "ge::DT_INT64" },
        { ge::DT_UINT32, "ge::DT_UINT32" },
        { ge::DT_UINT64, "ge::DT_UINT64" },
        { ge::DT_BOOL, "ge::DT_BOOL" },
        { ge::DT_DOUBLE, "ge::DT_DOUBLE" },
        { ge::DT_STRING, "ge::DT_STRING" },
        { ge::DT_COMPLEX32, "ge::DT_COMPLEX32" },
        { ge::DT_COMPLEX64, "ge::DT_COMPLEX64" },
        { ge::DT_COMPLEX128, "ge::DT_COMPLEX128" },
        { ge::DT_RESOURCE, "ge::DT_RESOURCE" },
        { ge::DT_STRING_REF, "ge::DT_STRING_REF" },
        { ge::DT_DUAL, "ge::DT_DUAL" },
        { ge::DT_VARIANT, "ge::DT_VARIANT" },
        { ge::DT_INT4, "ge::DT_INT4" },
        { ge::DT_UINT1, "ge::DT_UINT1" },
        { ge::DT_INT2, "ge::DT_INT2" },
        { ge::DT_UINT2, "ge::DT_UINT2" },
        { ge::DT_DUAL_SUB_INT8, "ge::DT_DUAL_SUB_INT8" },
        { ge::DT_DUAL_SUB_UINT8, "ge::DT_DUAL_SUB_UINT8" },
        { ge::DT_QINT8, "ge::DT_QINT8" },
        { ge::DT_QINT16, "ge::DT_QINT16" },
        { ge::DT_QINT32, "ge::DT_QINT32" },
        { ge::DT_QUINT8, "ge::DT_QUINT8" },
        { ge::DT_QUINT16, "ge::DT_QUINT16" },
        { ge::DT_BF16, "ge::DT_BF16" }
    };
};
}
#endif
