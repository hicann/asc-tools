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
 * \file op_aclnn_generator.cpp
 * \brief
 */

#include "op_aclnn_generator.h"
#include <algorithm>
#include <mutex>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include "ascendc_tool_log.h"
#include "op_build_params.h"

namespace {
using namespace std;
/**
* 输入输出属性名称转成小驼峰命名
*/
std::string ConvertName(const char* s)
{
    string str(s);
    string result = "";
    result = std::tolower(str[0]);
    for (size_t i = 1U; i < str.length();) {
        if (str[i] == '_') {
            i++;
            result += toupper(str[i]);
        } else {
            result += str[i];
        }
        i++;
    }
    return result;
}

void Split(const std::string& str, const char delimiter, std::vector<std::string>& result)
{
    std::stringstream ss(str);
    std::string tmp;
    while (std::getline(ss, tmp, delimiter)) {
        result.push_back(tmp);
    }
}

bool AreAllInputDataTypesSame(const std::vector<ge::DataType>& inputDataTypes)
{
    for (size_t i = 1U; i < inputDataTypes.size(); ++i) {
        if (inputDataTypes[i] != inputDataTypes[0]) {
            return false;
        }
    }
    return true;
}

bool AreInputDataTypesSupported(const std::unordered_set<ge::DataType>& supportDateTypeSet, const std::vector<ge::DataType>& inputDataTypes)
{
    for (size_t i = 0U; i < inputDataTypes.size(); ++i) {
        if (supportDateTypeSet.find(inputDataTypes[i]) == supportDateTypeSet.cend()) {
            return false;
        }
    }
    return true;
}
} // Anonymous Namespace

