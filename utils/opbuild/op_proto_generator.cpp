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
 * \file op_proto_generator.cpp
 * \brief
 */

#include <fstream>
#include <climits>
#include <map>
#include <sstream>
#include "ascendc_tool_log.h"
#include "op_proto_generator.h"

namespace ops {
void OpProtoGenerator::GenOpRegImplDeclBegin(std::ofstream& outfile, bool isOpProtoH, std::string fileName) const
{
    if (isOpProtoH) {
        std::string macroFileName = GenerateMacros(fileName);
        std::string decName = macroFileName + "_H_";
        outfile << "#ifndef " << decName << std::endl;
        outfile << "#define " << decName << std::endl << std::endl;
        outfile << "#include \"graph/operator_reg.h\"" << std::endl;
        outfile << "#include \"register/op_impl_registry.h\"" << std::endl << std::endl;
        outfile << "namespace ge {" << std::endl << std::endl;
    } else {
        outfile << "#include \"" << fileName << ".h\"" << std::endl;
        outfile << "namespace ge {" << std::endl << std::endl;
    }
}

void OpProtoGenerator::GenOpRegImplDeclEnd(std::ofstream& outfile, bool isOpProtoH) const
{
    outfile << "}" << std::endl << std::endl;
    if (isOpProtoH) {
        outfile << "#endif" << std::endl;
    }
}

std::string OpProtoGenerator::GenOpShapeInferFunc(OpDef& opDef, std::ofstream& outfile) const
{
    std::map<ge::AscendString, OpDef::PortFollowInfo> followStructMap = opDef.GetFollowMap();
    FollowMap shapeMap = opDef.GetFollowShapeMap();
    std::string shapeInferFunctionName("InferShape" + std::string(opDef.GetOpType().GetString()));
    if (opDef.GetInferShape() != nullptr || shapeMap.size() == 0) {
        ASCENDLOGD("op %s has no need to generate shape infer function", opDef.GetOpType().GetString());
        return std::string();
    }
    std::string outFileStr = "";
    outFileStr.append("static ge::graphStatus " + shapeInferFunctionName + "(gert::InferShapeContext* context)\n{\n");
    uint32_t relationAllCount = 0;
    for (const auto& relationship : shapeMap) {
        ge::AscendString rootName = relationship.first;
        auto followList = relationship.second;
        std::string rootVarName = rootName.GetString();
        rootVarName.append("_shape");
        uint32_t index = followStructMap[rootName].index_in;
        std::string relationStr = "";
        relationStr.append("    const gert::Shape* " + rootVarName + " = context->GetInputShape(" +
                            std::to_string(index) + ");\n");
        uint32_t relationCount = 0;
        for (const auto& portPair : followList) {
            std::string paramVarName = portPair.first.GetString();
            if (portPair.second == OpDef::PortStat::IN) {
                ASCENDLOGW(
                    "Input %s can not Follow Shape of Input %s.", paramVarName.c_str(), rootName.GetString());
                continue;
            }
            paramVarName.append("_shape");
            if (paramVarName == rootVarName) {
                paramVarName.append("_out");
            }
            index = followStructMap[rootName].index_out;
            relationStr.append("    gert::Shape* " + paramVarName + " = context->GetOutputShape(" +
                                std::to_string(index) + ");\n");
            relationStr.append("    *" + paramVarName + " = *" + rootVarName +  ";\n");
            ++relationCount;
        }
        if (relationCount > 0) {
            outFileStr.append(relationStr);
            ++relationAllCount;
        }
    }
    if (relationAllCount > 0) {
        outfile << outFileStr << "    return GRAPH_SUCCESS;\n}" << std::endl;
        return "ge::" + shapeInferFunctionName;
    }
    return std::string();
}
void OpProtoGenerator::GenTypeMap(OpDef& opDef, TypeMap& typeMap) const
{
    FollowMap dtypeFollowMap = opDef.GetFollowTypeMap();
    for (const auto& relationship : dtypeFollowMap) {
        ge::AscendString rootName = relationship.first;
        auto followList = relationship.second;
        std::string typeName = rootName.GetString();
        typeName.append("_type");
        OpParamDef rootParam = opDef.GetParamDef(rootName, OpDef::PortStat::IN);
        std::vector<ge::DataType> typeVec = rootParam.GetDataTypes();
        std::set<ge::DataType> typeSet;
        for (const auto& dtype : typeVec) {
            typeSet.emplace(dtype);
        }
        typeMap.emplace(typeName, typeSet);
    }
}

void OpProtoGenerator::GenOpRegImplInputDecl(OpDef& opDef, std::ofstream& outfile, TypeMap& typeMap) const
{
    std::vector<OpParamDef> inputs = opDef.GetInputs();
    for (auto& input : inputs) {
        if (input.GetParamType() == REQUIRED) {
            outfile << "    .INPUT(";
        } else if (input.GetParamType() == OPTIONAL) {
            outfile << "    .OPTIONAL_INPUT(";
        } else if (input.GetParamType() == DYNAMIC) {
            outfile << "    .DYNAMIC_INPUT(";
        } else {
            continue;
        }
        std::string paramName = input.GetParamName().GetString();
        std::string paramTmpName = std::string(paramName + "_type");
        if (typeMap.find(paramTmpName) != typeMap.end()) {
            outfile << paramName << ", \"" << paramTmpName << "\")" << std::endl;
            continue;
        }
        if (input.GetFollowName().GetLength() > 0) {
            ge::AscendString followName = input.GetFollowName();
            std::string typeName = followName.GetString();
            typeName.append("_type");
            outfile << paramName << ", \"" << typeName << "\")" << std::endl;
            continue;
        }
        outfile << paramName << ", ge::TensorType::ALL())" << std::endl;
    }
}
void OpProtoGenerator::GenOpRegImplOutputDecl(OpDef& opDef, std::ofstream& outfile, TypeMap& typeMap) const
{
    std::vector<OpParamDef> outputs = opDef.GetOutputs();
    for (auto& output : outputs) {
        if (output.GetParamType() == REQUIRED) {
            outfile << "    .OUTPUT(";
        } else if (output.GetParamType() == DYNAMIC) {
            outfile << "    .DYNAMIC_OUTPUT(";
        } else if (output.GetParamType() == OPTIONAL) {
            outfile << "    .OUTPUT(";
            ASCENDLOGW("The operator in graph mode does not support optional outputs. The proto library will change \
the optional outputs to required outputs. Please confirm whether the optional output %s of the operator %s can be \
converted into a required output in the graph.", output.GetParamName().GetString(), opDef.GetOpType().GetString());
        } 
        std::string paramName = output.GetParamName().GetString();
        std::string paramTmpName = std::string(paramName + "_type");
        if (typeMap.find(paramTmpName) != typeMap.end()) {
            outfile << paramName << ", \"" << paramTmpName << "\")" << std::endl;
            continue;
        }
        if (output.GetFollowName().GetLength() > 0) {
            ge::AscendString followName = output.GetFollowName();
            std::string typeName = followName.GetString();
            typeName.append("_type");
            outfile << paramName << ", \"" << typeName << "\")" << std::endl;
            continue;
        }
        outfile << paramName << ", ge::TensorType::ALL())" << std::endl;
    }
}
void OpProtoGenerator::GenOpRegImplTypeDecl(std::ofstream& outfile, TypeMap& typeMap) const
{
    if (typeMap.size() == 0) {
        return;
    }
    for (auto& typeRelationShip : typeMap) {
        std::string typeName = typeRelationShip.first;
        std::set<ge::DataType> typeSet = typeRelationShip.second;
        outfile << "    .DATATYPE(" << typeName << ", ge::TensorType({";
        std::string typeListStr = "";
        for (auto& type : typeSet) {
            std::string typeStr = "";
            if (this->dtypeSuppMap.find(static_cast<int>(type)) != this->dtypeSuppMap.end()) {
                typeStr = this->dtypeSuppMap.find(static_cast<int>(type))->second;
            }
            typeListStr.append(typeStr + ", ");
        }
        typeListStr = typeListStr.substr(0, typeListStr.length() - this->gapSize);
        outfile << typeListStr << "}))" << std::endl;
    }
}
void OpProtoGenerator::GenOpRegImplAttrDecl(OpDef& opDef, std::ofstream& outfile) const
{
    std::vector<OpAttrDef> attrs = opDef.GetAttrs();
    if (attrs.size() == 0) {
        return;
    }
    for (auto& attr : attrs) {
        if (attr.IsRequired()) {
            outfile << "    .REQUIRED_ATTR(";
            outfile << attr.GetName().GetString() << ", " << attr.GetProtoDataType().GetString() << ")" << std::endl;
        } else {
            std::string attrType = attr.GetProtoDataType().GetString();
            std::string attrVal = attr.GetAttrDefaultVal("{}").GetString();
            if (attrType == "String") {
                attrVal = "\"" + attrVal + "\"";
            }
            outfile << "    .ATTR(";
            outfile << attr.GetName().GetString() << ", " << attrType << ", " << attrVal << ")" << std::endl;
        }
    }
}
std::string OpProtoGenerator::GenCommentLine(const std::string& str) const
{
    std::stringstream ssComment(str);
    std::string line = "";
    std::string comment = "";
    bool isFirstLine = true;
    while (std::getline(ssComment, line)) {
        if (isFirstLine) {
            isFirstLine = false;
        } else {
            comment.append("* ");
        }
        comment.append(line + "\n");
    }
    return comment;
}
std::string OpProtoGenerator::GenCommentFormat(const std::vector<std::string>& commentList) const
{
    std::string formatComment;
    bool isList = commentList.size() > 1;
    for (const auto& comment : commentList) {
        std::string head = "* ";
        if (isList) {
            head.append("@li ");
        }
        formatComment.append(head + this->GenCommentLine(comment));
    }
    return formatComment;
}
std::string OpProtoGenerator::GenOpCommentInput(OpDef& opDef) const
{
    std::vector<OpParamDef> inputs = opDef.GetInputs();
    std::string inputComment = "* @par Inputs:\n";
    std::vector<std::string> inputCommentList;
    for (const auto& opParam : inputs) {
        std::string comment = opParam.GetComment().GetString();
        if (comment.empty()) {
            continue;
        }
        std::string paramName = opParam.GetParamName().GetString();
        inputCommentList.emplace_back(paramName + ": " + comment);
    }
    if (inputCommentList.size() == 0) {
        return std::string();
    }
    inputComment.append(this->GenCommentFormat(inputCommentList));
    return inputComment;
}
std::string OpProtoGenerator::GenOpCommentAttr(OpDef& opDef) const
{
    std::vector<OpAttrDef> attrs = opDef.GetAttrs();
    std::string attrComment = "* @par Attributes:\n";
    std::vector<std::string> attrCommentList;
    for (const auto& attr : attrs) {
        std::string comment = attr.GetComment().GetString();
        if (comment.empty()) {
            continue;
        }
        std::string name = attr.GetName().GetString();
        attrCommentList.emplace_back(name + ": " + comment);
    }
    if (attrCommentList.size() == 0) {
        return std::string();
    }
    attrComment.append(this->GenCommentFormat(attrCommentList));
    return attrComment;
}
std::string OpProtoGenerator::GenOpCommentOutput(OpDef& opDef) const
{
    std::vector<OpParamDef> outputs = opDef.GetOutputs();
    std::string outputComment = "* @par Outputs:\n";
    std::vector<std::string> outputCommentList;
    for (const auto& opParam : outputs) {
        std::string comment = opParam.GetComment().GetString();
        if (comment.empty()) {
            continue;
        }
        std::string paramName = opParam.GetParamName().GetString();
        outputCommentList.emplace_back(paramName + ": " + comment);
    }
    if (outputCommentList.size() == 0) {
        return std::string();
    }
    outputComment.append(this->GenCommentFormat(outputCommentList));
    return outputComment;
}
std::string OpProtoGenerator::GenOpCommentSee(OpDef& opDef) const
{
    if (opDef.GetSee().size() == 0) {
        return std::string();
    }
    std::string seeComment = "* @see ";
    std::vector<ge::AscendString> seeList = opDef.GetSee();
    bool isList = seeList.size() > 1;
    if (!isList) {
        std::string comment = seeList[0].GetString();
        seeComment.append(this->GenCommentLine(comment));
        return seeComment;
    }
    seeComment.append("\n");
    for (const auto& see : seeList) {
        std::string head = "* @li ";
        std::string comment = see.GetString();
        seeComment.append(head + this->GenCommentLine(comment));
    }
    return seeComment;
}
std::string OpProtoGenerator::GenOpCommentOverview(OpDef& opDef, ops::CommentSection section) const
{
    std::vector<ge::AscendString> commentList;
    if (section == ops::CommentSection::BRIEF) {
        commentList = opDef.GetBrief();
    }
    if (section == ops::CommentSection::CONSTRAINTS) {
        commentList = opDef.GetConstraints();
    }
    if (section == ops::CommentSection::RESTRICTIONS) {
        commentList = opDef.GetRestrictions();
    }
    if (section == ops::CommentSection::THIRDPARTYFWKCOMPAT) {
        commentList = opDef.GetThirdPartyFwkCopat();
    }
    if (commentList.size() == 0) {
        return std::string();
    }
    std::vector<std::string> commentStrList;
    for (const auto& str : commentList) {
        commentStrList.emplace_back(str.GetString());
    }
    std::string allComment = this->sectionToHead.find(section)->second;
    allComment.append(this->GenCommentFormat(commentStrList));
    return allComment;
}
std::vector<std::string> OpProtoGenerator::GetOpCommentList(OpDef& opDef) const
{
    std::vector<std::string> commentList;
    auto addCommentList = [&commentList](const std::string& cmt) {
        if (!cmt.empty()) {
            commentList.emplace_back(cmt);
        }
    };
    addCommentList(this->GenOpCommentOverview(opDef, ops::CommentSection::BRIEF));
    addCommentList(this->GenOpCommentInput(opDef));
    addCommentList(this->GenOpCommentAttr(opDef));
    addCommentList(this->GenOpCommentOutput(opDef));
    addCommentList(this->GenOpCommentOverview(opDef, ops::CommentSection::CONSTRAINTS));
    addCommentList(this->GenOpCommentOverview(opDef, ops::CommentSection::RESTRICTIONS));
    addCommentList(this->GenOpCommentSee(opDef));
    addCommentList(this->GenOpCommentOverview(opDef, ops::CommentSection::THIRDPARTYFWKCOMPAT));
    return commentList;
}
void OpProtoGenerator::GenOpComment(OpDef& opDef, std::ofstream& outfile) const
{
    std::vector<std::string> commentList = this->GetOpCommentList(opDef);
    if (commentList.empty()) {
        ASCENDLOGD("%s comment is null", opDef.GetOpType().GetString());
        return;
    }
    outfile << "/**" << std::endl;
    for (const auto& comment : commentList) {
        outfile << comment;
        outfile << "*" << std::endl;
    }
    outfile << "*/" << std::endl;
}
void OpProtoGenerator::GenOpRegImplDeclOp(OpDef& opDef, std::ofstream& outfile) const
{
    this->GenOpComment(opDef, outfile);
    outfile << "REG_OP(" << opDef.GetOpType().GetString() << ")" << std::endl;
    TypeMap typeMap;
    this->GenTypeMap(opDef, typeMap);
    this->GenOpRegImplInputDecl(opDef, outfile, typeMap);
    this->GenOpRegImplOutputDecl(opDef, outfile, typeMap);
    this->GenOpRegImplTypeDecl(outfile, typeMap);
    this->GenOpRegImplAttrDecl(opDef, outfile);
    outfile << "    .OP_END_FACTORY_REG(" << opDef.GetOpType().GetString() << ");" << std::endl << std::endl;
}

void OpProtoGenerator::GenOpRegImpl(OpDef& opDef, std::ofstream& outfile,
    const std::vector<std::pair<int32_t, DependScope>>& valueDependIndexList,
    const std::vector<int32_t>& outShapeDependIndexList) const
{
    std::string shapeInferName = GenOpShapeInferFunc(opDef, outfile);
    if (valueDependIndexList.size() == 0 && outShapeDependIndexList.size() == 0 && shapeInferName.size() == 0) {
        return;
    }
    std::string indexInputList = "";
    std::string indexInputListTiling = "";
    std::string indexOutputList = "";
    std::string opType = opDef.GetOpType().GetString();
    outfile << "IMPL_OP(" << opType << ")";
    for (const auto& inputInfo : valueDependIndexList) {
        if (inputInfo.second == DependScope::ALL) {
            indexInputList.append(std::to_string(inputInfo.first) + ", ");
        }
        if (inputInfo.second == DependScope::TILING) {
            indexInputListTiling.append(std::to_string(inputInfo.first) + ", ");
        }
    }
    std::string outFileStr = GenOpRegImplSection(indexInputList, indexInputListTiling, opType);
    if (outShapeDependIndexList.size() != 0) {
        for (size_t i = 0; i < (outShapeDependIndexList.size() - 1); ++i) {
            indexOutputList = indexOutputList + std::to_string(outShapeDependIndexList[i]) + ", ";
        }
        indexOutputList += std::to_string(outShapeDependIndexList[outShapeDependIndexList.size() - 1]);
        if (outFileStr.size() > 0) {
            outFileStr.append("\n         " + std::string(opType.size(), ' '));
        }
        outFileStr.append(".OutputShapeDependOnCompute({" + indexOutputList + "})");
    }
    if (shapeInferName.size() > 0) {
        if (outFileStr.size() > 0) {
            outFileStr.append("\n         " + std::string(opType.size(), ' '));
        }
        outFileStr.append(".InferShape(" + shapeInferName + ")");
    }
    outfile << outFileStr << ";" << std::endl;
}
std::string OpProtoGenerator::GenOpRegImplSection(
    std::string &indexInputList, std::string &indexInputListTiling, const std::string &opType) const
{
    std::string outFileStr = "";
    if (indexInputList.size() > 0) {
        indexInputList = indexInputList.substr(0, indexInputList.length() - this->gapSize);
        outFileStr.append(".InputsDataDependency({" + indexInputList + "})");
    }
    if (indexInputListTiling.size() > 0) {
        indexInputListTiling = indexInputListTiling.substr(0, indexInputListTiling.length() - this->gapSize);
        if (outFileStr.size() > 0) {
            outFileStr.append("\n         " + std::string(opType.size(), ' '));
        }
        outFileStr.append(".TilingInputsDataDependency({" + indexInputListTiling + "}");
        if (OpDefFactory::OpIsTilingSink(opType.c_str())) {
            outFileStr.append(", {gert::TilingPlacement::TILING_ON_HOST, gert::TilingPlacement::TILING_ON_AICPU}");
        }
        outFileStr.append(")");
    }
    return outFileStr;
}
std::vector<std::pair<int32_t, DependScope>> OpProtoGenerator::GetInputDataDependIndexList(OpDef& opDef) const
{
    std::vector<OpParamDef> inputList = opDef.GetInputs();
    std::string opType = opDef.GetOpType().GetString();
    bool isTilingSink = OpDefFactory::OpIsTilingSink(opType.c_str());
    if (!isTilingSink) {
        ASCENDLOGI("TilingSink Check: OpType: %s is not TilingSink.", opType.c_str());
    } else {
        ASCENDLOGI("TilingSink Check: OpType: %s is set TilingSink.", opType.c_str());
    }
    std::vector<std::pair<int32_t, DependScope>> valueDependIndexList;
    int32_t inputIndex = 0;
    for (auto& input : inputList) {
        if (input.GetValueDepend() != ge::AscendString("")) {
            ASCENDLOGI("OpType %s, Input %d set ValueDepend.", opType.c_str(), inputIndex);
            valueDependIndexList.emplace_back(inputIndex, input.GetDependScope());
        }
        if (isTilingSink && input.GetDependScope() == DependScope::TILING &&
            input.GetValueDepend() != ge::AscendString("optional")) {
            ASCENDLOGE("Input %d Set TilingSink error, ValueDepend must be set OPTIONAL", inputIndex);
        }
        inputIndex++;
    }
    return valueDependIndexList;
}

std::vector<int32_t> OpProtoGenerator::GetOutputShapeDependOnComputeIndexList(OpDef& opDef) const
{
    std::vector<OpParamDef> outputList = opDef.GetOutputs();
    std::vector<int32_t> outShapeDependIndexList;
    int32_t outputIndex = 0;
    for (auto& output : outputList) {
        if (output.IsOutputShapeDependOnCompute() == true) {
            outShapeDependIndexList.push_back(outputIndex);
        }
        outputIndex++;
    }
    return outShapeDependIndexList;
}
opbuild::Status OpProtoGenerator::GetFile(std::ofstream& fileH, std::ofstream& fileCC, const std::string& catg) {
    std::string filePath = fileGenPath;
    if (catg != std::string("op_proto")) {
        filePath.append("/group_proto");
    }
    char resolvedGenPath[PATH_MAX] = {0};
    if (realpath(filePath.c_str(), resolvedGenPath) == nullptr) {
        ASCENDLOGE("Path: %s is invalid!", filePath.c_str());
        std::cerr << "Path: " << filePath << " is not valid!" << std::endl;
        return opbuild::OPBUILD_FAILED;
    }
    filePath = std::string(resolvedGenPath);
    if (filePath.size() > PATH_MAX) {
        ASCENDLOGE("ProtoFile Path Length Exceeded the limit!!!");
        return opbuild::OPBUILD_FAILED;
    }
    if (this->protoCatgNames.find(catg) == this->protoCatgNames.end()) {
        fileH.open(filePath + "/" + catg + ".h");
        fileCC.open(filePath + "/" + catg + ".cc");
        GenOpRegImplDeclBegin(fileH, true, catg);
        GenOpRegImplDeclBegin(fileCC, false, catg);
        this->protoCatgNames.emplace(catg);
        return opbuild::OPBUILD_SUCCESS;
    }
    fileH.open(filePath + "/" + catg + ".h", std::fstream::app);
    fileCC.open(filePath + "/" + catg + ".cc", std::fstream::app);
    return opbuild::OPBUILD_SUCCESS;
}
opbuild::Status OpProtoGenerator::GenerateCode(void)
{
    ASCENDLOGI("OpProto GenerateCode called!");
    std::string genPath;
    Generator::GetGenPath(genPath);
    char resolvedGenPath[PATH_MAX] = {0};
    if (realpath(genPath.c_str(), resolvedGenPath) == nullptr) {
        ASCENDLOGE("Generate Path %s is invalid!", genPath.c_str());
        std::cerr << "Path: " << genPath << " is not valid!" << std::endl;
        return opbuild::OPBUILD_FAILED;
    }
    fileGenPath = std::string(resolvedGenPath);
    std::ofstream opProtoInitH;
    std::ofstream opProtoInitCc;
    opbuild::Status result = GetFile(opProtoInitH, opProtoInitCc, "op_proto");
    if (result == opbuild::OPBUILD_FAILED) {
        ASCENDLOGW("Opbuild: open op_proto file Failed!");
    }
    opProtoInitH.close();
    opProtoInitCc.close();
    std::vector<std::string> ops = this->GetAllOp();
    for (const auto& op : ops) {
        OpDef opDef = OpDefFactory::OpDefCreate(op.c_str());
        ASCENDLOGD("OpProtoGen: %s", opDef.GetOpType().GetString());
        std::vector<std::pair<int32_t, DependScope>> valueDependIndexList = GetInputDataDependIndexList(opDef);
        std::vector<int32_t> outShapeDependIndexList = GetOutputShapeDependOnComputeIndexList(opDef);
        opDef.FollowImpl();
        std::string catgName = opDef.GetCateGory().GetString();
        std::ofstream opProtoH;
        std::ofstream opProtoCc;
        result = GetFile(opProtoH, opProtoCc, catgName);
        GenOpRegImplDeclOp(opDef, opProtoH);
        GenOpRegImpl(opDef, opProtoCc, valueDependIndexList, outShapeDependIndexList);
        opProtoH.close();
        opProtoCc.close();
    }
    for (const auto& catgName : protoCatgNames) {
        std::ofstream opProtoH;
        std::ofstream opProtoCc;
        GetFile(opProtoH, opProtoCc, catgName);
        GenOpRegImplDeclEnd(opProtoH, true);
        GenOpRegImplDeclEnd(opProtoCc, false);
        opProtoH.close();
        opProtoCc.close();
    }
    ASCENDLOGI("OpProto GenerateCode end!");
    return opbuild::OPBUILD_SUCCESS;
}

opbuild::Status OpProtoGenerator::GenerateCodeSeparate(void)
{
    ASCENDLOGI("OpProto GenerateCodeSeparate called!");
    std::string genPath;
    Generator::GetGenPath(genPath);
    std::vector<std::string> ops = this->GetAllOp();
    char resolvedGenPath[PATH_MAX] = {0};
    if (realpath(genPath.c_str(), resolvedGenPath) == nullptr) {
        ASCENDLOGE("Generate Path %s is invalid!", genPath.c_str());
        std::cerr << "Path: " << genPath << " is not valid!" << std::endl;
        return opbuild::OPBUILD_FAILED;
    }
    for (const auto& op : ops) {
        OpDef opDef = OpDefFactory::OpDefCreate(op.c_str());
        ASCENDLOGD("OpProtoGen: %s", opDef.GetOpType().GetString());
        std::string opName = opDef.GetOpType().GetString();
        std::string lowerName = ConvertToSnakeCase(opName);
        std::string opProtoFileName = lowerName + "_proto";
        std::ofstream opProtoH(std::string(resolvedGenPath) + "/" + opProtoFileName + ".h");
        std::ofstream opProtoCc(std::string(resolvedGenPath) + "/" + opProtoFileName + ".cpp");
        GenOpRegImplDeclBegin(opProtoH, true, opProtoFileName);
        GenOpRegImplDeclBegin(opProtoCc, false, opProtoFileName);
        std::vector<std::pair<int32_t, DependScope>> valueDependIndexList = GetInputDataDependIndexList(opDef);
        std::vector<int32_t> outShapeDependIndexList = GetOutputShapeDependOnComputeIndexList(opDef);
        opDef.FollowImpl();
        GenOpRegImplDeclOp(opDef, opProtoH);
        GenOpRegImpl(opDef, opProtoCc, valueDependIndexList, outShapeDependIndexList);
        GenOpRegImplDeclEnd(opProtoH, true);
        GenOpRegImplDeclEnd(opProtoCc, false);
    }
    ASCENDLOGI("OpProto GenerateCodeSeparate end!");
    return opbuild::OPBUILD_SUCCESS;
}

OpProtoGenerator::OpProtoGenerator(std::vector<std::string>& ops) : Generator(ops)
{
    ASCENDLOGI("OpProto Generator construct!");
}

static opbuild::Status OpProtoGeneratorBuilder(std::vector<std::string>& ops)
{
    OpProtoGenerator g(ops);
    const char* projectEnv = std::getenv("OPS_PROTO_SEPARATE");
    bool genSeparate = false;
    if (projectEnv != nullptr && std::strcmp(projectEnv, "1") == 0) {
        genSeparate = true;
    }
    if (genSeparate) {
        return g.GenerateCodeSeparate();
    } else {
        return g.GenerateCode();
    }
    return opbuild::OPBUILD_FAILED;
}

static void AddOpProtoGenerator(void) __attribute__((constructor));
void AddOpProtoGenerator(void)
{
    GeneratorFactory::AddBuilder("op_proto", OpProtoGeneratorBuilder);
}
}
