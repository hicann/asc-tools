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
 * \file op_aclnn_generator.h
 * \brief
 */

#ifndef ACLNN_GENERATOR_H
#define ACLNN_GENERATOR_H

#include <fstream>
#include <cstring>
#include <unordered_set>
#include "op_generator.h"
#include "op_generator_factory.h"
#include "register/op_def.h"
#include "register/op_def_factory.h"
#include "op_build_error_codes.h"

namespace ops {
using namespace std;
constexpr const char* OP_ACLNN_ATTR_STR = "str";
constexpr const char* OP_ACLNN_ATTR_BOOL = "bool";
constexpr const char* OP_ACLNN_ATTR_FLOAT = "float";
constexpr const char* OP_ACLNN_ATTR_INT = "int";
constexpr const char* OP_ACLNN_ATTR_LISTBOOL = "listBool";
constexpr const char* OP_ACLNN_ATTR_LISTFLOAT = "listFloat";
constexpr const char* OP_ACLNN_ATTR_LISTINT = "listInt";
constexpr const char* OP_ACLNN_STRUCT_INFO = 
    "typedef struct {\n"
    "    uint32_t id;\n"
    "    const char *funcName;\n"
    "    bool hasReg;\n"
    "} NnopbaseDfxId;\n"
    "typedef struct {\n"
    "    ge::DataType dtype;\n"
    "    ge::Format format;\n"
    "} TensorDesc;\n"
    "typedef struct {\n"
    "    TensorDesc *inputsDesc;\n"
    "    size_t inputsNum;\n"
    "    TensorDesc *outputsDesc;\n"
    "    size_t outputsNum;\n"
    "} SupportInfo;\n"
    "typedef struct {\n"
    "    SupportInfo *supportInfo;\n"
    "    size_t num;\n"
    "} OpSocSupportInfo;\n"
    "typedef struct {\n"
    "    OpSocSupportInfo *socSupportInfo;\n"
    "    size_t num;\n"
    "} OpSupportList;\n";
constexpr const char* OP_ACLNN_SOC_INFO = 
    "enum SocType {\n"
    "    SOC_VERSION_ASCEND910A = 1,\n"
    "    SOC_VERSION_ASCEND910B = 2,\n"
    "    SOC_VERSION_ASCEND910_93 = 3,\n"
    "    SOC_VERSION_ASCEND950 = 4,\n"
    "    SOC_VERSION_ASCEND310P = 5,\n"
    "    SOC_VERSION_ASCEND310B = 6,\n"
    "    SOC_VERSION_BS9SX1A = 7,\n"
    "    SOC_VERSION_ASCEND610Lite = 8,\n"
    "    SOC_VERSION_MC61AM21A = 10, // 9 is deprecated\n"
    "    SOC_VERSION_MC62CM12A = 11,\n"
    "    SOC_VERSION_BS9SX2A = 12,\n"
    "    SOC_VERSION_ASCEND910_96 = 13,\n"
    "    SOC_VERSION_KIRINX90 = 14,\n"
    "    SOC_VERSION_KIRIN9030 = 15\n"
    "};\n";// NOTE: Always add new SOC versions at the end of this enum.
constexpr const char* OP_ACLNN_NNOPBASE_ATTR_DTYPE_INFO = 
    "enum NnopbaseAttrDtype {\n"
    "    kNnopbaseBool = 0U,\n"
    "    kNnopbaseFloat,\n"
    "    kNnopbaseInt,\n"
    "    kNnopbaseString,\n"
    "    kNnopbaseAttrEnd\n"
    "};\n";
constexpr const char *OP_ACLNN_EXTERN_FUNC = 
    "#ifdef __cplusplus\n"
    "extern \"C\" {\n"
    "#endif\n\n"
    "extern aclnnStatus NnopbaseCreateExecutorSpace(void **space);\n"
    "extern void *NnopbaseGetExecutor(void *space, const char *opType, char *inputsDesc, uint32_t inputNum,\n"
    "                                 char *outputsDesc, uint32_t outputNum, char *attrsDesc, uint32_t attrsNum);\n"
    "extern aclnnStatus NnopbaseAddInput(void *executor, const aclTensor *tensor, const uint32_t index);\n"
    "extern aclnnStatus NnopbaseAddIgnoreContinuesInput(void *executor,\n"
    "                                                   const aclTensor *tensor, const uint32_t index);\n"
    "extern aclnnStatus NnopbaseAddIntArrayInput(void *executor, const aclIntArray *array, const uint32_t index);\n"
    "extern aclnnStatus NnopbaseAddBoolArrayInput(void *executor, const aclBoolArray *array, "
    "const uint32_t index);\n"
    "extern aclnnStatus NnopbaseAddFloatArrayInput(void *executor, const aclFloatArray *array, "
    "const uint32_t index);\n"
    "extern aclnnStatus NnopbaseAddOutput(void *executor, const aclTensor *tensor, const uint32_t index);\n"
    "extern aclnnStatus NnopbaseAddDynamicInput(void *executor, const aclTensorList *tensor_list, "
    "const uint32_t index);\n"
    "extern aclnnStatus NnopbaseAddDynamicOutput(void *executor, const aclTensorList *tensor_list, "
    "const uint32_t index);\n"
    "extern aclnnStatus NnopbaseAddAttrWithDtype(void *executor, void *attrAddr, size_t attrLen, "
    "const size_t index, const NnopbaseAttrDtype dtype);\n"
    "extern aclnnStatus NnopbaseAddIntArrayAttr(void *executor, const aclIntArray* array, const size_t index);\n"
    "extern aclnnStatus NnopbaseAddFloatArrayAttr(void *executor, const aclFloatArray* array, "
    "const size_t index);\n"
    "extern aclnnStatus NnopbaseAddBoolArrayAttr(void *executor, const aclBoolArray* array, const size_t index);\n"
    "extern aclnnStatus NnopbaseAddArrayAttrWithDtype(void *executor, void *array, const size_t len, "
    "const size_t elementSize, const size_t index, const NnopbaseAttrDtype dtype);\n"
    "extern uint64_t NnopbaseMsprofSysTime();\n"
    "extern aclnnStatus NnopbaseAddTilingId(void *executor, NnopbaseDfxId *tilingId);\n"
    "extern void NnopbaseReportApiInfo(const uint64_t beginTime, NnopbaseDfxId &dfxId);\n"
    "extern aclnnStatus NnopbaseRunForWorkspace(void *executor, uint64_t *workspaceLen);\n"
    "extern aclnnStatus NnopbaseRunWithWorkspace(void *executor, aclrtStream stream, void *workspace, "
    "uint64_t workspaceSize);\n"
    "extern aclnnStatus NnopbaseAddSupportList(void *executor, OpSupportList *list, "
    "uint32_t *socSupportList, size_t socSupportListLen);\n"
    "extern aclnnStatus NnopbaseAddScalarInput(void *executor, const aclScalar *scalar, const uint32_t index, "
    "const int32_t srcIndex, const ge::DataType dtype);\n"
    "extern aclnnStatus NnopbaseAddScalarListInput(void *executor, const aclScalarList *scalarList, "
    "const uint32_t index, const int32_t srcIndex, const ge::DataType dtype);\n"
    "extern void NnopbaseAddOpTypeId(void *executor, const uint32_t opTypeId);\n"
    "extern aclnnStatus __attribute__((weak)) NnopbaseAddParamName(void *executor, const uint32_t index, "
    "const char *name, const bool isInput);\n"
    "extern aclnnStatus __attribute__((weak)) NnopbaseSetFormatMatchMode(void *executor, const uint32_t mode);\n"
    "extern aclnnStatus NnopbaseSetRef(void *executor, const size_t inputIrIdx, const size_t outputIrIdx);\n"
    "extern void __attribute__((weak)) NnopbaseSetMatchArgsFlag(void *executor);\n"
    "extern bool __attribute__((weak)) NnopbaseMatchArgs(void *executor, uint64_t *workspaceLen);\n";

constexpr const int32_t OP_ACLNN_ATTR_TYPE_STR = 0;
constexpr const int32_t OP_ACLNN_ATTR_TYPE_BOOL = 1;
constexpr const int32_t OP_ACLNN_ATTR_TYPE_FLOAT = 2;
constexpr const int32_t OP_ACLNN_ATTR_TYPE_INT = 3;
constexpr const int32_t OP_ACLNN_ATTR_TYPE_LISTBOOL = 4;
constexpr const int32_t OP_ACLNN_ATTR_TYPE_LISTFLOAT = 5;
constexpr const int32_t OP_ACLNN_ATTR_TYPE_LISTINT = 6;
constexpr const int32_t K_DIFF_NUM = 32;
constexpr const size_t OP_ACLNN_REF_SUFFIX_LEN = 3U;

const std::map<std::string, int32_t> ACLNN_OP_ATTR_TYPE_MAP = {
    { OP_ACLNN_ATTR_STR, OP_ACLNN_ATTR_TYPE_STR },
    { OP_ACLNN_ATTR_BOOL, OP_ACLNN_ATTR_TYPE_BOOL },
    { OP_ACLNN_ATTR_FLOAT, OP_ACLNN_ATTR_TYPE_FLOAT },
    { OP_ACLNN_ATTR_INT, OP_ACLNN_ATTR_TYPE_INT },
    { OP_ACLNN_ATTR_LISTBOOL, OP_ACLNN_ATTR_TYPE_LISTBOOL },
    { OP_ACLNN_ATTR_LISTFLOAT, OP_ACLNN_ATTR_TYPE_LISTFLOAT },
    { OP_ACLNN_ATTR_LISTINT, OP_ACLNN_ATTR_TYPE_LISTINT },
};

const std::map<enum HcclServerType, std::string> HCCL_SERVER_TYPE_MAP = {
    { HcclServerType::AICPU, "NNOPBASE_HCCL_SERVER_TYPE_AICPU" },
    { HcclServerType::AICORE, "NNOPBASE_HCCL_SERVER_TYPE_MTE" },
    { HcclServerType::CCU, "NNOPBASE_HCCL_SERVER_TYPE_CCU" },
    { HcclServerType::MAX, "NNOPBASE_HCCL_SERVER_TYPE_END" },
};

const std::map<std::string, std::string> SOC_SUPPORT_MAP = {
    { "ascend910", "SOC_VERSION_ASCEND910A" },
    { "ascend910b", "SOC_VERSION_ASCEND910B" },
    { "ascend910_93", "SOC_VERSION_ASCEND910_93" },
    { "ascend950", "SOC_VERSION_ASCEND950" },
    { "ascend310p", "SOC_VERSION_ASCEND310P" },
    { "ascend310b", "SOC_VERSION_ASCEND310B" },
    { "bs9sx1a", "SOC_VERSION_BS9SX1A" },
    { "bs9sx2a", "SOC_VERSION_BS9SX2A" },
    { "ascend610lite", "SOC_VERSION_ASCEND610Lite" },
    { "ascend910_55", "SOC_VERSION_ASCEND910_55" },
    { "mc61am21a", "SOC_VERSION_MC61AM21A" },
    { "mc62cm12a", "SOC_VERSION_MC62CM12A" },
    { "ascend910_96", "SOC_VERSION_ASCEND910_96"},
    { "kirinx90", "SOC_VERSION_KIRINX90"},
    { "kirin9030", "SOC_VERSION_KIRIN9030"}
};

const std::map<int, std::string> DTYPE_SUPPORT_MAP = {
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
    { ge::DT_BF16, "ge::DT_BF16" },
    { ge::DT_HIFLOAT8, "ge::DT_HIFLOAT8" },
    { ge::DT_FLOAT8_E5M2, "ge::DT_FLOAT8_E5M2" },
    { ge::DT_FLOAT8_E4M3FN, "ge::DT_FLOAT8_E4M3FN" },
    { ge::DT_FLOAT8_E8M0, "ge::DT_FLOAT8_E8M0" },
    { ge::DT_FLOAT6_E3M2, "ge::DT_FLOAT6_E3M2" },
    { ge::DT_FLOAT6_E2M3, "ge::DT_FLOAT6_E2M3" },
    { ge::DT_FLOAT4_E2M1, "ge::DT_FLOAT4_E2M1" },
    { ge::DT_FLOAT4_E1M2, "ge::DT_FLOAT4_E1M2" }
};

const std::map<int, std::string> FORMAT_SUPPORT_MAP = {
    { ge::FORMAT_NCHW, "ge::FORMAT_NCHW" },
    { ge::FORMAT_NHWC, "ge::FORMAT_NHWC" },
    { ge::FORMAT_ND, "ge::FORMAT_ND" },
    { ge::FORMAT_NC1HWC0, "ge::FORMAT_NC1HWC0" },
    { ge::FORMAT_FRACTAL_Z, "ge::FORMAT_FRACTAL_Z" },
    { ge::FORMAT_NC1C0HWPAD, "ge::FORMAT_NC1C0HWPAD" },
    { ge::FORMAT_NHWC1C0, "ge::FORMAT_NHWC1C0" },
    { ge::FORMAT_FSR_NCHW, "ge::FORMAT_FSR_NCHW" },
    { ge::FORMAT_FRACTAL_DECONV, "ge::FORMAT_FRACTAL_DECONV" },
    { ge::FORMAT_C1HWNC0, "ge::FORMAT_C1HWNC0" },
    { ge::FORMAT_FRACTAL_DECONV_TRANSPOSE, "ge::FORMAT_FRACTAL_DECONV_TRANSPOSE" },
    { ge::FORMAT_FRACTAL_DECONV_SP_STRIDE_TRANS, "ge::FORMAT_FRACTAL_DECONV_SP_STRIDE_TRANS" },
    { ge::FORMAT_NC1HWC0_C04, "ge::FORMAT_NC1HWC0_C04" },
    { ge::FORMAT_FRACTAL_Z_C04, "ge::FORMAT_FRACTAL_Z_C04" },
    { ge::FORMAT_CHWN, "ge::FORMAT_CHWN" },
    { ge::FORMAT_HWCN, "ge::FORMAT_HWCN" },
    { ge::FORMAT_FRACTAL_DECONV_SP_STRIDE8_TRANS, "ge::FORMAT_FRACTAL_DECONV_SP_STRIDE8_TRANS" },
    { ge::FORMAT_NC1KHKWHWC0, "ge::FORMAT_NC1KHKWHWC0" },
    { ge::FORMAT_BN_WEIGHT, "ge::FORMAT_BN_WEIGHT" },
    { ge::FORMAT_FILTER_HWCK, "ge::FORMAT_FILTER_HWCK" },
    { ge::FORMAT_MD, "ge::FORMAT_MD" },
    { ge::FORMAT_HASHTABLE_LOOKUP_LOOKUPS, "ge::FORMAT_HASHTABLE_LOOKUP_LOOKUPS" },
    { ge::FORMAT_HASHTABLE_LOOKUP_KEYS, "ge::FORMAT_HASHTABLE_LOOKUP_KEYS" },
    { ge::FORMAT_HASHTABLE_LOOKUP_VALUE, "ge::FORMAT_HASHTABLE_LOOKUP_VALUE" },
    { ge::FORMAT_HASHTABLE_LOOKUP_OUTPUT, "ge::FORMAT_HASHTABLE_LOOKUP_OUTPUT" },
    { ge::FORMAT_HASHTABLE_LOOKUP_HITS, "ge::FORMAT_HASHTABLE_LOOKUP_HITS" },
    { ge::FORMAT_C1HWNCoC0, "ge::FORMAT_C1HWNCoC0" },
    { ge::FORMAT_NDHWC, "ge::FORMAT_NDHWC" },
    { ge::FORMAT_FRACTAL_ZZ, "ge::FORMAT_FRACTAL_ZZ" },
    { ge::FORMAT_FRACTAL_NZ, "ge::FORMAT_FRACTAL_NZ" },
    { ge::FORMAT_NCDHW, "ge::FORMAT_NCDHW" },
    { ge::FORMAT_DHWCN, "ge::FORMAT_DHWCN" },
    { ge::FORMAT_NDC1HWC0, "ge::FORMAT_NDC1HWC0" },
    { ge::FORMAT_FRACTAL_Z_3D, "ge::FORMAT_FRACTAL_Z_3D" },
    { ge::FORMAT_CN, "ge::FORMAT_CN" },
    { ge::FORMAT_NC, "ge::FORMAT_NC" },
    { ge::FORMAT_DHWNC, "ge::FORMAT_DHWNC" },
    { ge::FORMAT_FRACTAL_Z_3D_TRANSPOSE, "ge::FORMAT_FRACTAL_Z_3D_TRANSPOSE" },
    { ge::FORMAT_FRACTAL_ZN_LSTM, "ge::FORMAT_FRACTAL_ZN_LSTM" },
    { ge::FORMAT_FRACTAL_Z_G, "ge::FORMAT_FRACTAL_Z_G" },
    { ge::FORMAT_RESERVED, "ge::FORMAT_RESERVED" },
    { ge::FORMAT_FRACTAL_ZN_RNN, "ge::FORMAT_FRACTAL_ZN_RNN" },
    { ge::FORMAT_NULL, "ge::FORMAT_NULL" },
    { ge::FORMAT_ALL, "ge::FORMAT_ALL" },
    { ge::FORMAT_ND_RNN_BIAS, "ge::FORMAT_ND_RNN_BIAS" },
    { ge::FORMAT_NYUV, "ge::FORMAT_NYUV" },
    { ge::FORMAT_NYUV_A, "ge::FORMAT_NYUV_A" },
    { ge::FORMAT_NCL, "ge::FORMAT_NCL" }
};

const std::unordered_set<ge::DataType> VALUE_DEPEND_SUPPORT_DTYPES = {
    ge::DT_FLOAT, ge::DT_BOOL, ge::DT_INT64, ge::DT_UINT64, ge::DT_INT32,
    ge::DT_UINT32, ge::DT_INT16, ge::DT_UINT16, ge::DT_INT8, ge::DT_UINT8
};

const std::unordered_set<ge::DataType> VALUE_DEPEND_SUPPORT_INT_DTYPES = {
    ge::DT_INT64, ge::DT_UINT64, ge::DT_INT32,ge::DT_UINT32, 
    ge::DT_INT16, ge::DT_UINT16, ge::DT_INT8, ge::DT_UINT8
};


struct OpDefName {
    std::vector<std::string> originInputName; // op_host配置的输入名称，scalar输入跟随指定参数做类型转换
    std::vector<std::string> inputsName; // 转换后的输入名称
    std::vector<std::string> outputsName; // 转换后的输出名称
    std::vector<std::string> attrsName; // 转换后的属性名称
    std::vector<std::string> defaultAttrsName; // 配置版本号时默认属性名称
    std::string prefixName; // 前缀名
    std::string maxVersionName; // 最大版本号前缀名
    std::string opName; // 算子名
    std::string decName; // include的头文件名称
    std::string macroNmae; // 宏名称
    std::string fileName; // 头文件名称
    std::string maxDecName; // 最大版本号宏名称
    bool hasOutputShapeDepend;
};

class AclnnOpGenerator : public Generator {
public:
    explicit AclnnOpGenerator(std::vector<std::string>& ops);
    void AclnnSetErrorMessage(std::string& str, const std::string opType) const;
    void AclnnOpGenHeaderFileDel(std::string& name, std::ofstream& outfile, bool isStart) const;
    std::ofstream AclnnOpGenHeaderFileStart(std::string& fileName, std::string& macroNmae, uint32_t version) const;
    bool AclnnOpGenFunProtoValueDependParam(
        OpDef& opDef, const OpDefName& opdefName, size_t paramIndex, ofstream& outfile, const std::string& opType) const;
    std::vector<std::vector<ge::DataType>> AclnnGetInputAndOutputDataTypeList(std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs) const;
    std::vector<std::string> AclnnGetInputAndOutputNames(const std::vector<OpParamDef>& inputs, const std::vector<OpParamDef>& outputs) const;
    std::vector<size_t> AclnnGetValueDependIntTypeIndex(std::vector<OpParamDef>& inputs) const;
    std::string AclnnBuildValueDependDataTypeErrorMessage(const std::vector<std::string>& paramOriginNames, const std::vector<ge::DataType>& originDataTypes, 
        const std::vector<ge::DataType>& requiredDataTypes, int valueDependIndex, const string& opType) const;
    bool AclnnCheckForInt64CombinationWithValueDepend(OpDef& opDef, size_t paramIndex, const std::string& opType) const;
    bool AclnnIsValueDependDataTypeSupport(std::vector<OpParamDef>& inputs, const std::string& opType) const;
    bool AclnnIsRefParam(const std::string& inputName) const;
    void AclnnOpGenFunProtoParam(const OpParamDef& param, const std::string& paramName, int32_t paramType, bool hasOutputShapeDepend, ofstream& outfile) const;
    void AclnnOpGenFunProtoInputParams(OpDef& opDef, OpDefName& opdefName, std::ofstream& outfile, const uint32_t version,
        const bool valDependApi) const;
    void AclnnOpGenFunProtoOutputParams(OpDef& opDef, OpDefName& opdefName, std::ofstream& outfile, const uint32_t version, 
        const bool valDependApi) const;
    void AclnnOpGenFunProtoAttrParamsImpl(
        OpAttrDef& attr, std::ofstream& outfile, std::string& name, const std::string opType) const;
    void AclnnOpGenFunProtoAttrParams(
        OpDef& opDef, std::vector<std::string>& paramNames, std::ofstream& outfile, uint32_t version) const;
    void AclnnOpGenValueDependInput(OpParamDef& input, std::string& name, size_t index, std::ofstream& outfile) const;
    bool AclOpGenScalarInput(OpParamDef& input, size_t index,
        OpDefName& opdefName, std::ofstream& outfile, std::string funcName) const;
    void AclnnOpGenCodeAddInputTensors(OpDef& opDef, OpDefName& opdefName, std::ofstream& outfile, bool valueDependApi) const;
    void AclnnOpGenCodeAddOutputShapeDependTensors(
        std::vector<OpParamDef>& outputs, std::vector<std::string>& name, std::ofstream& outfile) const;
    void AclnnOpGenCodeAddOutputTensors(std::vector<OpParamDef>& outputs, std::vector<std::string>& name,
        bool hasOutputShapeDepend, std::ofstream& outfile) const;
    void AclnnoOpGenCodeAttrValue(OpAttrDef& attr, size_t* len, std::ofstream& outfile) const;
    void AclnnOpGenCodeOptionalStrAttr(OpAttrDef& attr, std::string& name, size_t index, std::ofstream& outfile) const;
    void AclnnOpGenCodeOptionalBoolAttr(OpAttrDef& attr, std::string& name, size_t index, std::ofstream& outfile) const;
    void AclnnOpGenCodeOptionalFloatAttr(
        OpAttrDef& attr, std::string& name, size_t index, std::ofstream& outfile) const;
    void AclnnOpGenCodeOptionalIntAttr(OpAttrDef& attr, std::string& name, size_t index, std::ofstream& outfile) const;
    void AclnnOpGenCodeAttrParams(OpDef& opDef, std::vector<std::string>& name, std::ofstream& outfile,
        std::vector<int32_t>& attrTypes) const;
    void AclnnOpGenCodeFunIoTypeCommentImpl(const int32_t type, std::string& name, std::ofstream& outfile) const;
    void AclnnOpGenCodeFunInputComment(std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs,
        OpDefName& opdefName, std::ofstream& outfile, uint32_t version) const;
    void AclnnOpGenCodeFunOutputComment(std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs,
        std::vector<std::string>& paramName, bool *hasOutputShapeDepend, std::ofstream& outfile) const;
    void AclnnOpGenCodeFunAttrComment(std::vector<OpAttrDef>& attrs, std::vector<std::string>& paramName,
        std::ofstream& outfile, uint32_t version) const;
    void AclnnOpGenCodeRunForWSFunComment(OpDef& opDef, OpDefName& opdefName,
        std::ofstream& outfile, uint32_t version) const;
    void AclnnOpGenCodeRunWithWSFunComment(std::string& prefixName, std::ofstream& outfile) const;
    void AclnnOpGenCodeRunForWSFunProto(
        OpDef& opDef, OpDefName& opdefName, std::ofstream& outfile, uint32_t version) const;
    void AclnnOpGenCodeTensorRunForWSFunProto(
        OpDef& opDef, OpDefName& opdefName, ofstream& outfile, uint32_t version) const;
    void AclnnOpGenCodeRunWithWSFunProto(std::string& prefixName, std::ofstream& outfile) const;
    void AclnnOpGenCodeIoParamCheck(std::vector<OpParamDef>& param, std::vector<std::string>& name,
        std::ofstream& outfile, bool isInput) const;
    void AclnnOpGenCodeParamCheck(std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs,
        OpDefName& opdefName, std::ofstream& outfile) const;
    void AclnnGenCodeCommFunDelcare(std::ofstream& outfile) const;
    void AclnnOpGenCodeWorkspaceDelcare(
        OpDef& opDef, OpDefName& opdefName, std::ofstream& outfile, uint32_t version) const;
    void AclnnOpGenCodeIoParamDesc(std::vector<OpParamDef>& params, const std::string& desc,
        std::ofstream& outfile, uint32_t version) const;
    void AclnnOpGenCodeParamDesc(OpDef& opDef, std::ofstream& outfile, uint32_t version) const;
    void AclnnOpGenCodeExecutor(OpDef& opDef, std::ofstream& outfile) const;
    void AclnnOpGenSocSupportList(OpDef& opDef, std::ofstream& outfile) const;
    void AclnnOpGenHcclServerTypeList(OpDef& opDef, ofstream& outfile) const;
    void AclnnOpGenHcclServerType(OpDef& opDef, ofstream& outfile) const;
    void AclnnOpGenIoTensorDesc(
        size_t i, std::vector<OpParamDef>& params, std::ofstream& outfile, const std::string opType) const;
    void AclnnOpGenTensorDesc(size_t index, std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs,
        std::ofstream& outfile, const std::string opType) const;
    void AclnnOpGenOpSupportList(size_t index, std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs,
        std::ofstream& outfile, const std::string opType) const;
    void AclnnOpGenOpSupportListAll(OpDef& opDef, std::ofstream& outfile) const;
    void AclnnGenOpTypeId(OpDef& opDef, std::ofstream& outfile) const;
    void AclnnGenNameSpaceInfo(std::ofstream& outfile, OpDef& opDef) const;
    void AclnnGenCheckInfo(std::ofstream& outfile) const;
    bool IsSupportAutoContiguous(std::vector<OpParamDef>& inputs) const;
    void AclnnGenUncontDeclaration(OpDef& opDef, std::ofstream& outfile) const;
    void AclnnGenCodeDecImpl(std::string& declFile, std::ofstream& outfile) const;
    void AclnnGenCodeImplStart(std::string& declFile, bool hasOutputShapeDepend, std::ofstream& outfile, OpDef& opDef) const;
    void AclnnGenCodeImplEnd(std::ofstream& outfile) const;
    std::string AclnnOpGetIoSize(std::vector<OpParamDef>& params, std::ofstream& outfile) const;
    void AclopGenDfxInfo(OpDef& opDef, std::string& opName, std::string& prefixName, std::ofstream& outfile) const;
    void AclnnOpGenCodeSetUnContInfo(OpDef& opDef, std::ofstream& outfile) const;
    void AclopGenCodeCommon(OpDef& opDef, OpDefName& opdefName, std::ofstream& outfile, uint32_t version, bool valueDependApi) const;
    void AclnnOpGenIoParam(std::vector<OpParamDef>& params, std::vector<std::string>& paramName,
        uint32_t version, const bool isInput, std::ofstream& outfile) const;
    void AclnnOpGenAttrDefParam(std::vector<OpAttrDef>& attrs,
        std::vector<std::string>& paramName, std::ofstream& outfile) const;
    void AclnnOpGenDefaultArrayAttr(OpAttrDef& attr, std::string attrsName,
        std::vector<std::string>& defaultAttrsName, int32_t type, std::ofstream& outfile) const;
    void AclnnOpGenDefaultAttr(OpDef& opdef, OpDefName& opdefName, uint32_t version, std::ofstream& outfile) const;
    void AclnnOpGenCodeRunForWorkspaceVersionImpl(
        OpDef& opDef, OpDefName& opdefName, uint32_t version, uint32_t maxVersion, std::ofstream& outfile) const;
    void AclnnOpGenCodeRunForWorkspaceImpl(
        OpDef& opDef, OpDefName& opdefName, uint32_t version, std::ofstream& outfile, bool valDependApi) const;
    void AclnnOpGenCodeRunUnContWithWorkspaceImpl(OpDef& opDef, OpDefName& opDefName, std::ofstream& outfile) const;
    void AclnnOpGenCodeRunWithWorkspaceVersionImpl(OpDefName& opdefName, std::ofstream& outfile) const;
    void AclnnOpGenCodeRunWithWorkspaceImpl(OpDef& opDef, OpDefName& opDefName, std::ofstream& outfile) const;
    std::vector<std::string> AclnnOpGetEnvValue() const;
    bool GetInputConfigVerion(OpDef& opDef, std::set<uint32_t>& versions, uint32_t& maxVersion) const;
    bool GetAttrConfigVerion(OpDef& opDef, std::set<uint32_t>& versions, uint32_t& maxVersion) const;
    bool GetConfigVerion(OpDef& opDef, std::set<uint32_t>& versions, uint32_t& maxVersion) const;
    void AclopGenVersionCode(OpDef& opDef, OpDefName& opdefName, std::string prefixName, std::set<uint32_t> versions,
        uint32_t maxVersion) const;
    void AclnnOpGenCodeAttrParamDesc(std::vector<OpAttrDef>& attrs, const std::string& desc,
        std::ofstream& outfile, uint32_t version) const;
    std::string ToLower(std::string str) const;
    bool IsSupportProduct(OpDef& opDef) const;
    std::vector<std::string> Spilt(const std::string& str, const char delim) const;
    void AclnnGenMc2Declaration(OpDef& opDef, std::ofstream& outfile) const;
    bool AclnnOpCheckMC2Groups(
        std::vector<std::string>& name, std::set<ge::AscendString>& groups, const std::string& opType) const;
    void AclnnGenOutEmptyLaunchDeclaration(OpDef& opDef, ofstream& outfile) const;
    void AclnnOpGenCodeAttrParamsImpl(std::vector<OpAttrDef>& attrs, std::vector<std::string>& name, size_t index,
        int32_t type, std::ofstream& outfile) const;
    void AclnnOpGenCodeHcclGroup(
        OpDef& opDef, std::vector<std::string>& name, std::vector<int32_t> attrTypes, std::ofstream& outfile) const;
    void AclnnOpGenAddParamName(OpDef& opDef, const OpDefName& opdefName, std::ofstream& outfile) const;
    void AclnnAddDisableInputIndex(OpDef& opDef, uint32_t version, std::ofstream& outfile) const;

    bool HasRef(std::vector<std::string>& names) const;
    void AclnnOpGenCodeSetRef(std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs,
                              std::ofstream& outfile) const;
    void AclopGenCodeRefContiguous(OpDef& opDef, OpDefName& opdefName, std::ofstream& outfile) const;
    void AclnnOpGenCodeRunRefUnContWithWorkspaceImpl(OpDef& opDef, OpDefName& opDefName, std::ofstream& outfile) const;
    opbuild::Status GenerateCode(void) override;
    void AclnnGenExternFunc(std::ofstream& outfile) const;
    void AclnnOpGenFormatMode(OpDef& opDef, std::ofstream& outfile) const;
    bool IsBaseTypeOfAttr(const char *type) const;
    bool IsOpValueDepend(OpDef& opDef) const;
    ~AclnnOpGenerator() override = default;
    void AclOpGenMatchArgsFunc(ofstream& outfile) const;
};
} // namespace ops

#endif