namespace ops {
using namespace std;

void AclnnOpGenerator::AclnnSetErrorMessage(std::string& str, const std::string opType) const
{
    str.append(" of op ");
    str.append(opType);
    str.append(" is not support.");
    Generator::SetErrorMessage(str);
}

void AclnnOpGenerator::AclnnOpGenHeaderFileDel(string& name, ofstream& outfile, bool isStart) const
{
    if (isStart) {
        outfile << "#ifndef " << name;
        outfile << "#define " << name;
        return;
    }
    outfile << "#ifdef __cplusplus\n}\n#endif\n\n#endif\n";
    outfile.close();
}

ofstream AclnnOpGenerator::AclnnOpGenHeaderFileStart(string& fileName, string& macroNmae, uint32_t version) const
{
    ofstream outfile(fileName);
    chmod(fileName.c_str(), S_IRUSR | S_IWUSR);
    outfile << "\n/*\n * calution: this file was generated automaticlly donot change it.\n*/\n\n";
    std::string name = GenerateMacros(macroNmae);
    if (version != 0U) {
        string decName = name + "_V" + std::to_string(version) + "_H_\n";
        AclnnOpGenHeaderFileDel(decName, outfile, true);
    } else {
        string decName = name + "_H_\n";
        AclnnOpGenHeaderFileDel(decName, outfile, true);
        outfile <<"\n";
    }
    const char* str = "#include \"aclnn/acl_meta.h\"\n\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n\n";
    outfile << str;
    return outfile;
}

bool AclnnOpGenerator::AclnnOpGenFunProtoValueDependParam(
    OpDef& opDef, const OpDefName& opdefName, size_t paramIndex, ofstream& outfile, const std::string& opType) const
{
    std::vector<OpParamDef>& inputs = opDef.GetInputs();
    const std::vector<std::string>& paramNames = opdefName.inputsName;
    OpParamDef& valueDependInput = inputs[paramIndex];
    const std::string& valueDependParamName = paramNames[paramIndex];
    std::vector<ge::DataType> dataTypes = valueDependInput.GetDataTypes();
    if (dataTypes.empty()) {
        return true;
    }
    ge::DataType firstType = dataTypes[0];
    std::string errMsg = "ValueDepend input dtypes of op " + opType +  " must satisfy one of the following conditions:\n"
                       " 1. All input dtypes are float.\n"
                       " 2. All input dtypes are bool.\n"
                       " 3. All input dtypes are integers or unsigned integers form the supported set: [int64, uint64, int32, uint32, int16, uint16, int8, uint8].";
    if (firstType == ge::DT_FLOAT && AreAllInputDataTypesSame(dataTypes)) {
        outfile << "    const aclFloatArray *" << valueDependParamName << ",\n";
    } else if (firstType == ge::DT_BOOL && AreAllInputDataTypesSame(dataTypes)) {
        outfile << "    const aclBoolArray *" << valueDependParamName << ",\n";
    } else if (firstType == ge::DT_INT64 && AreAllInputDataTypesSame(dataTypes)) {
         outfile << "    const aclIntArray *" << valueDependParamName << ",\n";
    } else if (AreInputDataTypesSupported(VALUE_DEPEND_SUPPORT_INT_DTYPES, dataTypes)) {
        if (AclnnCheckForInt64CombinationWithValueDepend(opDef, paramIndex, opType)) {
            outfile << "    const aclIntArray *" << valueDependParamName << ",\n";
        } else {
            return false;
        }
    } else {
        Generator::SetErrorMessage(errMsg);
        return false;
    }
    return true;
}

std::vector<std::vector<ge::DataType>> AclnnOpGenerator::AclnnGetInputAndOutputDataTypeList(std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs) const
{
    std::vector<std::vector<ge::DataType>> paramDataTypeList;
    size_t dataTypeNum = inputs[0].GetDataTypes().size();
    for (size_t i = 0U; i < dataTypeNum; ++i) {
        std::vector<ge::DataType> paramDataTypes;
        for (size_t j = 0U; j < inputs.size(); ++j) {
            paramDataTypes.emplace_back((inputs[j].GetDataTypes())[i]);
        }
        for (size_t j = 0U; j < outputs.size(); ++j) {
            paramDataTypes.emplace_back((outputs[j].GetDataTypes())[i]);
        }
        paramDataTypeList.emplace_back(paramDataTypes);
    }
    return paramDataTypeList;
}

std::vector<std::string> AclnnOpGenerator::AclnnGetInputAndOutputNames(
    const std::vector<OpParamDef>& inputs, const std::vector<OpParamDef>& outputs) const
{
    std::vector<std::string> paramOriginNames;
    for (const auto& input : inputs) {
        paramOriginNames.emplace_back(std::string(input.GetParamName().GetString()));
    }
    for (const auto& output : outputs) {
        paramOriginNames.emplace_back(std::string(output.GetParamName().GetString()));
    }
    return paramOriginNames;
}

std::vector<size_t> AclnnOpGenerator::AclnnGetValueDependIntTypeIndex(std::vector<OpParamDef>& inputs) const
{
    std::vector<size_t> valueDependIndex;
    for (size_t i = 0; i < inputs.size(); ++i) {
        if (!inputs[i].IsValueDepend()) {
            continue;
        }
        if (AreInputDataTypesSupported(VALUE_DEPEND_SUPPORT_INT_DTYPES, inputs[i].GetDataTypes())) {
            valueDependIndex.emplace_back(i);
        }
    }
    return valueDependIndex;
}

std::string AclnnOpGenerator::AclnnBuildValueDependDataTypeErrorMessage(const std::vector<std::string>& paramOriginNames, const std::vector<ge::DataType>& originDataTypes, 
    const std::vector<ge::DataType>& requiredDataTypes, int valueDependIndex, const string& opType) const
{
    std::string errMsg = "One combination of input and output dtypes of op " + opType +" is [";
    for (size_t j = 0U; j < paramOriginNames.size(); ++j) {
        errMsg = errMsg + paramOriginNames[j] + ": " + DTYPE_SUPPORT_MAP.at(originDataTypes[j]);
        if (j != paramOriginNames.size() - 1) {
            errMsg += ", ";
        }
    }
    errMsg += "], the combinations of input and output dtypes should add [";
    for (size_t j = 0U; j < paramOriginNames.size(); ++j) {
        errMsg = errMsg + paramOriginNames[j] + ": " + DTYPE_SUPPORT_MAP.at(requiredDataTypes[j]);
        if (j != paramOriginNames.size() - 1) {
            errMsg += ", ";
        }
    }
    errMsg += "], because of the ValueDepend input " + paramOriginNames[valueDependIndex] + ".";
    return errMsg;
}

bool AclnnOpGenerator::AclnnCheckForInt64CombinationWithValueDepend(OpDef& opDef, size_t paramIndex, const std::string& opType) const
{
    for (auto& aicoreItem : opDef.AICore().GetAICoreConfigs()) {
        std::string socVer = aicoreItem.first.GetString();
        if (SOC_SUPPORT_MAP.find(socVer) == SOC_SUPPORT_MAP.end()) {
            continue;
        }
        OpAICoreConfig aicoreConfig = aicoreItem.second;
        std::vector<OpParamDef> inputs = opDef.GetMergeInputs(aicoreConfig);
        std::vector<OpParamDef> outputs = opDef.GetMergeOutputs(aicoreConfig);
        std::vector<ge::DataType> valueDependDataTypes = inputs[paramIndex].GetDataTypes();
        std::vector<std::vector<ge::DataType>> paramDataTypeList = AclnnGetInputAndOutputDataTypeList(inputs, outputs);
        std::vector<std::string> paramOriginNames = AclnnGetInputAndOutputNames(inputs, outputs);
        std::vector<size_t> valueDependIndexList = AclnnGetValueDependIntTypeIndex(inputs);

        for (size_t i = 0U; i < valueDependDataTypes.size(); ++i) {
            if (valueDependDataTypes[i] == ge::DT_INT64) {
                continue;
            }
            auto inputDataTypes = paramDataTypeList[i];
            for (const auto valueDependIntIndex : valueDependIndexList) {
                inputDataTypes[valueDependIntIndex] = ge::DT_INT64;
            }
            bool hasInt64Combination = false;
            for (const auto& baseInputDataTypes : paramDataTypeList) {
                if (inputDataTypes == baseInputDataTypes) {
                    hasInt64Combination = true;
                    break;
                }
            }
            if (hasInt64Combination) {
                continue;
            }
            
            std::string errMsg = AclnnBuildValueDependDataTypeErrorMessage(paramOriginNames, paramDataTypeList[i], inputDataTypes, paramIndex, opType);
            Generator::SetErrorMessage(errMsg);
            return false;
        }
    }
    return true;
}

bool AclnnOpGenerator::AclnnIsValueDependDataTypeSupport(std::vector<OpParamDef>& inputs, const std::string& opType) const
{
    for (auto& input : inputs) {
        if (!input.IsValueDepend()) {
            continue;
        }
        std::vector<ge::DataType> dataTypes = input.GetDataTypes();
        if (dataTypes.empty()) {
            return false;
        }

        ge::DataType firstType = dataTypes[0];
        if (VALUE_DEPEND_SUPPORT_DTYPES.find(firstType) == VALUE_DEPEND_SUPPORT_DTYPES.cend()) {
            std::string str = "ValueDepend input dtypes of op " + opType +  " must be [float, bool, "
                                + "int64, uint64, int32, uint32, int16, uint16, int8, uint8].";
            Generator::SetErrorMessage(str);
            return false;
        }
        std::string errMsg = "ValueDepend input dtypes of op " + opType +  " must satisfy one of the following conditions:\n"
                        " 1. All input dtypes are float.\n"
                        " 2. All input dtypes are bool.\n"
                        " 3. All input dtypes are integers or unsigned integers form the supported set: [int64, uint64, int32, uint32, int16, uint16, int8, uint8].";
        if (firstType == ge::DT_FLOAT && AreAllInputDataTypesSame(dataTypes)) {
            continue;
        } else if (firstType == ge::DT_BOOL && AreAllInputDataTypesSame(dataTypes)) {
            continue;
        } else if (AreInputDataTypesSupported(VALUE_DEPEND_SUPPORT_INT_DTYPES, dataTypes)) {
            continue;
        } else {
            Generator::SetErrorMessage(errMsg);
            return false;
        }
    }
    return true;
}

bool AclnnOpGenerator::AclnnIsRefParam(const std::string& inputName) const
{
    if (inputName.size() > OP_ACLNN_REF_SUFFIX_LEN) {
        const std::string suffix = inputName.substr(inputName.size() - OP_ACLNN_REF_SUFFIX_LEN);
        return suffix == "Ref";
    }
    return false;
}

void AclnnOpGenerator::AclnnOpGenFunProtoParam(const OpParamDef& param, const std::string& paramName,
    int32_t paramType, bool hasOutputShapeDepend, ofstream& outfile) const
{
    if (param.IsScalar()) {
        outfile << "    const aclScalar *" << paramName << ",\n";
    } else if (param.IsScalarList()) {
        outfile << "    const aclScalarList *" << paramName << ",\n";
    } else if (paramType != DYNAMIC) {
        if ((AclnnIsRefParam(paramName)) || (hasOutputShapeDepend)) {
            outfile << "    aclTensor *" << paramName << ",\n";
        } else {
            outfile << "    const aclTensor *" << paramName << ",\n";
        }
    } else {
        if (AclnnIsRefParam(paramName)) {
            outfile << "    aclTensorList *" << paramName << ",\n";
        } else {
            outfile << "    const aclTensorList *" << paramName << ",\n";
        }
    }
}

void AclnnOpGenerator::AclnnOpGenFunProtoInputParams(
    OpDef& opDef, OpDefName& opdefName, ofstream& outfile, const uint32_t version, const bool valDependApi) const
{
    std::vector<OpParamDef>& params = opDef.GetInputs();
    std::vector<std::string>& paramNames = opdefName.inputsName;
    const std::string opType = opDef.GetOpType().GetString();
    for (size_t i = 0; i < params.size(); i++) {
        if (params[i].GetVersion() > version) {
            continue;
        }
        int32_t type = params[i].GetParamType();
        const char* const valueDepend = params[i].GetValueDepend().GetString();
        if (!valDependApi && ((std::string(valueDepend) == "required") || (std::string(valueDepend) == "optional"))) {
            if ((params[i].IsScalar() || (params[i].IsScalarList()))) {
                Generator::SetErrorMessage(
                    "Valuedepend and Scalar/ScalarList of op " + opType + " cannot be configured at the same time.");
                return;
            }
            if (!AclnnOpGenFunProtoValueDependParam(opDef, opdefName, i, outfile, opType)) {
                return;
            }
        }else {
            AclnnOpGenFunProtoParam(params[i], paramNames[i], type, opdefName.hasOutputShapeDepend, outfile);
        }
    }
}

void AclnnOpGenerator::AclnnOpGenFunProtoOutputParams(
    OpDef& opDef, OpDefName& opdefName, ofstream& outfile, const uint32_t version, const bool valDependApi) const
{
    std::vector<OpParamDef>& params = opDef.GetOutputs();
    std::vector<std::string>& paramNames = opdefName.outputsName;
    const std::string opType = opDef.GetOpType().GetString();
    for (size_t i = 0; i < params.size(); i++) {
        if (params[i].GetVersion() > version) {
            continue;
        }
        int32_t type = params[i].GetParamType();
        const char* const valueDepend = params[i].GetValueDepend().GetString();
        if (AclnnIsRefParam(paramNames[i])) {
            continue;
        } else if (!valDependApi && ((std::string(valueDepend) == "required") || (std::string(valueDepend) == "optional"))) {
            Generator::SetErrorMessage(
                    "Valuedepend does not support output " + std::string(params[i].GetParamName().GetString()) + " of op " + opType + ".");
            return;
        } else {
            AclnnOpGenFunProtoParam(params[i], paramNames[i], type, opdefName.hasOutputShapeDepend, outfile);
        }
    }
}

void AclnnOpGenerator::AclnnOpGenFunProtoAttrParamsImpl(
    OpAttrDef& attr, ofstream& outfile, std::string& name, const std::string opType) const
{
    auto iter = ACLNN_OP_ATTR_TYPE_MAP.find(attr.GetCfgDataType().GetString());
    if (iter == ACLNN_OP_ATTR_TYPE_MAP.end()) {
        std::string str = "Data type of attr " + std::string(attr.GetName().GetString());
        AclnnSetErrorMessage(str, opType);
        return;
    }
    int32_t type = iter->second;
    switch (type) {
        case OP_ACLNN_ATTR_TYPE_STR: {
            outfile << "    char *" << name << ",\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_BOOL: {
            outfile << "    bool " << name << ",\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTBOOL: {
            outfile << "    const aclBoolArray *" << name << ",\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_FLOAT: {
            outfile << "    double " << name << ",\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTFLOAT: {
            outfile << "    const aclFloatArray *" << name << ",\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_INT: {
            outfile << "    int64_t " << name << ",\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTINT: {
            outfile << "    const aclIntArray *" << name << ",\n";
            break;
        }
        default: {
            break;
        }
    }
}

void AclnnOpGenerator::AclnnOpGenFunProtoAttrParams(
    OpDef& opDef, std::vector<std::string>& paramNames, ofstream& outfile, uint32_t version) const
{
    std::vector<OpAttrDef>& attrs = opDef.GetAttrs();
    const std::string opType = opDef.GetOpType().GetString();
    for (size_t i = 0U; i < attrs.size(); i++) {
        if (attrs[i].GetVersion() > version) {
            continue;
        }
        AclnnOpGenFunProtoAttrParamsImpl(attrs[i], outfile, paramNames[i], opType);
    }
}

void AclnnOpGenerator::AclnnOpGenValueDependInput(
    OpParamDef& input, std::string& name, size_t index, ofstream& outfile) const
{
    ge::DataType dataType = input.GetDataTypes()[0];
    if (dataType == ge::DT_FLOAT) {
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddFloatArrayInput(*executor, " << name << ", " << index <<
                   "));\n";
    } else if (dataType == ge::DT_BOOL) {
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddBoolArrayInput(*executor, " << name << ", " << index <<
                   "));\n";
    } else {
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddIntArrayInput(*executor, " << name << ", " << index <<
                   "));\n";
    }
}

bool AclnnOpGenerator::AclOpGenScalarInput(OpParamDef& input, size_t index,
    OpDefName& opdefName, ofstream& outfile, string funcName) const
{
    const char* const srcName = input.GetScalarName().GetString();
    if (!std::string(srcName).empty()) {
        for (size_t j = 0U; j < opdefName.originInputName.size(); j++) {
            if (std::string(srcName) == opdefName.originInputName[j]) {
                outfile << "    NNOPBASE_ASSERT_OK_RETVAL(" << funcName << "(*executor, "
                        << opdefName.inputsName[index] << ", " << index << ", " << j << ", ge::DT_UNDEFINED));\n";
                return true;
            }
        }
    } else if (input.GetScalarType() != ge::DT_UNDEFINED) {
        auto dtype = DTYPE_SUPPORT_MAP.find(input.GetScalarType());
        if (dtype == DTYPE_SUPPORT_MAP.end()) {
            return false;
        }
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(" << funcName << "(*executor, " << opdefName.inputsName[index]
                << ", " << index << ", -1, " << dtype->second << "));\n";
    } else {
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(" << funcName << "(*executor, " << opdefName.inputsName[index]
                << ", " << index << ", -1, ge::DT_UNDEFINED));\n";
    }
    return true;
}

void AclnnOpGenerator::AclnnOpGenCodeAddInputTensors(OpDef& opDef, OpDefName& opdefName, ofstream& outfile, bool valueDependApi) const
{
    std::vector<OpParamDef>& inputs = opDef.GetInputs();
    const std::string opType = opDef.GetOpType().GetString();
    for (size_t i = 0U; i < inputs.size(); i++) {
        if (inputs[i].IsOutputShapeDependOnCompute()) {
            std::string str = "Input " + std::string(inputs[i].GetParamName().GetString()) +
                              " of " + opType + " does not support OutputShapeDependOnCompute.";
            Generator::SetErrorMessage(str);
            return;
        }
        int32_t type = inputs[i].GetParamType();
        const char* const valueDepend = inputs[i].GetValueDepend().GetString();
        if (!valueDependApi && !std::string(valueDepend).empty()) {
            AclnnOpGenValueDependInput(inputs[i], opdefName.inputsName[i], i, outfile);
        } else if (inputs[i].IsScalar()) {
            if (!AclOpGenScalarInput(inputs[i], i, opdefName, outfile, "NnopbaseAddScalarInput")) {
                std::string str = "Dtype of input " + std::string(inputs[i].GetParamName().GetString());
                AclnnSetErrorMessage(str, opType);
            }
        } else if (inputs[i].IsScalarList()) {
            if (!AclOpGenScalarInput(inputs[i], i, opdefName, outfile, "NnopbaseAddScalarListInput")) {
                std::string str = "Dtype of input " + std::string(inputs[i].GetParamName().GetString());
                AclnnSetErrorMessage(str, opType);
            }
        } else if (type == DYNAMIC) {
            outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddDynamicInput(*executor, " << opdefName.inputsName[i]
                    << ", " << i << "));\n";
        } else {
            bool ignoreCont = inputs[i].GetIgnoreContiguous();
            if (ignoreCont) {
                outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddIgnoreContinuesInput(*executor, "
                        << opdefName.inputsName[i] << ", " << i << "));\n";
            } else {
                outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddInput(*executor, " << opdefName.inputsName[i]
                        << ", " << i << "));\n";
            }
        }
    }
}

void AclnnOpGenerator::AclnnOpGenCodeAddOutputShapeDependTensors(
    std::vector<OpParamDef>& outputs, std::vector<std::string>& name, ofstream& outfile) const
{
    outfile << "    if (NnopbaseAddOutputShapeDependTensor != NULL) {\n";
    for (size_t i = 0; i < outputs.size(); i++) {
        int32_t type = outputs[i].GetParamType();
        if (type == DYNAMIC) {
            outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddDynamicOutput(*executor, " << name[i] << ", "
                    << i << "));\n";
        } else {
            if (outputs[i].IsOutputShapeDependOnCompute()) {
                outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddOutputShapeDependTensor(*executor, "
                        << name[i] << ", " << i << "));\n";
            } else {
                outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddOutput(*executor, " << name[i] << ", " << i
                        << "));\n";
            }
        }
    }
    outfile << "    }\n";
}

void AclnnOpGenerator::AclnnOpGenCodeAddOutputTensors(std::vector<OpParamDef>& outputs, std::vector<std::string>& name,
    bool hasOutputShapeDepend, ofstream& outfile) const
{
    if (hasOutputShapeDepend) {
        AclnnOpGenCodeAddOutputShapeDependTensors(outputs, name, outfile);
    } else {
        for (size_t i = 0; i < outputs.size(); i++) {
            int32_t type = outputs[i].GetParamType();
            if (type == DYNAMIC) {
                outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddDynamicOutput(*executor, " << name[i] << ", " << i
                        << "));\n";
            } else {
                outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddOutput(*executor, " << name[i] << ", " << i
                        << "));\n";
            }
        }
    }
}

void AclnnOpGenerator::AclnnoOpGenCodeAttrValue(OpAttrDef& attr, size_t* len, ofstream& outfile) const
{
    const char* val = attr.GetAttrDefaultVal("[]").GetString();
    size_t strSize = strlen(val);
    *len = 1;
    for (size_t i = 1U; i < strSize - 1U; i++) {
        outfile << val[i];
        if (val[i] == ',') {
            outfile <<" ";
            (*len)++;
        }
    }
    outfile << "};\n";
}

void AclnnOpGenerator::AclnnOpGenCodeOptionalStrAttr(
    OpAttrDef& attr, std::string& name, size_t index, ofstream& outfile) const
{
    if (attr.IsRequired()) {
        outfile << "    NNOPBASE_ASSERT_NOTNULL_RETVAL(" << name << ");\n";
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddAttrWithDtype(*executor, static_cast<void*>(" << name <<
                   "), strlen(" << name << ") + 1, " << index << ", kNnopbaseString));\n";
    } else {
        outfile << "    if (" << name << ") {\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddAttrWithDtype(*executor, static_cast<void*>(" <<
                   name << "), strlen(" << name << ") + 1, " << index << ", kNnopbaseString));\n";
        outfile << "    } else {\n";
        outfile << "        static char *" << name << "Def = \"" << attr.GetAttrDefaultVal("[]").GetString()
                << "\";\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddAttrWithDtype(*executor, static_cast<void*>("
                << name << "Def), strlen(" << name << "Def) + 1, " << index << ", kNnopbaseString));\n    }\n";
    }
}

void AclnnOpGenerator::AclnnOpGenCodeOptionalBoolAttr(
    OpAttrDef& attr, std::string& name, size_t index, ofstream& outfile) const
{
    if (attr.IsRequired()) {
        outfile << "    NNOPBASE_ASSERT_NOTNULL_RETVAL(" << name << ");\n";
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddBoolArrayAttr(*executor, " << name <<
                   ", " << index << "));\n";
    } else {
        outfile << "    if (" << name << ") {\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddBoolArrayAttr(*executor, " << name <<
                   ", " << index << "));\n";
        outfile << "    } else {\n";
        outfile << "        static bool " << name << "Def[] = {";
        size_t len = 0U;
        AclnnoOpGenCodeAttrValue(attr, &len, outfile);
        outfile << "        static size_t " << name << "Len = " << len << ";\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddArrayAttrWithDtype(*executor, static_cast<void*>("
                << name << "Def), " << name << "Len, sizeof(bool), " << index << ", kNnopbaseBool));\n    }\n";
    }
}

void AclnnOpGenerator::AclnnOpGenCodeOptionalFloatAttr(
    OpAttrDef& attr, std::string& name, size_t index, ofstream& outfile) const
{
    if (attr.IsRequired()) {
        outfile << "    NNOPBASE_ASSERT_NOTNULL_RETVAL(" << name << ");\n";
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddFloatArrayAttr(*executor, " << name <<
                   ", " << index << "));\n";
    } else {
        outfile << "    if (" << name << ") {\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddFloatArrayAttr(*executor, " << name <<
                   ", " << index << "));\n";
        outfile << "    } else {\n";
        outfile << "        static float " << name << "Def[] = {";
        size_t len = 0U;
        AclnnoOpGenCodeAttrValue(attr, &len, outfile);
        outfile << "        static size_t " << name << "Len = " << len << ";\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddArrayAttrWithDtype(*executor, static_cast<void*>("
                << name << "Def), " << name << "Len, sizeof(float), " << index << ", kNnopbaseFloat));\n    }\n";
    }
}

void AclnnOpGenerator::AclnnOpGenCodeOptionalIntAttr(
    OpAttrDef& attr, std::string& name, size_t index, ofstream& outfile) const
{
    if (attr.IsRequired()) {
        outfile << "    NNOPBASE_ASSERT_NOTNULL_RETVAL(" << name << ");\n";
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddIntArrayAttr(*executor, " << name <<
                   ", " << index << "));\n";
    } else {
        outfile << "    if (" << name << ") {\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddIntArrayAttr(*executor, " << name <<
                   ", " << index << "));\n";
        outfile << "    } else {\n";
        outfile << "        static int64_t " << name << "Def[] = {";
        size_t len = 0U;
        AclnnoOpGenCodeAttrValue(attr, &len, outfile);
        outfile << "        static size_t " << name << "Len = " << len << ";\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddArrayAttrWithDtype(*executor, static_cast<void*>("
                << name << "Def), " << name << "Len, sizeof(int64_t), " << index << ", kNnopbaseInt));\n    }\n";
    }
}

void AclnnOpGenerator::AclnnOpGenCodeAttrParamsImpl(
    std::vector<OpAttrDef>& attrs, std::vector<std::string>& name, size_t index, int32_t type, ofstream& outfile) const
{
    switch (type) {
        case OP_ACLNN_ATTR_TYPE_FLOAT: {
            outfile << "    float tmp" << index << " = static_cast<float>(" << name[index] << ");\n";
            outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddAttrWithDtype(*executor, static_cast<void*>(&tmp"
                    << index << "), sizeof(float), " << index << ", kNnopbaseFloat));\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_STR: {
            AclnnOpGenCodeOptionalStrAttr(attrs[index], name[index], index, outfile);
            break;
        }
        case OP_ACLNN_ATTR_TYPE_BOOL: {
            outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddAttrWithDtype(*executor, static_cast<void*>(&"
                    << name[index] << "), sizeof(bool), " << index << ", kNnopbaseBool));\n";
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTBOOL: {
            AclnnOpGenCodeOptionalBoolAttr(attrs[index], name[index], index, outfile);
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTFLOAT: {
            AclnnOpGenCodeOptionalFloatAttr(attrs[index], name[index], index, outfile);
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTINT: {
            AclnnOpGenCodeOptionalIntAttr(attrs[index], name[index], index, outfile);
            break;
        }
        case OP_ACLNN_ATTR_TYPE_INT: {
            outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddAttrWithDtype(*executor, static_cast<void*>(&"
                    << name[index] << "), sizeof(int64_t), " << index << ", kNnopbaseInt));\n";
            break;
        }
        default: {
            break;
        }
    }
}

void AclnnOpGenerator::AclnnOpGenCodeAttrParams(
    OpDef& opDef, std::vector<std::string>& name, ofstream& outfile, std::vector<int32_t>& attrTypes) const
{
    const std::string opType = opDef.GetOpType().GetString();
    std::vector<OpAttrDef>& attrs = opDef.GetAttrs();
    for (size_t i = 0; i < attrs.size(); i++) {
        auto iter = ACLNN_OP_ATTR_TYPE_MAP.find(attrs[i].GetCfgDataType().GetString());
        if (iter == ACLNN_OP_ATTR_TYPE_MAP.end()) {
            std::string str = "Data type of attr " + std::string(attrs[i].GetName().GetString());
            AclnnSetErrorMessage(str, opType);
            return;
        }
        int32_t type = iter->second;
        attrTypes.push_back(type);
        AclnnOpGenCodeAttrParamsImpl(attrs, name, i, type, outfile);
    }
}

void AclnnOpGenerator::AclnnOpGenCodeFunIoTypeCommentImpl(
    const int32_t type, std::string& name, ofstream& outfile) const
{
    if (type == REQUIRED) {
        outfile << " * " << name << " : ";
        outfile << "required\n";
    } else if ((type == OPTIONAL) || (type == VIRTUAL)) {
        name += "Optional";
        outfile << " * " << name << " : ";
        outfile << "optional\n";
    } else {
        outfile << " * " << name << " : ";
        outfile << "dynamic\n";
    }
}

void AclnnOpGenerator::AclnnOpGenCodeFunInputComment(
    std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs,
    OpDefName& opdefName, ofstream& outfile, uint32_t version) const
{
    for (size_t i = 0U; i < inputs.size(); i++) {
        if (inputs[i].GetVersion() > version) {
            continue;
        }
        const char* originName = inputs[i].GetParamName().GetString();
        std::string name = ConvertName(originName);
        opdefName.originInputName.push_back(originName);
        bool isRef = false;
        for (size_t j = 0U; j < outputs.size(); j++) {
            if (inputs[i].GetParamName() == outputs[j].GetParamName()) {
                isRef = true;
                break;
            }
        }
        if (isRef) {
            name += "Ref";
        }
        AclnnOpGenCodeFunIoTypeCommentImpl(inputs[i].GetParamType(), name, outfile);
        opdefName.inputsName.push_back(name);
    }
}

void AclnnOpGenerator::AclnnOpGenCodeFunOutputComment(
    std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs,
    std::vector<std::string>& paramName, bool *hasOutputShapeDepend, ofstream& outfile) const
{
    for (size_t i = 0U; i < outputs.size(); i++) {
        if (outputs[i].IsOutputShapeDependOnCompute()) {
            *hasOutputShapeDepend = true;
        }
        bool isRef = false;
        for (size_t j = 0U; j < inputs.size(); j++) {
            if (outputs[i].GetParamName() == inputs[j].GetParamName()) {
                isRef = true;
                break;
            }
        }
        std::string name;
        if (isRef) {
            name = ConvertName(outputs[i].GetParamName().GetString());
            name += "Ref";
        } else if (outputs.size() == 1U) {
            name = "out";
        } else {
            name = ConvertName(outputs[i].GetParamName().GetString());
            name += "Out";
        }
        AclnnOpGenCodeFunIoTypeCommentImpl(outputs[i].GetParamType(), name, outfile);
        paramName.push_back(name);
    }
}

bool AclnnOpGenerator::IsBaseTypeOfAttr(const char *type) const
{
    return ((strcmp(type, OP_ACLNN_ATTR_STR) == 0) || (strcmp(type, OP_ACLNN_ATTR_LISTBOOL) == 0) ||
            (strcmp(type, OP_ACLNN_ATTR_LISTFLOAT) == 0) || (strcmp(type, OP_ACLNN_ATTR_LISTINT) == 0));
}

void AclnnOpGenerator::AclnnOpGenCodeFunAttrComment(
    std::vector<OpAttrDef>& attrs, std::vector<std::string>& paramName,
    ofstream& outfile, uint32_t version) const
{
    for (size_t i = 0; i < attrs.size(); i++) {
        if (attrs[i].GetVersion() > version) {
            continue;
        }
        std::string name = ConvertName(attrs[i].GetName().GetString());
        if (attrs[i].IsRequired()) {
            paramName.push_back(name);
            outfile << " * " << name << " : ";
            outfile << "required\n";
        } else {
            const char *type = attrs[i].GetCfgDataType().GetString();
            if (IsBaseTypeOfAttr(type)) {
                name += "Optional";
            }
            paramName.push_back(name);
            outfile << " * " << name << " : ";
            outfile << "optional\n";
        }
    }
}

void AclnnOpGenerator::AclnnOpGenCodeRunForWSFunComment(
    OpDef& opDef, OpDefName& opdefName, ofstream& outfile, uint32_t version) const
{
    outfile << "/* funtion: " << opdefName.prefixName << "GetWorkspaceSize\n * parameters :\n";
    AclnnOpGenCodeFunInputComment(opDef.GetInputs(), opDef.GetOutputs(), opdefName, outfile, version);
    AclnnOpGenCodeFunAttrComment(opDef.GetAttrs(), opdefName.attrsName, outfile, version);
    AclnnOpGenCodeFunOutputComment(
        opDef.GetInputs(), opDef.GetOutputs(), opdefName.outputsName, &opdefName.hasOutputShapeDepend, outfile);
    outfile << " * workspaceSize : size of workspace(output).\n";
    outfile << " * executor : executor context(output).\n */\n";
}

void AclnnOpGenerator::AclnnOpGenCodeRunWithWSFunComment(string& prefixName, ofstream& outfile) const
{
    outfile << "/* funtion: " << prefixName << "\n * parameters :\n";
    outfile << " * workspace : workspace memory addr(input).\n";
    outfile << " * workspaceSize : size of workspace(input).\n";
    outfile << " * executor : executor context(input).\n";
    outfile << " * stream : acl stream.\n */\n";
}

void AclnnOpGenerator::AclnnOpGenCodeRunForWSFunProto(
    OpDef& opDef, OpDefName& opdefName, ofstream& outfile, uint32_t version) const
{
    outfile << "aclnnStatus " << opdefName.prefixName << "GetWorkspaceSize(\n";
    AclnnOpGenFunProtoInputParams(opDef, opdefName, outfile, version, false);
    AclnnOpGenFunProtoAttrParams(opDef, opdefName.attrsName, outfile, version);
    AclnnOpGenFunProtoOutputParams(opDef, opdefName, outfile, 0U, false);
    outfile << "    uint64_t *workspaceSize,\n";
    outfile << "    aclOpExecutor **executor)";
}

bool AclnnOpGenerator::IsOpValueDepend(OpDef& opDef) const
{
    std::vector<OpParamDef>& param = opDef.GetInputs();
    for (size_t i = 0; i < param.size(); i++) {
        const char* const valueDepend = param[i].GetValueDepend().GetString();
        if ((std::string(valueDepend) == "required") || (std::string(valueDepend) == "optional")) {
            return true;
        }
    }
    return false;
}

void AclnnOpGenerator::AclnnOpGenCodeTensorRunForWSFunProto(
    OpDef& opDef, OpDefName& opdefName, ofstream& outfile, uint32_t version) const
{
    outfile << "aclnnStatus " << opdefName.prefixName << "TensorGetWorkspaceSize(\n";
    AclnnOpGenFunProtoInputParams(opDef, opdefName, outfile, version, true);
    AclnnOpGenFunProtoAttrParams(opDef, opdefName.attrsName, outfile, version);
    AclnnOpGenFunProtoOutputParams(opDef, opdefName, outfile, 0U, true);
    outfile << "    uint64_t *workspaceSize,\n";
    outfile << "    aclOpExecutor **executor)";
}

void AclnnOpGenerator::AclnnOpGenCodeRunWithWSFunProto(string& prefixName, ofstream& outfile) const
{
    outfile << "aclnnStatus " << prefixName << "(\n";
    outfile << "    void *workspace,\n";
    outfile << "    uint64_t workspaceSize,\n";
    outfile << "    aclOpExecutor *executor,\n";
    outfile << "    aclrtStream stream)";
}

void AclnnOpGenerator::AclnnOpGenCodeIoParamCheck(
    std::vector<OpParamDef>& param, std::vector<std::string>& name, ofstream& outfile, bool isInput) const
{
    for (size_t i = 0; i < param.size(); i++) {
        int32_t type = param[i].GetParamType();
        // option 和 output的ref不做检查
        if ((type != OPTIONAL) && (type != VIRTUAL) && !(!isInput && AclnnIsRefParam(name[i]))) {
            outfile << "    NNOPBASE_ASSERT_NOTNULL_RETVAL(" << name[i] << ");\n";
        }
    }
}

void AclnnOpGenerator::AclnnOpGenCodeParamCheck(
    std::vector<OpParamDef>& inputs, std::vector<OpParamDef>& outputs,
    OpDefName& opdefName, ofstream& outfile) const
{
    if (inputs.size() == 0U && outputs.size() == 0U) {
        return;
    }
    AclnnOpGenCodeIoParamCheck(inputs, opdefName.inputsName, outfile, true);
    AclnnOpGenCodeIoParamCheck(outputs, opdefName.outputsName, outfile, false);
}

void AclnnOpGenerator::AclnnGenCodeCommFunDelcare(ofstream& outfile) const
{
    const char* str = "#define ACLNN_SUCCESS  0\n"
        "#define ACLNN_ERR_PARAM_NULLPTR 161001\n"
        "#define ACLNN_ERR_PARAM_INVALID 161002\n\n";
    outfile << str;
}

void AclnnOpGenerator::AclnnOpGenCodeWorkspaceDelcare(
    OpDef& opDef, OpDefName& opdefName, ofstream& outfile, uint32_t version) const
{
    AclnnOpGenCodeRunForWSFunComment(opDef, opdefName, outfile, version);
    outfile << "__attribute__((visibility(\"default\")))\n";
    AclnnOpGenCodeRunForWSFunProto(opDef, opdefName, outfile, version);
    outfile << ";\n\n";

    // Non value depend operator does not need to generate TensorGetWorkspaceSize interface
    if (IsOpValueDepend(opDef)) {
        outfile << "__attribute__((visibility(\"default\")))\n";
        AclnnOpGenCodeTensorRunForWSFunProto(opDef, opdefName, outfile, version);
        outfile << ";\n\n";
    }
    AclnnOpGenCodeRunWithWSFunComment(opdefName.prefixName, outfile);
    outfile << "__attribute__((visibility(\"default\")))\n";
    AclnnOpGenCodeRunWithWSFunProto(opdefName.prefixName, outfile);
    outfile << ";\n\n";
}

void AclnnOpGenerator::AclnnOpGenCodeIoParamDesc(std::vector<OpParamDef>& params, const string& desc,
    ofstream& outfile, uint32_t version) const
{
    outfile << "    char " << desc << "[] = {";
    for (size_t i = 0U; i < params.size(); i++) {
        if (params[i].GetVersion() > version) {
            continue;
        }
        if (i != 0U) {
            outfile << ", ";
        }
        int32_t type = params[i].GetParamType();
        if (type == REQUIRED) {
            outfile << "1";
        } else if ((type == OPTIONAL) || (type == VIRTUAL)) {
            outfile << "0";
        } else {
            outfile << "2";
        }
    }
    outfile << "};\n";
}

void AclnnOpGenerator::AclnnOpGenCodeAttrParamDesc(std::vector<OpAttrDef>& attrs, const string& desc,
    ofstream& outfile, uint32_t version) const
{
    outfile << "    char " << desc << "[] = {";
    for (size_t i = 0U; i < attrs.size(); i++) {
        if (attrs[i].GetVersion() > version) {
            continue;
        }
        if (i != 0U) {
            outfile << ", ";
        }
        if (attrs[i].IsRequired()) {
            outfile << "1";
        } else {
            outfile << "0";
        }
    }
    outfile << "};\n\n";
}

void AclnnOpGenerator::AclnnOpGenCodeParamDesc(OpDef& opDef, ofstream& outfile, uint32_t version) const
{
    AclnnOpGenCodeIoParamDesc(opDef.GetInputs(), "inputDesc", outfile, version);
    AclnnOpGenCodeIoParamDesc(opDef.GetOutputs(), "outputDesc", outfile, version);
    AclnnOpGenCodeAttrParamDesc(opDef.GetAttrs(), "attrDesc", outfile, version);
}

std::vector<std::string> AclnnOpGenerator::Spilt(const std::string& str, const char delim) const
{
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (std::getline(ss, item, delim)) {
        result.push_back(item);
    }
    return result;
}

std::string AclnnOpGenerator::ToLower(std::string str) const
{
    std::transform(str.begin(), str.end(), str.begin(), [](char c) {
        return std::tolower(c);
    });
    return str;
}

bool AclnnOpGenerator::IsSupportProduct(OpDef& opDef) const
{
    const char *productEnv = nullptr;
    MM_SYS_GET_ENV(MM_ENV_OPS_PRODUCT_NAME, productEnv);
    if (productEnv != nullptr && strlen(productEnv) != 0) {
        std::string productStr = productEnv;
        productStr = ToLower(productStr);
        const auto& products = Spilt(productStr, ';');
        const auto& map = opDef.AICore().GetAICoreConfigs();
        for (const auto& it : map) {
            const std::string& socVer = it.first.GetString();
            for (const auto& product : products) {
                if (socVer == product) {
                    return true;
                }
            }
        }
    }
    return false;
}

void AclnnOpGenerator::AclnnOpGenCodeExecutor(OpDef& opDef, ofstream& outfile) const
{
    static std::string str = "\n"
        "    if (!executorSpace) {\n"
        "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseCreateExecutorSpace(&executorSpace));\n"
        "    }\n"
        "    nnopExecutor = NnopbaseGetExecutor(executorSpace, opType, inputDesc, "
        "sizeof(inputDesc) / sizeof(char), outputDesc,\n"
        "                                       sizeof(outputDesc) / sizeof(char), attrDesc, "
        "sizeof(attrDesc) / sizeof(char));\n"
        "    NNOPBASE_ASSERT_NOTNULL_RETVAL(nnopExecutor);\n"
        "    NNOPBASE_ASSERT_NOTNULL_RETVAL(executor);\n"
        "    *executor = reinterpret_cast<aclOpExecutor *>(nnopExecutor);\n"
        "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddTilingId(*executor, &tilingId));\n"
        "    if (NnopbaseSetMatchArgsFlag != NULL) {\n"
        "        NnopbaseSetMatchArgsFlag(*executor);\n"
        "    }\n";
    outfile << str;
    if (IsSupportProduct(opDef)) {
        outfile << "#ifdef ACLNN_WITH_BINARY\n";
        outfile << "    NnopbaseAddOpTypeId(*executor, " << opDef.GetOpType().GetString() << "OpTypeId);\n";
        outfile << "#endif\n";
    }
}

void AclnnOpGenerator::AclnnOpGenSocSupportList(OpDef& opDef, ofstream& outfile) const
{
    uint32_t socSupportListLen = 0;
    outfile << "uint32_t socSupportList[] = {";
    string str;
    std::map<ge::AscendString, OpAICoreConfig> map = opDef.AICore().GetAICoreConfigs();
    std::unordered_set<std::string> socSupportSet;
    const std::string opType = opDef.GetOpType().GetString();
    for (auto iter = map.begin(); iter != map.end(); ++iter) {
        std::string socVer = iter->first.GetString();
        auto it = SOC_SUPPORT_MAP.find(socVer);
        if (it == SOC_SUPPORT_MAP.end()) {
            std::string soc = "Invalid socVersion ";
            soc.append(socVer);
            soc += " of op " + opType +
                ", please check whether AddConfig are correctly configured in Opdef.";
            ASCENDLOGW("%s\n", soc.c_str());
 	        continue;
        }
        if ((it->first).find("kirin") == 0) {
            continue;
        }
        socSupportSet.insert(it->second);
        if (!str.empty()) {
            str.append(",");
        }
        str.append(it->second);
        socSupportListLen++;
    }
    auto computeUnitCfg = opbuild::Params::GetInstance().Optional("compute_unit");
    if (computeUnitCfg.size() != 0) {
        std::vector<std::string> computeUnits;
        Split(computeUnitCfg, ';', computeUnits);
        for (uint32_t i = 0; i < computeUnits.size(); ++i) {
            std::string socVer = computeUnits[i];
            auto it = SOC_SUPPORT_MAP.find(socVer);
            if (it == SOC_SUPPORT_MAP.end()) {
                std::string soc = "Invalid socVersion ";
                soc.append(socVer);
                soc += " of op " + opType + ", please check whether ASCEND_COMPUTE_UNIT is correctly configured.";
                ASCENDLOGW("%s\n", soc.c_str());
                continue;
            }
            if ((socSupportSet.find(it->second) == socSupportSet.end()) && ((it->first).find("kirin") != 0)) {
                if (!str.empty()) {
                    str.append(",");
                }
                str.append(it->second);
                socSupportListLen++;
            }
        }
    }
    if (str.empty()) {
        std::string warnMsg = "Invalid socVersion of op " + opType + " in OpDef and ASCEND_COMPUTE_UNIT, please make sure at least one is correct.";
        ASCENDLOGW("%s\n", warnMsg.c_str());
    }
    outfile << str;
    outfile << "};\n";
    outfile << "uint32_t socSupportListLen = " << socSupportListLen << ";\n\n";
}

void AclnnOpGenerator::AclnnOpGenHcclServerTypeList(OpDef& opDef, ofstream& outfile) const
{
    string str;
    std::map<ge::AscendString, OpAICoreConfig> map = opDef.AICore().GetAICoreConfigs();
    const std::string opType = opDef.GetOpType().GetString();
    for (auto iter = map.begin(); iter != map.end(); ++iter) {
        std::string socVer = iter->first.GetString();
        auto it = SOC_SUPPORT_MAP.find(socVer);
        if (it == SOC_SUPPORT_MAP.end()) {
            std::string warnMsg = "Invalid socVersion" + socVer + "of op " + opType + " when setting HcclServerType.";
            ASCENDLOGW("%s\n", warnMsg.c_str());
            continue;
        }
        auto type = opDef.MC2().GetHcclServerType(iter->first);
        if (type == HcclServerType::MAX) {
            // server type has not been set, no need to gen code
            continue;
        }
        auto serverTypeIter = HCCL_SERVER_TYPE_MAP.find(type);
        if (serverTypeIter == HCCL_SERVER_TYPE_MAP.end()) {
            Generator::SetErrorMessage("HcclServerType params of op " + opType +
                                       " must be an enumeration value of enum class HcclServerType.");
            continue;
        }
        str.append(serverTypeIter->second);
        if (iter != std::prev(map.end())) {
            str.append(",");
        }
    }
    if (str.empty()) {
        return;
    }
    outfile << "NnopbaseHcclServerType hcclServerTypeList[] = {";
    outfile << str;
    outfile << "};\n\n";
}

void AclnnOpGenerator::AclnnOpGenIoTensorDesc(
    size_t i, std::vector<OpParamDef>& params, ofstream& outfile, const std::string opType) const
{
    for (size_t j = 0U; j < params.size(); j++) {
        std::vector<ge::DataType> dataTypes = params[j].GetDataTypes();
        auto dtype = DTYPE_SUPPORT_MAP.find(dataTypes[i]);
        if (dtype == DTYPE_SUPPORT_MAP.end()) {
            std::string str = "Dtype of " + std::string(params[j].GetParamName().GetString());
            AclnnSetErrorMessage(str, opType);
            return;
        }
        if (j == 0U) {
            outfile << "{" << dtype->second << ", ";
        } else {
            outfile << "     {" << dtype->second << ", ";
        }
        std::vector<ge::Format> formatTypes = params[j].GetFormats();
        auto format = FORMAT_SUPPORT_MAP.find(formatTypes[i]);
        if (format == FORMAT_SUPPORT_MAP.end()) {
            std::string str = "Format of " + std::string(params[j].GetParamName().GetString());
            AclnnSetErrorMessage(str, opType);
            return;
        }
        if (j == params.size() - 1) {
            outfile << format->second << "}};\n";
        } else {
            outfile << format->second << "},\n";
        }
    }
}
void AclnnOpGenerator::AclnnOpGenTensorDesc(size_t index, std::vector<OpParamDef>& inputs,
    std::vector<OpParamDef>& outputs, ofstream& outfile, const std::string opType) const
{
    if (inputs.size() > 0) {
        for (size_t i = 0U; i < inputs[0].GetDataTypes().size(); i++) {
            outfile << "TensorDesc inputDesc" << index << "_" << i << "[" << inputs.size() << "] =\n";
            outfile << "    {";
            AclnnOpGenIoTensorDesc(i, inputs, outfile, opType);
        }
    }

    if (outputs.size() > 0) {
        for (size_t i = 0U; i < outputs[0].GetDataTypes().size(); i++) {
            outfile << "TensorDesc outputDesc" << index << "_" << i << "[" << outputs.size() << "] =\n";
            outfile << "    {";
            AclnnOpGenIoTensorDesc(i, outputs, outfile, opType);
        }
    }

    if (inputs.size() == 0) {
        for (size_t i = 0U; i < outputs[0].GetDataTypes().size(); i++) {
            outfile << "SupportInfo list" << index << "_" << i << " = {nullptr, 0, outputDesc" << index << "_" << i << ", " <<
                outputs.size() << "};\n";
        }
    } else if (outputs.size() == 0) {
        for (size_t i = 0U; i < inputs[0].GetDataTypes().size(); i++) {
            outfile << "SupportInfo list" << index << "_" << i << " = {inputDesc" << index << "_" << i << ", " <<
                inputs.size() << ", nullptr, 0};\n";
        }
    } else {
        for (size_t i = 0U; i < inputs[0].GetDataTypes().size(); i++) {
            outfile << "SupportInfo list" << index << "_" << i << " = {inputDesc" << index << "_" << i << ", " <<
                inputs.size() << ", outputDesc" << index << "_" << i << ", " << outputs.size() << "};\n";
        }
    }
}

void AclnnOpGenerator::AclnnOpGenOpSupportList(size_t index, std::vector<OpParamDef>& inputs,
    std::vector<OpParamDef>& outputs, ofstream& outfile, const std::string opType) const
{
    AclnnOpGenTensorDesc(index, inputs, outputs, outfile, opType);
    size_t size = 0U;
    if (inputs.size() == 0U) {
        size = outputs[0].GetDataTypes().size();
        if (size == 0U) {
            const std::string errorInfo = "The dtype size of output[0] of op " + opType + " is 0.";
            Generator::SetErrorMessage(errorInfo);
            return;
        }
    } else {
        size = inputs[0].GetDataTypes().size();
        if (size == 0U) {
            const std::string errorInfo = "The dtype size of input[0] of op " + opType + " is 0.";
            Generator::SetErrorMessage(errorInfo);
            return;
        }
    }

    outfile << "SupportInfo supportInfo" << index << "[" << size << "] = {";
    for (size_t i = 0U; i < size - 1U; i++) {
        outfile << "list" << index << "_" << i << ", ";
    }
    outfile << "list" << index << "_" << (size - 1U) << "};\n";
    outfile << "OpSocSupportInfo socSupportInfo" << index << "= {supportInfo" << index << ", " << size << "};\n\n";
}

void AclnnOpGenerator::AclnnOpGenOpSupportListAll(OpDef& opDef, ofstream& outfile) const
{
    size_t i = 0U;
    const std::string opType = opDef.GetOpType().GetString();
    for (auto& aicoreItem : opDef.AICore().GetAICoreConfigs()) {
        std::string socVer = aicoreItem.first.GetString();
        if (SOC_SUPPORT_MAP.find(socVer) == SOC_SUPPORT_MAP.end()) {
            continue;
        }
        OpAICoreConfig aicoreConfig = aicoreItem.second;
        std::vector<OpParamDef> inputs = opDef.GetMergeInputs(aicoreConfig);
        std::vector<OpParamDef> outputs = opDef.GetMergeOutputs(aicoreConfig);
        AclnnOpGenOpSupportList(i, inputs, outputs, outfile, opType);
        if (!AclnnIsValueDependDataTypeSupport(inputs, opType)) {
            return;
        }
        i++;
    }
    if (i == 0U) {
        std::string localopType = opDef.GetOpType().GetString();
        Generator::SetErrorMessage(
            "The soc version of op " + localopType + 
            " is not configured, at least one soc version must be configured."
            " Check whether the soc version is added to the op proto type"
            " definition in the host implementation(Opdef, through AddConfig).");
        return;
    }
    outfile << "OpSocSupportInfo opSocSupportList[" << i << "] = {";
    for (size_t index = 0U; index < i - 1; index++) {
        outfile << "socSupportInfo" << index << ", ";
    }
    outfile << "socSupportInfo" << (i - 1U) << "};\n";
    outfile << "OpSupportList supportList = {opSocSupportList, " << i << "};\n";
}

void AclnnOpGenerator::AclnnGenOpTypeId(OpDef& opDef, ofstream& outfile) const
{
    outfile << "[[maybe_unused]] uint32_t NNOPBASE_" << opDef.GetOpType().GetString() << " = 0U;\n";
}

void AclnnOpGenerator::AclnnGenNameSpaceInfo(ofstream& outfile, OpDef& opDef) const
{
    outfile << "namespace {\n";
 	outfile << OP_ACLNN_STRUCT_INFO;
    outfile << OP_ACLNN_SOC_INFO;
 	outfile << OP_ACLNN_NNOPBASE_ATTR_DTYPE_INFO;
    if (opDef.MC2().GetHcclServerType() != HcclServerType::MAX) {
        outfile << "enum NnopbaseHcclServerType {\n";
        outfile << "    NNOPBASE_HCCL_SERVER_TYPE_AICPU = 0,\n";
        outfile << "    NNOPBASE_HCCL_SERVER_TYPE_MTE,\n";
        outfile << "    NNOPBASE_HCCL_SERVER_TYPE_CCU,\n";
        outfile << "    NNOPBASE_HCCL_SERVER_TYPE_END\n";
        outfile << "};\n";
    }

    if (opDef.GetInputs().size() == 0U && opDef.GetOutputs().size() == 0U) {
        AclnnGenOpTypeId(opDef, outfile);
        outfile << "} // namespace\n\n";
        return;
    }
    AclnnOpGenSocSupportList(opDef, outfile);
    AclnnOpGenHcclServerTypeList(opDef, outfile);
    AclnnOpGenOpSupportListAll(opDef, outfile);
    outfile << "\n";
    AclnnGenOpTypeId(opDef, outfile);
    outfile << "} // namespace\n\n";
}

void AclnnOpGenerator::AclnnGenCheckInfo(ofstream& outfile) const
{
    AclnnGenCodeCommFunDelcare(outfile);
    const char* str = "#define NNOPBASE_ASSERT_OK_RETVAL(v)                                    \\\n"
        "    do {                                                                \\\n"
        "        const aclnnStatus _chk_stutus = (v);                            \\\n"
        "        if (_chk_stutus != ACLNN_SUCCESS) {                             \\\n"
        "            NnopbaseOpLogE(_chk_stutus, #v);                            \\\n"
        "            return _chk_stutus;                                         \\\n"
        "        }                                                               \\\n"
        "    } while (false)\n"
        "\n"
        "#define NNOPBASE_ASSERT_NOTNULL_RETVAL(v)                               \\\n"
        "    do {                                                                \\\n"
        "        if ((v) == nullptr) {                                           \\\n"
        "            NnopbaseOpLogE(ACLNN_ERR_PARAM_NULLPTR, #v \" != nullptr\");  \\\n"
        "            return ACLNN_ERR_PARAM_NULLPTR;                             \\\n"
        "        }                                                               \\\n"
        "    } while (false)\n"
        "\n";
    outfile << str;
}

bool AclnnOpGenerator::IsSupportAutoContiguous(std::vector<OpParamDef>& inputs) const
{
    for (auto& paramDef : inputs) {
        if (paramDef.GetAutoContiguous()) {
            return true;
        }
    }
    return false;
}

void AclnnOpGenerator::AclnnGenUncontDeclaration(OpDef& opDef, ofstream& outfile) const
{
    if (IsSupportAutoContiguous(opDef.GetInputs())) {
        outfile << "extern aclnnStatus NnopbaseGetUnContiguousTensors(void *executor, "
                   "const aclTensorList **inTensors);\n";
        outfile << "extern aclnnStatus NnopbaseSetUnContExecutor(void *executor, aclOpExecutor *inExe, "
                   "const size_t inWsSize);\n";
        outfile << "extern aclnnStatus NnopbaseGetUnContExecutor(void *executor, aclOpExecutor **inExe, "
                   "size_t *inWsSize);\n";
        outfile << "extern aclnnStatus NnopbaseGetRefUnContiguousTensors(void *executor, "
                   "const aclTensorList **unContTensors, const aclTensorList **contTensors);\n";
        outfile << "extern aclnnStatus NnopbaseSetViewCopyExecutor(void *executor, aclOpExecutor *exe);\n";
        outfile << "extern aclnnStatus NnopbaseGetViewCopyExecutor(void *executor, aclOpExecutor **exe);\n";
        outfile << "extern aclnnStatus NnopbaseReleaseRefContiguousTensors(void *executor, "
                   "const aclTensorList **tensors);\n";
        outfile << "extern void *NnopbaseGetApiFunc(const char *funcName);\n";
        outfile << "using AclnnContiguousGetWorkspaceSizeFunc = "
                   "aclnnStatus (*)(const aclTensorList *, uint64_t *, aclOpExecutor **);\n";
        outfile << "using AclnnViewCopyGetWorkspaceSizeFunc = "
                   "aclnnStatus (*)(const aclTensorList *, const aclTensorList *, uint64_t *, aclOpExecutor **);\n";
        outfile << "using AclnnFunc = aclnnStatus (*)(void *, uint64_t, aclOpExecutor *, aclrtStream);\n";
    }
}

void AclnnOpGenerator::AclnnGenMc2Declaration(OpDef& opDef, ofstream& outfile) const
{
    if (opDef.MC2().GetHcclGroups().size() > 0U) {
        outfile << "extern aclnnStatus NnopbaseSetMc2(void *const executor);\n";
        outfile << "extern aclnnStatus NnopbaseSetHcomGroup(void *const executor, char *const group);\n";
    }
    if (opDef.MC2().GetHcclServerType() != HcclServerType::MAX) {
        outfile << "extern void NnopbaseSetHcclServerTypeList(void *executor, NnopbaseHcclServerType *hcclServerTypeList, "
                "uint32_t *socSupportList, size_t socSupportListLen);\n";
    }
}

void AclnnOpGenerator::AclnnGenCodeDecImpl(string& declFile, ofstream& outfile) const
{
    outfile << "#include <string.h>\n";
    outfile << "#include \"graph/types.h\"\n";
    outfile << "#include \"" << declFile << ".h\"\n";
}

void AclnnOpGenerator::AclnnGenExternFunc(ofstream& outfile) const
{
    outfile << "extern void NnopbaseOpLogE(const aclnnStatus code, const char *const expr);\n\n";
    outfile << OP_ACLNN_EXTERN_FUNC;
}

void AclnnOpGenerator::AclnnGenOutEmptyLaunchDeclaration(OpDef& opDef, ofstream& outfile) const
{
    if (opDef.AICore().GetZeroEleOutputLaunchFlag()) {
        outfile << "extern void NnopbaseSetZeroEleOutputLaunchFlag(void *executor);\n";
    }
}

void AclnnOpGenerator::AclnnGenCodeImplStart(
    string& declFile, bool hasOutputShapeDepend, ofstream& outfile, OpDef& opDef) const
{
    AclnnGenCodeDecImpl(declFile, outfile);
    outfile << "\n";
    if (IsSupportProduct(opDef)) {
        const std::string& opType = opDef.GetOpType().GetString();
        outfile << "#ifdef ACLNN_WITH_BINARY\n";
        outfile << "#include <vector>\n";
        outfile << "#include <tuple>\n";
        outfile << "#include <map>\n";
        outfile << "#include \"graph/ascend_string.h\"\n";
        outfile << "#include \"" << opType << "_op_resource.h\"\n";
        outfile << "using OP_HOST_FUNC_HANDLE = std::vector<void *>;\n"
                   "using OP_RES = std::tuple<const uint8_t *, const uint8_t *>;\n"
                   "using OP_BINARY_RES = std::vector<OP_RES>;\n"
                   "using OP_RUNTIME_KB_RES = std::vector<OP_RES>;\n"
                   "using OP_RESOURCES = std::map<ge::AscendString,\n"
                   "    std::tuple<OP_HOST_FUNC_HANDLE, OP_BINARY_RES, OP_RUNTIME_KB_RES>>;\n"
                   "using OP_SOC_RESOURCES = std::map<ge::AscendString, std::tuple<OP_HOST_FUNC_HANDLE,\n"
                   "    std::map<ge::AscendString, OP_BINARY_RES>, OP_RUNTIME_KB_RES>>;\n";
        outfile << "namespace op {\n";
        outfile << "extern uint32_t GenOpTypeId(const char *op_name, const OP_RESOURCES &op_resources);\n";
        outfile << "extern uint32_t GenOpTypeId(const char *op_name, const OP_SOC_RESOURCES &op_resources);\n";
        outfile << "}\n";
        outfile << "#endif\n";
        outfile << "\n";
    }
    AclnnGenNameSpaceInfo(outfile, opDef);
    AclnnGenExternFunc(outfile);
    AclnnGenUncontDeclaration(opDef, outfile);
    AclnnGenMc2Declaration(opDef, outfile);
    AclnnGenOutEmptyLaunchDeclaration(opDef, outfile);
    if (hasOutputShapeDepend) {
        outfile
            << "extern aclnnStatus __attribute__((weak)) NnopbaseAddOutputShapeDependTensor(void *executor, aclTensor "
               "*tensor, const uint32_t index);\n";
    }
    outfile << "\n";
    AclnnGenCheckInfo(outfile);
}

void AclnnOpGenerator::AclnnGenCodeImplEnd(ofstream& outfile) const
{
    const char* str = "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n";
    outfile << str;
}

std::string AclnnOpGenerator::AclnnOpGetIoSize(std::vector<OpParamDef>& params, ofstream& outfile) const
{
    (void)(outfile);
    std::string result;
    size_t ioNum = 0U;
    for (size_t i = 0U; i < params.size(); i++) {
        if (params[i].GetParamType() != DYNAMIC) {
            ioNum++;
        }
    }
    result.append(std::to_string(ioNum));
    return result;
}

void AclnnOpGenerator::AclopGenDfxInfo(OpDef& opDef, string& opName, string& prefixName, ofstream& outfile) const
{
    outfile << "\n{\n";
    outfile << "    uint64_t timeStamp = NnopbaseMsprofSysTime();\n";
    if (IsSupportProduct(opDef)) {
        const std::string& opType = opDef.GetOpType().GetString();
        outfile << "#ifdef ACLNN_WITH_BINARY" << "\n";
        outfile << "    static uint32_t " << opType << "OpTypeId = op::GenOpTypeId(\"" << opType << "\", " <<
                   opType  << "_RESOURCES);\n";
        outfile << "#endif" << "\n";
    }
    outfile << "    static NnopbaseDfxId dfxId = {0x60000, __func__, false};\n";
    outfile << "    static NnopbaseDfxId tilingId = {0x60000, \"" << prefixName << "Tiling\", false};\n";
    outfile << "    void *nnopExecutor;\n";
    outfile << "    static void *executorSpace = NULL;\n";
    outfile << "    const char *opType = \"" << opName << "\";\n";
}

void AclnnOpGenerator::AclnnOpGenCodeSetUnContInfo(OpDef& opDef, ofstream& outfile) const
{
    if (IsSupportAutoContiguous(opDef.GetInputs())) {
        outfile << "\n";
        outfile << "    const aclTensorList *inUnContTensors = nullptr;\n";
        outfile << "    NnopbaseGetUnContiguousTensors(*executor, &inUnContTensors);\n";
        outfile << "    aclOpExecutor *aclInExecutor = nullptr;\n";
        outfile << "    uint64_t inContWorkspaceSize = 0U;\n";
        outfile << "    if (inUnContTensors != nullptr) {\n";
        outfile << "        static AclnnContiguousGetWorkspaceSizeFunc aclnnContiguousGetWorkspaceSize = "
                   "(AclnnContiguousGetWorkspaceSizeFunc)NnopbaseGetApiFunc(\"aclnnContiguousGetWorkspaceSize\");\n";
        outfile << "        NNOPBASE_ASSERT_NOTNULL_RETVAL(aclnnContiguousGetWorkspaceSize);\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(aclnnContiguousGetWorkspaceSize(inUnContTensors, "
                "&inContWorkspaceSize, &aclInExecutor));\n";
        outfile << "    }\n";
        outfile << "    NnopbaseSetUnContExecutor(*executor, aclInExecutor, inContWorkspaceSize);\n\n";
    }
}

void AclnnOpGenerator::AclnnOpGenCodeHcclGroup(
    OpDef& opDef, std::vector<std::string>& name, std::vector<int32_t> attrTypes, ofstream& outfile) const
{
    std::vector<ge::AscendString>& groups = opDef.MC2().GetHcclGroups();
    std::vector<OpAttrDef> attrs = opDef.GetAttrs();
    const std::string opType = opDef.GetOpType().GetString();
    for (const auto& group : groups) {
        int32_t index = -1;
        for (size_t i = 0U; i < attrs.size(); i++) {
            if (group == attrs[i].GetName()) {
                index = i;
                if (attrTypes[i] != OP_ACLNN_ATTR_TYPE_STR) {
                    Generator::SetErrorMessage("HcclGroup params of op " + opType +
                                            " must be String attr name, but " + group.GetString() +
                                            " is not String attr.");
                    return;
                }
            }
        }
        if (index == -1) {
            std::string str(group.GetString());
            Generator::SetErrorMessage("HcclGroup params of op " + opType + " must be String attr name, but " +
                                       str + " is not attr name.");
            return;
        }
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseSetHcomGroup(*executor, " << name[index] << "));\n";
    }
}

void AclnnOpGenerator::AclnnOpGenAddParamName(OpDef& opDef, const OpDefName& opdefName, ofstream& outfile) const
{
    std::vector<OpParamDef>& inputs = opDef.GetInputs();
    std::vector<OpParamDef>& outputs = opDef.GetOutputs();
    outfile << "    if (NnopbaseAddParamName != NULL) {\n";
    for (size_t i = 0U; i < inputs.size(); i++) {
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddParamName(*executor, " << i << ", \""
                << opdefName.inputsName[i] << "\", true));\n";
    }
    for (size_t i = 0U; i < outputs.size(); i++) {
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddParamName(*executor, " << i << ", \""
                << opdefName.outputsName[i] << "\", false));\n";
    }
    outfile << "    }\n";
}

void AclnnOpGenerator::AclnnOpGenCodeSetRef(std::vector<OpParamDef>& inputs,
    std::vector<OpParamDef>& outputs, ofstream& outfile) const
{
    for (size_t i = 0U; i < inputs.size(); i++) {
        for (size_t j = 0U; j < outputs.size(); j++) {
            if (inputs[i].GetParamName() == outputs[j].GetParamName()) {
                outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseSetRef(*executor, " << i << ", " << j << "));\n";
                break;
            }
        }
    }
}

void AclnnOpGenerator::AclnnOpGenFormatMode(OpDef& opDef, ofstream& outfile) const
{
    if (opDef.GetFormatMatchMode() == ops::FormatCheckOption::STRICT) {
        outfile << "    if (NnopbaseSetFormatMatchMode != NULL) {\n";
        outfile << "        NnopbaseSetFormatMatchMode(*executor, 1);\n";
        outfile << "    }\n";
    }
}

void AclnnOpGenerator::AclOpGenMatchArgsFunc(ofstream& outfile) const
{
    outfile << "    if (NnopbaseMatchArgs != NULL) {\n";
    outfile << "        if (NnopbaseMatchArgs(*executor, workspaceSize)) {\n";
    outfile << "            NnopbaseReportApiInfo(timeStamp, dfxId);\n";
    outfile << "            return ACLNN_SUCCESS;\n";
    outfile << "        }\n";
    outfile << "    }\n";
}

void AclnnOpGenerator::AclopGenCodeCommon(
    OpDef& opDef, OpDefName& opdefName, ofstream& outfile, uint32_t version, bool valueDependApi) const
{
    std::vector<OpParamDef>& outputs = opDef.GetOutputs();
    AclnnOpGenCodeParamDesc(opDef, outfile, version);
    AclnnOpGenCodeParamCheck(opDef.GetInputs(), outputs, opdefName, outfile);
    AclnnOpGenCodeExecutor(opDef, outfile);
    if (opDef.MC2().GetHcclServerType() != HcclServerType::MAX) {
        outfile << "    NnopbaseSetHcclServerTypeList(*executor, hcclServerTypeList, "
            "socSupportList, socSupportListLen);\n";
    }
    
    AclnnOpGenFormatMode(opDef, outfile);
    AclnnOpGenCodeSetRef(opDef.GetInputs(), outputs, outfile);
    AclnnOpGenCodeAddInputTensors(opDef, opdefName, outfile, valueDependApi);
    std::vector<int32_t> attrTypes;
    AclnnOpGenCodeAttrParams(opDef, opdefName.attrsName, outfile, attrTypes);
    AclnnOpGenCodeAddOutputTensors(outputs, opdefName.outputsName, opdefName.hasOutputShapeDepend, outfile);
    if (opDef.MC2().GetHcclGroups().size() > 0U) {
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseSetMc2(*executor));\n";
        AclnnOpGenCodeHcclGroup(opDef, opdefName.attrsName, attrTypes, outfile);
    }
    if (opDef.AICore().GetZeroEleOutputLaunchFlag()) {
        outfile << "    NnopbaseSetZeroEleOutputLaunchFlag(*executor);\n";
    }
    AclOpGenMatchArgsFunc(outfile);
    if (opDef.GetInputs().size() == 0U && outputs.size() == 0U) {
        return;
    } else {
        AclnnOpGenAddParamName(opDef, opdefName, outfile);
        outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseAddSupportList(*executor, &supportList, socSupportList" <<
                   ", socSupportListLen));\n";
    }
    AclnnOpGenCodeSetUnContInfo(opDef, outfile);
}

void AclnnOpGenerator::AclnnOpGenIoParam(
    std::vector<OpParamDef>& params, std::vector<std::string>& paramName,
    uint32_t version, const bool isInput, ofstream& outfile) const
{
    for (size_t i = 0U; i < params.size(); i++) {
        if (!isInput && AclnnIsRefParam(paramName[i])) {
            // do nothing
        } else if (params[i].GetVersion() > version) {
            outfile << "        NULL,\n";
        } else {
            outfile << "        " << paramName[i] << ",\n";
        }
    }
}

void AclnnOpGenerator::AclnnOpGenAttrDefParam(
    std::vector<OpAttrDef>& attrs, std::vector<std::string>& paramName, ofstream& outfile) const
{
    for (size_t i = 0U; i < attrs.size(); i++) {
        outfile << "        " << paramName[i] << ",\n";
    }
}

void AclnnOpGenerator::AclnnOpGenDefaultArrayAttr(
    OpAttrDef& attr, std::string attrsName, std::vector<std::string>& defaultAttrsName,
    int32_t type, ofstream& outfile) const
{
    switch (type) {
        case OP_ACLNN_ATTR_TYPE_LISTBOOL: {
            outfile << "    static bool " << attrsName << "Def[] = {";
            size_t len = 0U;
            AclnnoOpGenCodeAttrValue(attr, &len, outfile);
            outfile << "    static aclBoolArray *" << attrsName << "Array = aclCreateBoolArray(" << attrsName << "Def, "
                    << len << ");\n";
            defaultAttrsName.push_back(attrsName + "Array");
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTFLOAT: {
            outfile << "    static float " << attrsName << "Def[] = {";
            size_t len = 0U;
            AclnnoOpGenCodeAttrValue(attr, &len, outfile);
            outfile << "    static aclFloatArray *" << attrsName << "Array = aclCreateFloatArray(" << attrsName
                    << "Def, " << len << ");\n";
            defaultAttrsName.push_back(attrsName + "Array");
            break;
        }
        case OP_ACLNN_ATTR_TYPE_LISTINT: {
            outfile << "    static int64_t " << attrsName << "Def[] = {";
            size_t len = 0U;
            AclnnoOpGenCodeAttrValue(attr, &len, outfile);
            outfile << "    static aclIntArray *" << attrsName << "Array = aclCreateIntArray(" << attrsName << "Def, "
                    << len << ");\n";
            defaultAttrsName.push_back(attrsName + "Array");
            break;
        }
        default:
            break;
    }
}

void AclnnOpGenerator::AclnnOpGenDefaultAttr(
    OpDef& opdef, OpDefName& opdefName, uint32_t version, ofstream& outfile) const
{
    std::vector<OpAttrDef>& attrs = opdef.GetAttrs();
    const std::string opType = opdef.GetOpType().GetString();
    opdefName.defaultAttrsName.clear();
    for (size_t i = 0U; i < attrs.size(); i++) {
        if (attrs[i].GetVersion() > version) {
            auto iter = ACLNN_OP_ATTR_TYPE_MAP.find(attrs[i].GetCfgDataType().GetString());
            if (iter == ACLNN_OP_ATTR_TYPE_MAP.end()) {
                std::string str = "Data type of attr" + std::string(attrs[i].GetName().GetString());
                AclnnSetErrorMessage(str, opType);
                return;
            }
            int32_t type = iter->second;
            switch (type) {
                case OP_ACLNN_ATTR_TYPE_FLOAT: {
                    outfile << "    static float " << opdefName.attrsName[i]
                            << "Def = " << attrs[i].GetAttrDefaultVal("[]").GetString() << ";\n";
                    opdefName.defaultAttrsName.push_back(opdefName.attrsName[i] + "Def");
                    break;
                }
                case OP_ACLNN_ATTR_TYPE_BOOL: {
                    outfile << "    static bool " << opdefName.attrsName[i]
                            << "Def = " << attrs[i].GetAttrDefaultVal("[]").GetString() << ";\n";
                    opdefName.defaultAttrsName.push_back(opdefName.attrsName[i] + "Def");
                    break;
                }
                case OP_ACLNN_ATTR_TYPE_INT: {
                    outfile << "    static int64_t " << opdefName.attrsName[i]
                            << "Def = " << attrs[i].GetAttrDefaultVal("[]").GetString() << ";\n";
                    opdefName.defaultAttrsName.push_back(opdefName.attrsName[i] + "Def");
                    break;
                }
                case OP_ACLNN_ATTR_TYPE_STR: {
                    outfile << "    static char *" << opdefName.attrsName[i] << "Def = \""
                            << attrs[i].GetAttrDefaultVal("[]").GetString() << "\";\n";
                    opdefName.defaultAttrsName.push_back(opdefName.attrsName[i] + "Def");
                    break;
                }
                default:
                    AclnnOpGenDefaultArrayAttr(
                        attrs[i], opdefName.attrsName[i], opdefName.defaultAttrsName, type, outfile);
                    break;
            }
        } else {
            opdefName.defaultAttrsName.push_back(opdefName.attrsName[i]);
        }
    }
}

void AclnnOpGenerator::AclnnAddDisableInputIndex(OpDef& opDef, uint32_t version, ofstream& outfile) const
{
    std::vector<OpParamDef>& inputs = opDef.GetInputs();
    for (size_t i = 0U; i < inputs.size(); i++) {
        if (inputs[i].GetVersion() > version) {
            outfile << "        NnopbaseDisableOptionalInput(*executor, " << i << ");\n";
        }
    }
}

void AclnnOpGenerator::AclnnOpGenCodeRunForWorkspaceVersionImpl(
    OpDef& opDef, OpDefName& opdefName, uint32_t version, uint32_t maxVersion, ofstream& outfile) const
{
    string decFile = opdefName.decName;
    string decName = opdefName.decName;
    if (version != 0U) {
        decName = decName + "_v" + to_string(version);
    }
    AclnnGenCodeDecImpl(decName, outfile);
    outfile << "#include \"" << decFile + "_v" + to_string(maxVersion) << ".h\"\n\n";
    const char* str = "\n#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n\n";
    outfile << str;
    outfile << "aclnnStatus __attribute__((weak)) NnopbaseDisableOptionalInput(void *executor, const size_t irIndex);\n\n";
    AclnnOpGenCodeRunForWSFunProto(opDef, opdefName, outfile, version);
    outfile << "\n{\n";
    AclnnOpGenDefaultAttr(opDef, opdefName, version, outfile);
    outfile << "    aclnnStatus ret = " << opdefName.maxVersionName << "GetWorkspaceSize(\n";
    AclnnOpGenIoParam(opDef.GetInputs(), opdefName.inputsName, version, true, outfile);
    AclnnOpGenAttrDefParam(opDef.GetAttrs(), opdefName.defaultAttrsName, outfile);
    AclnnOpGenIoParam(opDef.GetOutputs(), opdefName.outputsName, version, false, outfile);
    outfile << "        workspaceSize,\n";
    outfile << "        executor);\n";
    outfile << "    if (NnopbaseDisableOptionalInput != NULL) {\n";
    AclnnAddDisableInputIndex(opDef, version, outfile);
    outfile << "    }\n";
    outfile << "    return ret;\n";
    outfile << "}\n\n";
}

bool AclnnOpGenerator::HasRef(std::vector<std::string>& names) const
{
    for (auto& name : names) {
        if (AclnnIsRefParam(name)) {
            return true;
        }
    }
    return false;
}

void AclnnOpGenerator::AclopGenCodeRefContiguous(OpDef& opDef, OpDefName& opdefName, ofstream& outfile) const
{
    if (IsSupportAutoContiguous(opDef.GetInputs()) && HasRef(opdefName.inputsName)) {
        outfile << "\n";
        outfile << "    aclOpExecutor *viewcopyExecutor = nullptr;\n";
        outfile << "    uint64_t viewcopyWsSize = 0U;\n";
        outfile << "    if (inUnContTensors != nullptr) {\n";
        outfile << "        const aclTensorList *unContTensors = nullptr;\n";
        outfile << "        const aclTensorList *contTensors = nullptr;\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(NnopbaseGetRefUnContiguousTensors(*executor, &unContTensors, "
                   "&contTensors));\n";
        outfile << "        if (unContTensors != nullptr) {\n";
        outfile << "            static AclnnViewCopyGetWorkspaceSizeFunc aclnnViewCopyGetWorkspaceSize = "
                   "(AclnnViewCopyGetWorkspaceSizeFunc)NnopbaseGetApiFunc(\"aclnnViewCopyGetWorkspaceSize\");\n";
        outfile << "            NNOPBASE_ASSERT_NOTNULL_RETVAL(aclnnViewCopyGetWorkspaceSize);\n";
        outfile << "            NNOPBASE_ASSERT_OK_RETVAL(aclnnViewCopyGetWorkspaceSize(contTensors, unContTensors, "
                   "&viewcopyWsSize, &viewcopyExecutor));\n";
        outfile << "            NNOPBASE_ASSERT_OK_RETVAL(NnopbaseSetViewCopyExecutor(*executor, viewcopyExecutor));\n";
        outfile << "        }\n";
        outfile << "    }\n";
        outfile << "    if (viewcopyWsSize > *workspaceSize) {\n";
        outfile << "        *workspaceSize = viewcopyWsSize;\n";
        outfile << "    }\n";
    }
}

// 就这个需要写成 workspace
void AclnnOpGenerator::AclnnOpGenCodeRunForWorkspaceImpl(
    OpDef& opDef, OpDefName& opdefName, uint32_t version, ofstream& outfile, bool valDependApi) const
{
    if (!valDependApi) {
        AclnnGenCodeImplStart(opdefName.maxDecName, opdefName.hasOutputShapeDepend, outfile, opDef);
        AclnnOpGenCodeRunForWSFunProto(opDef, opdefName, outfile, version);
        AclopGenDfxInfo(opDef, opdefName.opName, opdefName.prefixName, outfile);
        AclopGenCodeCommon(opDef, opdefName, outfile, version, valDependApi);
    } else if (IsOpValueDepend(opDef)){
        AclnnOpGenCodeTensorRunForWSFunProto(opDef, opdefName, outfile, version);
        AclopGenDfxInfo(opDef, opdefName.opName, opdefName.prefixName, outfile);
        AclopGenCodeCommon(opDef, opdefName, outfile, version, valDependApi);
    }
    outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseRunForWorkspace(*executor, workspaceSize));\n";
    AclopGenCodeRefContiguous(opDef, opdefName, outfile);
    if (IsSupportAutoContiguous(opDef.GetInputs())) {
        outfile << "    *workspaceSize += inContWorkspaceSize;\n";
    }
    outfile << "    NnopbaseReportApiInfo(timeStamp, dfxId);\n";
    outfile << "    return ACLNN_SUCCESS;\n";
    outfile << "}\n\n";
}

void AclnnOpGenerator::AclnnOpGenCodeRunUnContWithWorkspaceImpl(OpDef& opDef, OpDefName& opDefName,
                                                                ofstream& outfile) const
{
    if (IsSupportAutoContiguous(opDef.GetInputs())) {
        outfile << "    aclOpExecutor *aclInExecutor = nullptr;\n";
        outfile << "    uint64_t inContWorkspaceSize = 0U;\n";
        outfile << "    NnopbaseGetUnContExecutor(executor, &aclInExecutor, &inContWorkspaceSize);\n";
        // workspaceSize一定大于inContWorkspaceSize
        outfile << "    if (workspaceSize < inContWorkspaceSize) {\n";
        outfile << "        NnopbaseOpLogE(ACLNN_ERR_PARAM_INVALID, \"input workspaceSize must be larger than "
                   "contiguous size!\");\n";
        outfile << "        return ACLNN_ERR_PARAM_INVALID;\n";
        outfile << "    }\n";
        outfile << "    workspaceSize -= inContWorkspaceSize;\n";
        outfile << "    void *inWorkspace = (char *)workspace + workspaceSize;\n";
        outfile << "    if (aclInExecutor != nullptr) {\n";
        outfile << "        static AclnnFunc aclnnContiguous = (AclnnFunc)NnopbaseGetApiFunc(\"aclnnContiguous\");\n";
        outfile << "        NNOPBASE_ASSERT_NOTNULL_RETVAL(aclnnContiguous);\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(aclnnContiguous(inWorkspace, inContWorkspaceSize, aclInExecutor,"
                   " stream));\n";
        outfile << "    }\n";
        if (HasRef(opDefName.inputsName)) {
            outfile << "    aclOpExecutor *viewcopyExecutor = nullptr;\n";
            outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseGetViewCopyExecutor(executor, &viewcopyExecutor));\n";
            outfile << "    const aclTensorList *viewcopyTensors = nullptr;\n";
            outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseReleaseRefContiguousTensors(executor, "
                       "&viewcopyTensors));\n";
        }
    }
}

void AclnnOpGenerator::AclnnOpGenCodeRunRefUnContWithWorkspaceImpl(OpDef& opDef, OpDefName& opDefName,
                                                                   ofstream& outfile) const
{
    if (IsSupportAutoContiguous(opDef.GetInputs()) && HasRef(opDefName.inputsName)) {
        outfile << "    if (viewcopyExecutor != nullptr) {\n";
        outfile << "        static AclnnFunc aclnnViewCopy = (AclnnFunc)NnopbaseGetApiFunc(\"aclnnViewCopy\");\n";
        outfile << "        NNOPBASE_ASSERT_NOTNULL_RETVAL(aclnnViewCopy);\n";
        outfile << "        NNOPBASE_ASSERT_OK_RETVAL(aclnnViewCopy(inWorkspace, inContWorkspaceSize, viewcopyExecutor,"
                   " stream));\n";
        outfile << "        if (viewcopyTensors != nullptr) {\n";      
        outfile << "            NNOPBASE_ASSERT_OK_RETVAL(aclDestroyTensorList(viewcopyTensors));\n";
        outfile << "        }\n";
        outfile << "    }\n";
    }
}

void AclnnOpGenerator::AclnnOpGenCodeRunWithWorkspaceVersionImpl(OpDefName& opdefName, ofstream& outfile) const
{
    AclnnOpGenCodeRunWithWSFunProto(opdefName.prefixName, outfile);
    outfile << "\n{\n";
    outfile << "    return " << opdefName.maxVersionName <<"(workspace, workspaceSize, executor, stream);\n";
    outfile << "}\n\n";
    AclnnGenCodeImplEnd(outfile);
    outfile.close();
}

void AclnnOpGenerator::AclnnOpGenCodeRunWithWorkspaceImpl(OpDef& opDef, OpDefName& opDefName, ofstream& outfile) const
{
    AclnnOpGenCodeRunWithWSFunProto(opDefName.prefixName, outfile);
    outfile << "\n{\n";
    outfile << "    uint64_t timeStamp = NnopbaseMsprofSysTime();\n";
    outfile << "    static NnopbaseDfxId dfxId = {0x60000, __func__, false};\n";
    AclnnOpGenCodeRunUnContWithWorkspaceImpl(opDef, opDefName, outfile);
    outfile << "    NNOPBASE_ASSERT_OK_RETVAL(NnopbaseRunWithWorkspace(executor, stream, workspace, workspaceSize));\n";
    AclnnOpGenCodeRunRefUnContWithWorkspaceImpl(opDef, opDefName, outfile);
    outfile << "    NnopbaseReportApiInfo(timeStamp, dfxId);\n";
    outfile << "    return ACLNN_SUCCESS;\n";
    outfile << "}\n\n";
    AclnnGenCodeImplEnd(outfile);
    outfile.close();
}

std::vector<std::string> AclnnOpGenerator::AclnnOpGetEnvValue() const
{
    std::string projectName = "aclnn";
    std::string prefixName = "aclnn";
    const char* projectEnv = nullptr;
    MM_SYS_GET_ENV(MM_ENV_OPS_PROJECT_NAME, projectEnv);
    if (projectEnv != nullptr && strlen(projectEnv) != 0) {
        projectName = std::string(projectEnv);
        prefixName = std::string(projectEnv);
        const char* prefixEnv = nullptr;
        MM_SYS_GET_ENV(MM_ENV_OPS_DIRECT_ACCESS_PREFIX, prefixEnv);
        if (prefixEnv != nullptr && strlen(prefixEnv) != 0) {
            prefixName = std::string(prefixEnv);
        }
        if (prefixName[0] >= 'A' && prefixName[0] <= 'Z') {
            prefixName[0] = prefixName[0] + K_DIFF_NUM;
        }
    }

    return {projectName, prefixName};
}

bool AclnnOpGenerator::GetInputConfigVerion(OpDef& opDef, std::set<uint32_t>& versions, uint32_t& maxVersion) const
{
    std::vector<OpParamDef>& params = opDef.GetInputs();
    for (auto& param : params) {
        uint32_t version = param.GetVersion();
        if (version != 0U) {
            if (param.GetParamType() != OPTIONAL) {
                const std::string opType = opDef.GetOpType().GetString();
                Generator::SetErrorMessage(opType + " only optional input can configured version.");
                return false;
            }
            maxVersion = std::max(maxVersion, version);
            versions.insert(version);
        }
    }
    return true;
}

bool AclnnOpGenerator::GetAttrConfigVerion(OpDef& opDef, std::set<uint32_t>& versions, uint32_t& maxVersion) const
{
    std::vector<OpAttrDef>& attrs = opDef.GetAttrs();
    for (auto& attr : attrs) {
        uint32_t version = attr.GetVersion();
        if (version != 0U) {
            if (attr.IsRequired()) {
                const std::string opType = opDef.GetOpType().GetString();
                Generator::SetErrorMessage(opType + " only optional attr can configured version.");
                return false;
            }
            maxVersion = std::max(maxVersion, version);
            versions.insert(version);
        }
    }
    return true;
}

/**
 * 获取版本号，只有可选输入和可选属性可以配置号
 */
bool AclnnOpGenerator::GetConfigVerion(OpDef& opDef, std::set<uint32_t>& versions, uint32_t& maxVersion) const
{
    if (!GetInputConfigVerion(opDef, versions, maxVersion)) {
        return false;
    }

    for (auto& output : opDef.GetOutputs()) {
        if (output.GetVersion() != 0U) {
            const std::string opType = opDef.GetOpType().GetString();
            Generator::SetErrorMessage("The output version of op " + opType + " does not support.");
            return false;
        }
    }

    if (!GetAttrConfigVerion(opDef, versions, maxVersion)) {
        return false;
    }
    return true;
}

void AclnnOpGenerator::AclopGenVersionCode(
    OpDef& opDef, OpDefName& opdefName, std::string prefixName, std::set<uint32_t> versions, uint32_t maxVersion) const
{
    opdefName.maxDecName = opdefName.decName + "_v" + to_string(maxVersion);
    opdefName.maxVersionName = opdefName.prefixName;
    string declMaxFile = opdefName.fileName + "_v" + to_string(maxVersion) + ".h";
    ofstream headerStream = AclnnOpGenHeaderFileStart(declMaxFile, opdefName.macroNmae, maxVersion);
    AclnnOpGenCodeWorkspaceDelcare(opDef, opdefName, headerStream, maxVersion);
    AclnnOpGenHeaderFileDel(declMaxFile, headerStream, false);
    string implFile = opdefName.fileName + "_v" + to_string(maxVersion) + ".cpp";
    ofstream outfile = ofstream(implFile);
    chmod(implFile.c_str(), S_IRUSR | S_IWUSR);
    AclnnOpGenCodeRunForWorkspaceImpl(opDef, opdefName, maxVersion, outfile, false);
    if (IsOpValueDepend(opDef)) {
        AclnnOpGenCodeRunForWorkspaceImpl(opDef, opdefName, maxVersion, outfile, true);
    }
    AclnnOpGenCodeRunWithWorkspaceImpl(opDef, opdefName, outfile);
    for (set<uint32_t>::iterator it = versions.begin(); it != versions.end(); it++) {
        if (*it != maxVersion) {
            opdefName.prefixName = prefixName + opdefName.opName + "V" + to_string(*it);
            string decFileName = opdefName.fileName + "_v" + to_string(*it) + ".h";
            headerStream = AclnnOpGenHeaderFileStart(decFileName, opdefName.macroNmae, *it);
            AclnnOpGenCodeWorkspaceDelcare(opDef, opdefName, headerStream, *it);
            AclnnOpGenHeaderFileDel(decFileName, headerStream, false);
            string implFileName = opdefName.fileName + "_v" +  to_string(*it) + ".cpp";
            ofstream outVersionfile = ofstream(implFileName);
            chmod(implFileName.c_str(), S_IRUSR | S_IWUSR);
            AclnnOpGenCodeRunForWorkspaceVersionImpl(opDef, opdefName, *it, maxVersion, outVersionfile);
            AclnnOpGenCodeRunWithWorkspaceVersionImpl(opdefName, outVersionfile);
        }
    }
    opdefName.prefixName = prefixName + opdefName.opName;
    string decFileName = opdefName.fileName + ".h";
    headerStream = AclnnOpGenHeaderFileStart(decFileName, opdefName.macroNmae, 0U);
    AclnnOpGenCodeWorkspaceDelcare(opDef, opdefName, headerStream, 0U);
    AclnnOpGenHeaderFileDel(decFileName, headerStream, false);
    string implFileName = opdefName.fileName + ".cpp";
    ofstream outVersionfile = ofstream(implFileName);
    chmod(implFileName.c_str(), S_IRUSR | S_IWUSR);
    AclnnOpGenCodeRunForWorkspaceVersionImpl(opDef, opdefName, 0U, maxVersion, outVersionfile);
    AclnnOpGenCodeRunWithWorkspaceVersionImpl(opdefName, outVersionfile);
}

opbuild::Status AclnnOpGenerator::GenerateCode(void)
{
    ASCENDLOGI("Aclnn GenerateCode called!");
    std::string genPath;
    Generator::GetGenPath(genPath);
    uint8_t genFlag = 1U;
    const char* opsAclnntEnv = nullptr;
    MM_SYS_GET_ENV(MM_ENV_OPS_ACLNN_GEN, opsAclnntEnv);
    if (opsAclnntEnv != nullptr) {
        genFlag = std::atoi(opsAclnntEnv);
    }
    std::vector<std::string> projectName = AclnnOpGetEnvValue();
    std::vector<std::string> ops = this->GetAllOp();
    for (const auto& op : ops) {
        OpDef opDef = OpDefFactory::OpDefCreate(op.c_str());
        string opName = opDef.GetOpType().GetString();
        OpDefName opdefName = {};
        opdefName.opName = opName;
        string lowerName = ConvertToSnakeCase(opName);
        opdefName.macroNmae = projectName[0] + "_" + opName;
        opdefName.decName = projectName[0] + "_" + lowerName;
        opdefName.fileName = genPath + "/" + opdefName.decName;
        if (genFlag == 0U) {
            string declFile = opdefName.fileName + ".h";
            string implFile = opdefName.fileName + ".cpp";
            ofstream delOutfile = ofstream(declFile);
            delOutfile.close();
            ofstream implOutfile = ofstream(implFile);
            implOutfile.close();
            continue;
        }
        std::set<uint32_t> versions;
        uint32_t maxVersion = 0U;
        opDef.FollowImpl();
        if (!GetConfigVerion(opDef, versions, maxVersion)) {
            ASCENDLOGE("%s get config version failed!", op.c_str());
            return opbuild::OPBUILD_FAILED;
        }

        if (maxVersion != 0U) {
            opdefName.prefixName = projectName[1] + opdefName.opName + "V" + to_string(maxVersion);
            AclopGenVersionCode(opDef, opdefName, projectName[1], versions, maxVersion);
        } else {
            opdefName.prefixName = projectName[1] + opName;
            opdefName.maxDecName = opdefName.decName;
            string declFile = opdefName.fileName + ".h";
            ofstream headerStream = AclnnOpGenHeaderFileStart(declFile, opdefName.macroNmae, 0U);
            AclnnOpGenCodeWorkspaceDelcare(opDef, opdefName, headerStream, 0U);
            AclnnOpGenHeaderFileDel(opdefName.maxDecName, headerStream, false);
            string implFile = opdefName.fileName + ".cpp";
            ofstream outfile = ofstream(implFile);
            chmod(implFile.c_str(), S_IRUSR | S_IWUSR);
            AclnnOpGenCodeRunForWorkspaceImpl(opDef, opdefName, 0U, outfile, false);
            if (IsOpValueDepend(opDef)) {
                AclnnOpGenCodeRunForWorkspaceImpl(opDef, opdefName, 0U, outfile, true);
            }
            AclnnOpGenCodeRunWithWorkspaceImpl(opDef, opdefName, outfile);
        }
    }
    ASCENDLOGI("Aclnn GenerateCode end!");
    return opbuild::OPBUILD_SUCCESS;
}

AclnnOpGenerator::AclnnOpGenerator(std::vector<std::string>& ops) : Generator(ops)
{
    ASCENDLOGI("Aclnn Generator construct!");
}

static opbuild::Status AclnnOpGeneratorBuilder(std::vector<std::string>& ops)
{
    AclnnOpGenerator g(ops);
    return g.GenerateCode();
}

static void AddAclnnOpGenerator(void) __attribute__((constructor));
void AddAclnnOpGenerator(void)
{
    GeneratorFactory::AddBuilder("aclopnn", AclnnOpGeneratorBuilder);
}
} // namespace ops
