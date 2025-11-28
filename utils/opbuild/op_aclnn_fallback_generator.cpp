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
 * \file op_aclnn_fallback_generator.cpp
 * \brief
 */

#include <sstream>
#include <algorithm>
#include "ascendc_tool_log.h"
#include "op_aclnn_generator.h"
#include "op_aclnn_fallback_generator.h"

namespace ops {
using namespace std;
void AclnnFallBackGenerator::AclnnGenFallBackImplStart(ofstream &outfile) const
{
    outfile << "#include <vector>\n";
    outfile << "#include \"aclnn/acl_meta.h\"\n";
    outfile << "#include \"exe_graph/runtime/op_execute_context.h\"\n";
    outfile << "#include \"exe_graph/runtime/tensor.h\"\n";
    outfile << "#include \"register/op_impl_registry.h\"\n\n";
    outfile << "extern void NnopbaseOpLogE(const aclnnStatus code, const char *const expr);\n\n";

    const char *str =
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n\n"
        "extern void* __attribute__((weak)) NnopbaseGetOpApiFunc(const char *funcName);\n"
        "extern aclTensor* __attribute__((weak)) NnopbaseConvertTensor(const gert::Tensor* tensor);\n"
        "extern aclTensorList* __attribute__((weak)) NnopbaseConvertTensorList(std::vector<const gert::Tensor*> "
        "&tenserList);\n"
        "extern aclBoolArray* __attribute__((weak)) NnopbaseCovertBoolArray(const gert::Tensor* tensor);\n"
        "extern aclIntArray* __attribute__((weak)) NnopbaseCovertIntArray(const gert::Tensor* tensor);\n"
        "extern aclFloatArray* __attribute__((weak)) NnopbaseCovertFloatArray(const gert::Tensor* tensor);\n"
        "extern aclScalar* __attribute__((weak)) NnopbaseConvertScalar(const gert::Tensor* tensor);\n"
        "extern aclScalarList* __attribute__((weak)) NnopbaseConvertScalarList(const gert::Tensor* tensor);\n"
        "extern aclIntArray* __attribute__((weak)) NnopbaseCovertIntArrayAttr(const "
        "gert::TypedContinuousVector<int64_t> *arr);\n"
        "extern aclBoolArray* __attribute__((weak)) NnopbaseCovertBoolArrayAttr(const "
        "gert::TypedContinuousVector<bool> *arr);\n"
        "extern aclFloatArray* __attribute__((weak)) NnopbaseCovertFloatArrayAttr(const "
        "gert::TypedContinuousVector<float> *arr);\n"
        "extern void __attribute__((weak)) NnopbaseDestroyTensor(const aclTensor *tensor);\n"
        "extern void __attribute__((weak)) NnopbaseDestroyTensorList(const aclTensorList *tensorList);\n"
        "extern void __attribute__((weak)) NnopbaseDestroyScalar(const aclScalar *scalar);\n"
        "extern void __attribute__((weak)) NnopbaseDestroyScalarList(const aclScalarList *scalar);\n"
        "extern void __attribute__((weak)) NnopbaseDestroyIntArray(const aclIntArray *array);\n"
        "extern void __attribute__((weak)) NnopbaseDestroyBoolArray(const aclBoolArray *array);\n"
        "extern void __attribute__((weak)) NnopbaseDestroyFloatArray(const aclFloatArray *array);\n";
    outfile << str;
}

void AclnnFallBackGenerator::AclnnGenGetInputTensor(std::vector<OpParamDef> &param, ofstream &outfile) const
{
    for (size_t i = 0U; i < param.size(); i++) {
        int32_t type = param[i].GetParamType();
        const char *name = param[i].GetParamName().GetString();
        if (type == DYNAMIC) {
            outfile << "    size_t index_" << i << " = 0U;\n";
            outfile << "    std::vector<const gert::Tensor*> " << name << ";\n";
            outfile << "    do {\n";
            outfile << "        auto val = host_api_ctx->GetDynamicInputTensor(" << i << ", index_" << i << ");\n";
            outfile << "        if (val == nullptr) {break;}\n";
            outfile << "        " << name << ".push_back(val);\n";
            outfile << "        index_" << i << "++;\n";
            outfile << "    } while (true);\n\n";
        } else if (type == REQUIRED) {
            outfile << "    auto " << name << " = host_api_ctx->GetRequiredInputTensor(" << i << ");\n";
            outfile << "    FALLBACK_ASSERT_NOTNULL_RETVAL(" << name << ");\n";
        } else {
            outfile << "    auto " << name << " = host_api_ctx->GetOptionalInputTensor(" << i << ");\n";
        }
    }
    outfile << "\n";
}

void AclnnFallBackGenerator::AclnnGenGetOutputTensor(
    std::vector<OpParamDef> &param, const std::vector<size_t> &refIndex, ofstream &outfile) const
{
    for (size_t i = 0U; i < param.size(); i++) {
        if (std::find(refIndex.begin(), refIndex.end(), i) == refIndex.end()) {
            int32_t type = param[i].GetParamType();
            const char *name = param[i].GetParamName().GetString();
            if (type == DYNAMIC) {
                outfile << "    size_t outIndex_" << i << " = 0U;\n";
                outfile << "    std::vector<const gert::Tensor*> " << name << ";\n";
                outfile << "    do {\n";
                outfile << "        auto var = host_api_ctx->GetDynamicOutputTensor(" << i << ", outIndex_" << i
                        << ");\n";
                outfile << "        if (val == nullptr) {break;}\n";
                outfile << "        " << name << ".push_back(val);\n";
                outfile << "        outIndex_" << i << "++;\n";
                outfile << "    } while (true);\n\n";
            } else if (type == REQUIRED) {
                outfile << "    auto " << name << " = host_api_ctx->GetRequiredOutputTensor(" << i << ");\n";
                outfile << "    FALLBACK_ASSERT_NOTNULL_RETVAL(" << name << ");\n";
            } else {
                Generator::SetErrorMessage("Not support optional output.");
                break;
            }
        }
    }
    outfile << "\n";
}

void AclnnFallBackGenerator::AclnnGenGetAttrsImpl(std::string &name, int32_t type, ofstream &outfile) const
{
    switch (type) {
        case OP_ACLNN_ATTR_TYPE_FLOAT: {
            outfile << "    const float *" << name << " = attrs->GetAttrPointer<float>(attrIndex++);\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_STR: {
            outfile << "    const char *" << name << " = attrs->GetAttrPointer<char>(attrIndex++);\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_BOOL: {
            outfile << "    const bool *" << name << " = attrs->GetAttrPointer<bool>(attrIndex++);\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTBOOL: {
            outfile << "    const gert::TypedContinuousVector<bool> *" << name
                    << " = attrs->GetAttrPointer<gert::TypedContinuousVector<bool>>(attrIndex++);\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTFLOAT: {
            outfile << "    const gert::TypedContinuousVector<float> *" << name
                    << " = attrs->GetAttrPointer<gert::TypedContinuousVector<float>>(attrIndex++);\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTINT: {
            outfile << "    const gert::TypedContinuousVector<int64_t> *" << name
                    << " = attrs->GetAttrPointer<gert::TypedContinuousVector<int64_t>>(attrIndex++);\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_INT: {
            outfile << "    const int64_t *" << name << " = attrs->GetAttrPointer<int64_t>(attrIndex++);\n";
            break;
        }
        default: {
            break;
        }
    }
}

void AclnnFallBackGenerator::AclnnGenGetAttrs(std::vector<OpAttrDef> &attrs, ofstream &outfile) const
{
    if (attrs.size() > 0U) {
        outfile << "    auto attrs = host_api_ctx->GetAttrs();\n";
        outfile << "    FALLBACK_ASSERT_NOTNULL_RETVAL(attrs);\n";
        outfile << "    size_t attrIndex = 0U;\n";
        for (size_t i = 0U; i < attrs.size(); i++) {
            std::string name = std::string(attrs[i].GetName().GetString());
            auto iter = ACLNN_OP_ATTR_TYPE_MAP.find(attrs[i].GetCfgDataType().GetString());
            if (iter == ACLNN_OP_ATTR_TYPE_MAP.end()) {
                Generator::SetErrorMessage("Data type of attr " + name + "is not support.");
                return;
            }
            int32_t type = iter->second;
            AclnnGenGetAttrsImpl(name, type, outfile);
            if (attrs[i].IsRequired()) {
                outfile << "    FALLBACK_ASSERT_NOTNULL_RETVAL(" << name << ");\n";
            }
        }
        outfile << "\n";
    }
}

void AclnnFallBackGenerator::AclnnGenValueDependTensor(OpParamDef &param, ofstream &outfile) const
{
    ge::DataType dataType = param.GetDataTypes()[0];
    const char *name = param.GetParamName().GetString();
    if (dataType == ge::DT_FLOAT) {
        outfile << "    aclFloatArray *" << name << "_tensor = NnopbaseCovertFloatArray(" << name << ");\n";
    } else if (dataType == ge::DT_INT64) {
        outfile << "    aclIntArray *" << name << "_tensor = NnopbaseCovertIntArray(" << name << ");\n";
    } else {
        outfile << "    aclBoolArray *" << name << "_tensor = NnopbaseCovertBoolArray(" << name << ");\n";
    }
}

void AclnnFallBackGenerator::GetHostInputs(std::vector<size_t> &hostInputs, bool isInput, size_t index) const
{
    if (isInput) {
        hostInputs.push_back(index);
    }
}

void AclnnFallBackGenerator::AclnnGenCovertTensor(std::vector<OpParamDef> &param, ofstream &outfile,
    std::vector<size_t> &hostInputs, const std::vector<size_t> refIndex, bool isInput) const
{
    for (size_t i = 0U; i < param.size(); i++) {
        if ((!isInput) &&(std::find(refIndex.begin(), refIndex.end(), i) != refIndex.end())) {
            continue;
        }
        const char *name = param[i].GetParamName().GetString();
        if (std::string(param[i].GetValueDepend().GetString()) != "") {
            GetHostInputs(hostInputs, isInput, i);
            AclnnGenValueDependTensor(param[i], outfile);
        } else if (param[i].IsScalar()) {
            GetHostInputs(hostInputs, isInput, i);
            outfile << "    aclScalar *" << name << "_scalar = NnopbaseConvertScalar(" << name << ");\n";
        } else if (param[i].IsScalarList()) {
            GetHostInputs(hostInputs, isInput, i);
            outfile << "    aclScalarList *" << name << "_scalarList = NnopbaseConvertScalarList(" << name << ");\n";
        } else if (param[i].GetParamType() == DYNAMIC) {
            outfile << "    aclTensorList *" << name << "_tensorList = NnopbaseConvertTensorList(" << name << ");\n";
        } else {
            outfile << "    aclTensor *" << name << "_tensor = NnopbaseConvertTensor(" << name << ");\n";
        }
    }
}

void AclnnFallBackGenerator::AclnnGenCovertAttr(std::vector<OpAttrDef> &attrs, ofstream &outfile) const
{
    for (size_t i = 0U; i < attrs.size(); i++) {
        std::string name = std::string(attrs[i].GetName().GetString());
        auto iter = ACLNN_OP_ATTR_TYPE_MAP.find(attrs[i].GetCfgDataType().GetString());
        if (iter == ACLNN_OP_ATTR_TYPE_MAP.end()) {
            Generator::SetErrorMessage("Data type of attr " + name + "is not support.");
            return;
        }
        int32_t type = iter->second;
        if (type == OP_ACLNN_ATTR_TYPE_LISTBOOL) {
            outfile << "    aclBoolArray *" << name << "_attr = NnopbaseCovertBoolArrayAttr(" << name << ");\n";
        } else if (type == OP_ACLNN_ATTR_TYPE_LISTFLOAT) {
            outfile << "    aclFloatArray *" << name << "_attr = NnopbaseCovertFloatArrayAttr(" << name << ");\n";
        } else if (type == OP_ACLNN_ATTR_TYPE_LISTINT) {
            outfile << "    aclIntArray *" << name << "_attr = NnopbaseCovertIntArrayAttr(" << name << ");\n";
        }
    }
}

void AclnnFallBackGenerator::AclnnGenGetOpApiFuncHandle(OpDef &opDef, ofstream &outfile) const
{
    const string opName = opDef.GetOpType().GetString();
    outfile << "    if (NnopbaseGetOpApiFunc == NULL) {return ge::GRAPH_FAILED;}\n";
    outfile << "    static AclnnGetWorkspaceSizeFunc aclnn" << opName << "GetWorkspaceSize = "
               "(AclnnGetWorkspaceSizeFunc)NnopbaseGetOpApiFunc(\"aclnn"
            << opName << "GetWorkspaceSize\");\n";
    outfile << "    FALLBACK_ASSERT_NOTNULL_RETVAL(aclnn" << opName << "GetWorkspaceSize);\n";
    outfile << "    static AclnnFunc aclnn" << opName << "= (AclnnFunc)NnopbaseGetOpApiFunc(\"aclnn" << opName << "\");\n";
    outfile << "    FALLBACK_ASSERT_NOTNULL_RETVAL(aclnn" << opName << ");\n";
}

void AclnnFallBackGenerator::AclnnGenOpapiFuncIoName(
    std::vector<OpParamDef> &param, const std::vector<size_t> refIndex, bool isOutput, ofstream &outfile) const
{
    for (size_t i = 0U; i < param.size(); i++) {
        if ((isOutput) &&(std::find(refIndex.begin(), refIndex.end(), i) != refIndex.end())) {
            continue;
        }
        const char *name = param[i].GetParamName().GetString();
        if (std::string(param[i].GetValueDepend().GetString()) != "") {
            outfile << name << "_tensor, ";
        } else if (param[i].IsScalar()) {
            outfile << name << "_scalar, ";
        } else if (param[i].IsScalarList()) {
            outfile << name << "_scalarList, ";
        } else if (param[i].GetParamType() == DYNAMIC) {
            outfile << name << "_tensorList, ";
        } else {
            outfile << name << "_tensor, ";
        }
    }
}

void AclnnFallBackGenerator::AclnnGenOpapiFuncAttrName(std::vector<OpAttrDef> &attrs, ofstream &outfile) const
{
    for (size_t i = 0U; i < attrs.size(); i++) {
        std::string name = std::string(attrs[i].GetName().GetString());
        auto iter = ACLNN_OP_ATTR_TYPE_MAP.find(attrs[i].GetCfgDataType().GetString());
        if (iter == ACLNN_OP_ATTR_TYPE_MAP.end()) {
            std::string str = "Data type of attr " + name + "is not support.";
            Generator::SetErrorMessage(str);
            return;
        }
        int32_t type = iter->second;
        if ((type == OP_ACLNN_ATTR_TYPE_LISTBOOL) || (type == OP_ACLNN_ATTR_TYPE_LISTFLOAT) ||
            (type == OP_ACLNN_ATTR_TYPE_LISTINT)) {
            outfile << name << "_attr, ";
        } else if (type == OP_ACLNN_ATTR_TYPE_STR) {
            outfile << "const_cast<char *>(" << name << "), ";
        } else {
            outfile << "*" << name << ", ";
        }
    }
}

void AclnnFallBackGenerator::AclnnGenCallGetWorkspaceSizeFunc(
    OpDef &opDef, const std::vector<size_t> refIndex, ofstream &outfile) const
{
    outfile << "\n    uint64_t workspaceSize = 0;\n";
    outfile << "    aclOpExecutor *executor = nullptr;\n";
    const string opName = opDef.GetOpType().GetString();
    outfile << "    auto ret = aclnn" << opName << "GetWorkspaceSize(";
    AclnnGenOpapiFuncIoName(opDef.GetInputs(), refIndex, false, outfile);
    AclnnGenOpapiFuncAttrName(opDef.GetAttrs(), outfile);
    AclnnGenOpapiFuncIoName(opDef.GetOutputs(), refIndex, true, outfile);
    outfile << "&workspaceSize, &executor);\n";
    outfile << "    FALLBACK_ASSERT_OK_RETVAL(ret);\n";
}

void AclnnFallBackGenerator::AclnnDestroyValueDependTensor(OpParamDef &param, ofstream &outfile) const
{
    ge::DataType dataType = param.GetDataTypes()[0];
    const char *name = param.GetParamName().GetString();
    if (dataType == ge::DT_FLOAT) {
        outfile << "    NnopbaseDestroyFloatArray(" << name << "_tensor);\n";
    } else if (dataType == ge::DT_INT64) {
        outfile << "    NnopbaseDestroyIntArray(" << name << "_tensor);\n";
    } else {
        outfile << "    NnopbaseDestroyBoolArray(" << name << "_tensor);\n";
    }
}

void AclnnFallBackGenerator::AclnnGenDestroyImpl(
    std::vector<OpParamDef> &param, ofstream &outfile, const std::vector<size_t> &refIndex, bool isInput) const
{
    for (size_t i = 0U; i < param.size(); i++) {
        if ((!isInput) &&(std::find(refIndex.begin(), refIndex.end(), i) != refIndex.end())) {
            continue;
        }
        const char *name = param[i].GetParamName().GetString();
        if (std::string(param[i].GetValueDepend().GetString()) != "") {
            AclnnDestroyValueDependTensor(param[i], outfile);
        } else if (param[i].IsScalar()) {
            outfile << "    NnopbaseDestroyScalar(" << name << "_scalar);\n";
        } else if (param[i].IsScalarList()) {
            outfile << "    NnopbaseDestroyScalarList(" << name << "_scalarList);\n";
        } else if (param[i].GetParamType() == DYNAMIC) {
            outfile << "    NnopbaseDestroyTensorList(" << name << "_tensorList);\n";
        } else {
            outfile << "    NnopbaseDestroyTensor(" << name << "_tensor);\n";
        }
    }
}

void AclnnFallBackGenerator::AclnnGenDestroyAttr(std::vector<OpAttrDef> &attrs, ofstream &outfile) const
{
    for (size_t i = 0U; i < attrs.size(); i++) {
        std::string name = std::string(attrs[i].GetName().GetString());
        auto iter = ACLNN_OP_ATTR_TYPE_MAP.find(attrs[i].GetCfgDataType().GetString());
        if (iter != ACLNN_OP_ATTR_TYPE_MAP.end()) {
            int32_t type = iter->second;
            if (type == OP_ACLNN_ATTR_TYPE_LISTBOOL) {
                outfile << "    NnopbaseCovertBoolArrayAttr(" << name << "_attr);\n";
            } else if (type == OP_ACLNN_ATTR_TYPE_LISTFLOAT) {
                outfile << "    NnopbaseDestroyFloatArray(" << name << "_attr);\n";
            } else if (type == OP_ACLNN_ATTR_TYPE_LISTINT) {
                outfile << "    NnopbaseDestroyIntArray(" << name << "_attr);\n";
            }
        }
    }
}

void AclnnFallBackGenerator::AclnnGenCallApiFunc(OpDef &opDef, ofstream &outfile, const std::vector<size_t> &refIndex) const
{
    outfile << "    void *workspace = nullptr;\n";
    outfile << "    if (workspaceSize > 0) {\n";
    outfile << "        workspace = host_api_ctx->MallocWorkspace(workspaceSize);\n";
    outfile << "        FALLBACK_ASSERT_NOTNULL_RETVAL(workspace);\n";
    outfile << "    }\n";
    outfile << "    auto stream = host_api_ctx->GetStream();\n";
    const string opName = opDef.GetOpType().GetString();
    outfile << "    ret = aclnn" << opName << "(workspace, workspaceSize, executor, stream);\n";
    outfile << "    FALLBACK_ASSERT_OK_RETVAL(ret);\n";
    AclnnGenDestroyImpl(opDef.GetInputs(), outfile, refIndex, true);
    AclnnGenDestroyImpl(opDef.GetOutputs(), outfile, refIndex, false);
    AclnnGenDestroyAttr(opDef.GetAttrs(), outfile);
    outfile << "    host_api_ctx->FreeWorkspace();\n";
    outfile << "    return ge::GRAPH_SUCCESS;\n";
}

void AclnnFallBackGenerator::AclnnGenFallBackImplFunc(
    OpDef &opDef, ofstream &outfile, std::vector<size_t> &hostInputs, const std::vector<size_t> &refIndex) const
{
    AclnnGenGetInputTensor(opDef.GetInputs(), outfile);
    AclnnGenGetOutputTensor(opDef.GetOutputs(), refIndex, outfile);
    AclnnGenGetAttrs(opDef.GetAttrs(), outfile);
    AclnnGenGetOpApiFuncHandle(opDef, outfile);
    AclnnGenCovertTensor(opDef.GetInputs(), outfile, hostInputs, refIndex, true);
    AclnnGenCovertTensor(opDef.GetOutputs(), outfile, hostInputs, refIndex, false);
    AclnnGenCovertAttr(opDef.GetAttrs(), outfile);
    AclnnGenCallGetWorkspaceSizeFunc(opDef, refIndex, outfile);
    AclnnGenCallApiFunc(opDef, outfile, refIndex);
}

void AclnnFallBackGenerator::AclnnGenValueDependIoFunc(OpParamDef param, ofstream &outfile) const
{
    std::vector<ge::DataType> dataTypes = param.GetDataTypes();
    if (dataTypes.size() > 0U) {
        if (dataTypes[0] == ge::DT_BOOL) {
            outfile << "aclBoolArray *, ";
        } else if (dataTypes[0] == ge::DT_INT64) {
            outfile << "aclIntArray *, ";
        } else {
            outfile << "aclFloatArray *, ";
        }
    }
}

bool AclnnFallBackGenerator::IsRef(
    std::vector<OpParamDef> &outputs, ge::AscendString name, std::vector<size_t> &refIndex) const
{
    for (size_t j = 0U; j < outputs.size(); j++) {
        if (name == outputs[j].GetParamName()) {
            refIndex.push_back(j);
            return true;
        }
    }
    return false;
}

void AclnnFallBackGenerator::AclnnGenFallBackInputsFunc(
    OpDef &opDef, std::vector<size_t> &refIndex, ofstream &outfile) const
{
    std::vector<OpParamDef> &inputs = opDef.GetInputs();
    std::vector<OpParamDef> &outputs = opDef.GetOutputs();
    for (size_t i = 0U; i < inputs.size(); i++) {
        bool isRef = IsRef(outputs, inputs[i].GetParamName(), refIndex);
        if (!isRef) {
            outfile << "const ";
        }
        int32_t type = inputs[i].GetParamType();
        const char *const valueDepend = inputs[i].GetValueDepend().GetString();
        if ((std::string(valueDepend) == "required") || (std::string(valueDepend) == "optional")) {
            AclnnGenValueDependIoFunc(inputs[i], outfile);
        } else if (inputs[i].IsScalar()) {
            outfile << "aclScalar *, ";
        } else if (inputs[i].IsScalarList()) {
            outfile << "aclScalarList *, ";
        } else if (type != DYNAMIC) {
            outfile << "aclTensor *, ";
        } else {
            outfile << "aclTensorList *, ";
        }
    }
}

void AclnnFallBackGenerator::HasOutputShapeDepend(std::vector<OpParamDef> &outputs, bool &hasOutputShapeDepend) const
{
    for (size_t i = 0U; i < outputs.size(); i++) {
        if (outputs[i].IsOutputShapeDependOnCompute()) {
            hasOutputShapeDepend = true;
        }
    }
}

void AclnnFallBackGenerator::AclnnGenFallBackOutputsFunc(
    OpDef &opDef, std::vector<size_t> &refIndex, ofstream &outfile) const
{
    std::vector<OpParamDef> &outputs = opDef.GetOutputs();
    bool hasOutputShapeDepend = false;
    HasOutputShapeDepend(outputs, hasOutputShapeDepend);
    for (size_t i = 0U; i < outputs.size(); i++) {
        if (std::find(refIndex.begin(), refIndex.end(), i) == refIndex.end()) {
            if (!hasOutputShapeDepend) {
                outfile << "const ";
            }
            int32_t type = outputs[i].GetParamType();
            const char *const valueDepend = outputs[i].GetValueDepend().GetString();
            if ((std::string(valueDepend) == "required") || (std::string(valueDepend) == "optional")) {
                AclnnGenValueDependIoFunc(outputs[i], outfile);
            } else if (outputs[i].IsScalar()) {
                outfile << "aclScalar *, ";
            } else if (outputs[i].IsScalarList()) {
                outfile << "aclScalarList *, ";
            } else if (type != DYNAMIC) {
                outfile << "aclTensor *, ";
            } else {
                outfile << "aclTensorList *, ";
            }
        }
    }
}

void AclnnFallBackGenerator::AclnnGenFallBackAttrFuncImpl(
    const OpAttrDef &attr, const int32_t type, ofstream &outfile) const
{
    switch (type) {
        case OP_ACLNN_ATTR_TYPE_STR: {
            outfile << "char *, ";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_BOOL: {
            outfile << "bool, ";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTBOOL: {
            outfile << "const aclBoolArray *, ";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_FLOAT: {
            outfile << "double, ";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTFLOAT: {
            outfile << "const aclFloatArray *, ";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_INT: {
            outfile << "int64_t, ";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTINT: {
            outfile << "const aclIntArray *, ";
            break;
        }
        default:
            std::cerr << "[Error]: not support attr dtype " << attr.GetCfgDataType().GetString() << "!" << std::endl;
            ASCENDLOGE("not support attr dtype %s!", attr.GetCfgDataType().GetString());
            break;
    }
}

void AclnnFallBackGenerator::AclnnGenFallBackAttrFunc(std::vector<OpAttrDef> &attrs, ofstream &outfile) const
{
    for (size_t i = 0U; i < attrs.size(); i++) {
        auto iter = ACLNN_OP_ATTR_TYPE_MAP.find(attrs[i].GetCfgDataType().GetString());
        if (iter == ACLNN_OP_ATTR_TYPE_MAP.end()) {
            std::string name = std::string(attrs[i].GetName().GetString());
            std::string str = "Data type of attr " + name + "is not support.";
            Generator::SetErrorMessage(str);
            return;
        }
        int32_t type = iter->second;
        AclnnGenFallBackAttrFuncImpl(attrs[i], type, outfile);
    }
}

void AclnnFallBackGenerator::AclnnGenFallBackGetWorkspaceSizeFunc(
    OpDef &opDef, ofstream &outfile, std::vector<size_t> &refIndex) const
{
    outfile << "using AclnnGetWorkspaceSizeFunc = aclnnStatus (*)(";
    AclnnGenFallBackInputsFunc(opDef, refIndex, outfile);
    AclnnGenFallBackAttrFunc(opDef.GetAttrs(), outfile);
    AclnnGenFallBackOutputsFunc(opDef, refIndex, outfile);
    outfile << "uint64_t *, aclOpExecutor **);\n";
}

void AclnnFallBackGenerator::AclnnGenHostInputs(std::vector<size_t> hostInputs, ofstream &outfile) const
{
    outfile << ".HostInputs({";
    for (size_t i = 0U; i < hostInputs.size(); i++) {
        outfile << hostInputs[i];
        if (i != hostInputs.size() - 1) {
            outfile << ", ";
        }
    }
    outfile << "})";
}

void AclnnFallBackGenerator::AclnnGenFallbackCheckInfo(ofstream &outfile) const
{
    const char* str = "#define ACLNN_SUCCESS  0\n"
        "#define ACLNN_ERR_PARAM_NULLPTR 161001\n\n"
        "#define FALLBACK_ASSERT_OK_RETVAL(v)                                    \\\n"
        "    do {                                                                \\\n"
        "        const aclnnStatus _chk_stutus = (v);                            \\\n"
        "        if (_chk_stutus != ACLNN_SUCCESS) {                             \\\n"
        "            NnopbaseOpLogE(_chk_stutus, #v);                            \\\n"
        "            return ge::GRAPH_FAILED;                                     \\\n"
        "        }                                                               \\\n"
        "    } while (false)\n"
        "\n"
        "#define FALLBACK_ASSERT_NOTNULL_RETVAL(v)                               \\\n"
        "    do {                                                                \\\n"
        "        if ((v) == nullptr) {                                           \\\n"
        "            NnopbaseOpLogE(ACLNN_ERR_PARAM_NULLPTR, #v \" != nullptr\");  \\\n"
        "            return ge::GRAPH_FAILED;                                    \\\n"
        "        }                                                               \\\n"
        "    } while (false)\n"
        "\n";
    outfile << str;
}

void AclnnFallBackGenerator::AclnnGenFallBackImpl(OpDef &opDef, ofstream &outfile) const
{
    AclnnGenFallBackImplStart(outfile);
    std::vector<size_t> refIndex;
    AclnnGenFallBackGetWorkspaceSizeFunc(opDef, outfile, refIndex);
    outfile << "using AclnnFunc = aclnnStatus (*)(void *, uint64_t, aclOpExecutor *, aclrtStream);\n\n";
    AclnnGenFallbackCheckInfo(outfile);
    outfile << "namespace fallback {\n";
    const string opName = opDef.GetOpType().GetString();
    outfile << "ge::graphStatus " << opName << "HostExecuteFunc(gert::OpExecuteContext* host_api_ctx) {\n";
    outfile << "    FALLBACK_ASSERT_NOTNULL_RETVAL(host_api_ctx);\n";
    std::vector<size_t> hostInputs;
    AclnnGenFallBackImplFunc(opDef, outfile, hostInputs, refIndex);
    outfile << "}\n\n";
    outfile << "IMPL_OP(" << opName << ").OpExecuteFunc(" << opName << "HostExecuteFunc)";
    if (!hostInputs.empty()) {
        AclnnGenHostInputs(hostInputs, outfile);
    }
    outfile << ";\n} // namespace fallback\n\n";
    outfile << "#ifdef __cplusplus\n}\n#endif\n";
}

void AclnnFallBackGenerator::AclnnGenFallBack(OpDef &opDef) const
{
    const string opName = opDef.GetOpType().GetString();
    const string fileName = ConvertToSnakeCase(opName);
    std::string genPath;
    Generator::GetGenPath(genPath);
    const string implFile = genPath + "/fallback_" + fileName + ".cpp";
    ofstream outfile = ofstream(implFile);
    chmod(implFile.c_str(), S_IRUSR | S_IWUSR);
    AclnnGenFallBackImpl(opDef, outfile);
    outfile.close();
}

opbuild::Status AclnnFallBackGenerator::GenerateCode(void)
{
    ASCENDLOGI("Aclnn Fallback GenerateCode called!");
    std::vector<std::string> ops = this->GetAllOp();
    for (const auto& op : ops) {
        OpDef opDef = OpDefFactory::OpDefCreate(op.c_str());
        if (opDef.IsEnableFallBack()) {
            AclnnGenFallBack(opDef);
        }
    }
    ASCENDLOGI("Aclnn Fallback GenerateCode end!");
    return opbuild::OPBUILD_SUCCESS;
}

AclnnFallBackGenerator::AclnnFallBackGenerator(std::vector<std::string> &ops) : Generator(ops)
{
    ASCENDLOGI("Aclnn fallback Generator construct!");
}

static opbuild::Status AclnnFallBackGeneratorBuilder(std::vector<std::string> &ops)
{
    AclnnFallBackGenerator g(ops);
    return g.GenerateCode();
}

static void GeneratorAclnnFallBack(void) __attribute__((constructor));
void GeneratorAclnnFallBack(void)
{
    GeneratorFactory::AddBuilder("fallback", AclnnFallBackGeneratorBuilder);
}
}  // namespace ops