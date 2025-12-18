/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/
#include <gtest/gtest.h>
#include <cmath>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unistd.h>
#define private public
#define protected public
#include "mockcpp/mockcpp.hpp"
#include "op_cfg_generator.h"
#include "op_proto_generator.h"
#include "op_aclnn_generator.h"
#include "op_build_params.h"
#include "op_aclnn_fallback_generator.h"
#include "op_custom_registry_generator.h"
#include "register/op_def_registry.h"
#include "register/device_op_impl_registry.h"

extern int opbuild_main(int argc, std::vector<std::string> args);

namespace ops {
class TEST_OPBUILD : public testing::Test {
protected:
    void SetUp() {
        opbuild::Params::GetInstance().optionParams_ = {};
        opbuild::Params::GetInstance().requiredParams_ = {};
    }
    void TearDown() {
        GlobalMockObject::verify();
        opbuild::Params::GetInstance().optionParams_ = {};
        opbuild::Params::GetInstance().requiredParams_ = {};
    }
};

TEST_F(TEST_OPBUILD, OpBuildCoverage)
{
    Generator::SetGenPath("/ajofdij/jfoasj");
    std::vector<std::string> opsvec({"Adds"});
    OpProtoGenerator opProtoGen(opsvec);
    opProtoGen.GenerateCode();
    opProtoGen.GenerateCodeSeparate();
    std::string genPath = "";
    Generator::GetGenPath(genPath);
    EXPECT_EQ(genPath,"/ajofdij/jfoasj");
    CfgGenerator cfgGen(opsvec);
    cfgGen.GenerateCode();
    Generator::GetGenPath(genPath);
    EXPECT_EQ(genPath,"/ajofdij/jfoasj");
}

TEST_F(TEST_OPBUILD, OpBuildCoverageModeTest)
{
    Generator::SetCPUMode("--aicore");
    std::vector<std::string> opsvec({"Adds"});
    OpProtoGenerator opProtoGen(opsvec);
    opProtoGen.GenerateCode();
    opProtoGen.GenerateCodeSeparate();
    std::string genMode = "";
    Generator::GetCPUMode(genMode);
    EXPECT_EQ(genMode,"--aicore");
    CfgGenerator cfgGen(opsvec);
    std::string path = "";
    opbuild::Status status = Generator::SetCPUMode(path);
    EXPECT_EQ(status, opbuild::OPBUILD_FAILED);
    path = "core";
    status = Generator::SetCPUMode(path);
    EXPECT_EQ(status, opbuild::OPBUILD_FAILED);
}

TEST_F(TEST_OPBUILD, NotGenerateAclnnInterface)
{
    setenv("OPS_ACLNN_GEN", "0", 1);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    EXPECT_TRUE(nullptr != so_path);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    EXPECT_EQ(ret, 0);
    unsetenv("OPS_ACLNN_GEN");
}

TEST_F(TEST_OPBUILD, OpBuildFailLogTest)
{
    char buf[1024];
    char* cur_path = getcwd(buf, 1023);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    char* src_path = getenv("OPS_SRC_FILE_PATH");
    EXPECT_TRUE(nullptr != so_path);
    EXPECT_TRUE(nullptr != src_path);
    setenv("ASCEND_VENDOR_NAME", "customize", 1);
    int ret = opbuild_main(3, { "opbuild", "aabb", cur_path });
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 1);
    ret = opbuild_main(3, { "opbuild", so_path, "" });
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 1);
    unsetenv("ASCEND_VENDOR_NAME");
    ret = opbuild_main(3, { "opbuild", so_path, cur_path });
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 0);
    setenv("ASCEND_VENDOR_NAME", "customize", 1);
    std::vector<std::string> opvec = {"aa", "bb"};
    Generator gen(opvec);
    EXPECT_EQ(gen.GenerateCode(), opbuild::OPBUILD_SUCCESS);
    OpCustomGenerator custGen(opvec);
    MOCKER(realpath, char *(*)(const char *, char *)).times(2).will(returnValue((char*)nullptr));
    EXPECT_EQ(custGen.GenerateCode(), opbuild::OPBUILD_FAILED);
    ret = opbuild_main(3, { "opbuild", so_path, cur_path });
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 1);
}

TEST_F(TEST_OPBUILD, OpBuildProtoFailLog)
{
    char buf[1024];
    char* cur_path = getcwd(buf, 1023);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    char* src_path = getenv("OPS_SRC_FILE_PATH");
    std::cout << "OPS DSO: " << so_path << std::endl;
    std::cout << "OPS SRC: " << src_path << std::endl;
    EXPECT_TRUE(nullptr != so_path);
    EXPECT_TRUE(nullptr != src_path);
    setenv("ASCEND_VENDOR_NAME", "customize", 1);
    std::vector<std::string> opvec = {"ProtoLogFail"};
    OpProtoGenerator protoGen(opvec);
    protoGen.fileGenPath = cur_path;
    std::ofstream logH, logC;
    EXPECT_EQ(protoGen.GetFile(logH, logC, std::string("invalid:filename*")), opbuild::OPBUILD_FAILED);
}

TEST_F(TEST_OPBUILD, OpBuildRun)
{
    char buf[1024];
    char* cur_path = getcwd(buf, 1023);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    char* src_path = getenv("OPS_SRC_FILE_PATH");
    std::cout << "OPS DSO: " << so_path << std::endl;
    std::cout << "OPS SRC: " << src_path << std::endl;
    EXPECT_TRUE(nullptr != so_path);
    EXPECT_TRUE(nullptr != src_path);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    EXPECT_EQ(ret, 0);
    std::string src_file, gen_file;
    std::ifstream src_if, gen_if;
    std::stringstream src_ss, gen_ss;
    std::vector<std::string> files = { "/op_proto.cc", "/op_proto.h", "/aic-ascend310p-ops-info.ini",
        "/aic-ascend910-ops-info.ini" };
    std::string insConvert = "ABcd_Efg";
    std::string covertStr;
    auto convertString = ConvertToSnakeCase(insConvert);
    std::string emptyStringIn = "";
    std::string emptyStringOut = ConvertToSnakeCase(emptyStringIn);
    EXPECT_EQ(emptyStringOut, "");
    for (auto& file : files) {
        src_file = std::string(src_path) + file + ".txt";
        gen_file = std::string(cur_path) + file;
        std::cout << "compare " << src_file << " and " << gen_file << std::endl;
        src_if.open(src_file);
        EXPECT_TRUE(src_if.is_open());
        src_ss << src_if.rdbuf();
        gen_if.open(gen_file);
        EXPECT_TRUE(gen_if.is_open());
        gen_ss << gen_if.rdbuf();
        EXPECT_EQ(src_ss.str(), gen_ss.str());
        src_if.close();
        gen_if.close();
    }
}

TEST_F(TEST_OPBUILD, OpBuildRunTest)
{
    char buf[1024];
    char* cur_path = getcwd(buf, 1023);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    char* src_path = getenv("OPS_SRC_FILE_PATH");
    std::cout << "OPS DSO: " << so_path << std::endl;
    std::cout << "OPS SRC: " << src_path << std::endl;
    EXPECT_TRUE(nullptr != so_path);
    EXPECT_TRUE(nullptr != src_path);
    int ret = opbuild_main(4, { "opbuild", so_path, "." , "--aicore"});
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 0);
    ret = opbuild_main(4, { "opbuild", so_path, "." , "--aicpu"});
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 0);
    ret = opbuild_main(5, { "opbuild", so_path, "." , "--aicpu", "--output_file=./test"});
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 0);
    ret = opbuild_main(4, { "opbuild", so_path, "." , "--aic"});
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 1);
    ret = opbuild_main(5, { "opbuild", so_path, "." , "--aic", "--output_file=./test"});
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 1);
    ret = opbuild_main(5, { "opbuild", so_path, "." , "--aicore", "--output_file="});
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 1);
    ret = opbuild_main(4, { "opbuild", so_path, cur_path, "--aic"});
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 1);
    ret = opbuild_main(5, { "opbuild", so_path, cur_path, cur_path, "--output_file="});
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 1);
}

TEST_F(TEST_OPBUILD, CustomOpRegistrySuccess)
{
    char buf[1024];
    char* cur_path = getcwd(buf, 1023);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    char* src_path = getenv("OPS_SRC_FILE_PATH");
    std::cout << "OPS DSO: " << so_path << std::endl;
    std::cout << "OPS SRC: " << src_path << std::endl;
    EXPECT_TRUE(nullptr != so_path);
    EXPECT_TRUE(nullptr != src_path);
    setenv("ASCEND_VENDOR_NAME", "customize", 1);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 0);
    setenv("ASCEND_VENDOR_NAME", "", 1);
    ret = opbuild_main(3, { "opbuild", so_path, "." });
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    EXPECT_EQ(ret, 0);
    std::string src_file, gen_file;
    std::ifstream src_if, gen_if;
    std::stringstream src_ss, gen_ss;
    std::string file = "/custom_op_registry.cpp";
    src_file = std::string(src_path) + file + ".txt";
    gen_file = std::string(cur_path) + file;
    std::cout << "compare " << src_file << " and " << gen_file << std::endl;
    src_if.open(src_file);
    EXPECT_TRUE(src_if.is_open());
    src_ss << src_if.rdbuf();
    gen_if.open(gen_file);
    EXPECT_TRUE(gen_if.is_open());
    gen_ss << gen_if.rdbuf();
    EXPECT_EQ(src_ss.str(), gen_ss.str());
    src_if.close();
    gen_if.close();
}

TEST_F(TEST_OPBUILD, OpBuildRunProtoSeparate)
{
    char buf[1024];
    char* cur_path = getcwd(buf, 1023);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    char* src_path = getenv("OPS_SRC_FILE_PATH");
    std::cout << "OPS DSO: " << so_path << std::endl;
    std::cout << "OPS SRC: " << src_path << std::endl;
    EXPECT_TRUE(nullptr != so_path);
    EXPECT_TRUE(nullptr != src_path);
    setenv("OPS_PROTO_SEPARATE", "1", 1);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    EXPECT_EQ(ret, 0);
    std::string src_file, gen_file;
    std::ifstream src_if, gen_if;
    std::stringstream src_ss, gen_ss;
    std::vector<std::string> files = {
        "/add_tik2_proto.cpp",
        "/add_tik2_proto.h",
        "/auto_contiguous_test_proto.cpp",
        "/auto_contiguous_test_proto.h",
        "/dynamic_ref_test_proto.cpp",
        "/dynamic_ref_test_proto.h",
        "/input_test_proto.cpp",
        "/input_test_proto.h",
        "/output_test_proto.cpp",
        "/output_test_proto.h",
        "/ref_test_proto.cpp",
        "/ref_test_proto.h",
        "/scalar_test_proto.cpp",
        "/scalar_test_proto.h",
        "/value_depend_test_proto.cpp",
        "/value_depend_test_proto.h",
        "/version_test_proto.cpp",
        "/version_test_proto.h",
    };
    for (auto& file : files) {
        src_file = std::string(src_path) + "/op_proto_separate" + file + ".txt";
        gen_file = std::string(cur_path) + file;
        std::cout << "compare " << src_file << " and " << gen_file << std::endl;
        src_if.open(src_file);
        EXPECT_TRUE(src_if.is_open());
        src_ss << src_if.rdbuf();
        gen_if.open(gen_file);
        EXPECT_TRUE(gen_if.is_open());
        gen_ss << gen_if.rdbuf();
        EXPECT_EQ(src_ss.str(), gen_ss.str());
        src_if.close();
        gen_if.close();
    }
    unsetenv("OPS_PROTO_SEPARATE");
}

TEST_F(TEST_OPBUILD, OutPutFollowInputUt)
{
    class OutputFollowInput : public OpDef {
    public:
        OutputFollowInput(const char* name) : OpDef(name)
        {
            this->Input("x")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND})
                .ValueDepend(OPTIONAL, DependScope::TILING);
            this->Input("y")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND})
                .ValueDepend(OPTIONAL, DependScope::TILING);
            this->Output("z")
                .ParamType(REQUIRED)
                .Follow("y");
            OpAICoreConfig aicConfig;
            this->AICore().AddConfig("ascend910b", aicConfig);
        }
    };
    OP_ADD(OutputFollowInput);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    EXPECT_TRUE(nullptr != so_path);
    setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    char buf[1024];
    char *cur_path = getcwd(buf, 1023);
    std::string gen_file = std::string(cur_path) + "/op_proto.cc";
    std::ifstream gen_if;
    std::stringstream gen_ss;
    gen_if.open(gen_file);
    EXPECT_TRUE(gen_if.is_open());
    gen_ss << gen_if.rdbuf();
    EXPECT_NE(gen_ss.str().find("InferShapeOutputFollowInput(gert::InferShapeContext* context)"), std::string::npos);
    gen_if.close();
    system(("rm -rf " + gen_file).c_str());
    unsetenv("ENABLE_SOURCE_PACKAGE");
}

TEST_F(TEST_OPBUILD, InputFollowInputUt)
{
    class InputFollowInput : public OpDef {
    public:
        InputFollowInput(const char* name) : OpDef(name)
        {
            this->Input("x")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND})
                .ValueDepend(OPTIONAL, DependScope::TILING);
            this->Input("y")
                .ParamType(REQUIRED)
                .Follow("x");
            this->Output("z")
                .ParamType(REQUIRED)
                .Follow("y");
            OpAICoreConfig aicConfig;
            this->AICore().AddConfig("ascend910b", aicConfig);
        }
    };
    OP_ADD(InputFollowInput);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    EXPECT_TRUE(nullptr != so_path);
    setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    char buf[1024];
    char *cur_path = getcwd(buf, 1023);
    std::string gen_file = std::string(cur_path) + "/op_proto.cc";
    std::ifstream gen_if;
    std::stringstream gen_ss;
    gen_if.open(gen_file);
    EXPECT_TRUE(gen_if.is_open());
    gen_ss << gen_if.rdbuf();
    EXPECT_NE(gen_ss.str().find("InferShapeInputFollowInput(gert::InferShapeContext* context)"), std::string::npos);
    gen_if.close();
    system(("rm -rf " + gen_file).c_str());
    unsetenv("ENABLE_SOURCE_PACKAGE");
}

TEST_F(TEST_OPBUILD, ValueDependTilingSinkUt)
{
    class ValueDependTilingSink : public OpDef {
    public:
        ValueDependTilingSink(const char* name) : OpDef(name)
        {
            this->Input("x")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND})
                .ValueDepend(OPTIONAL, DependScope::TILING);
            this->Input("y")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND});
            this->Output("z")
                .ParamType(REQUIRED)
                .Follow("y");
            OpAICoreConfig aicConfig;
            this->AICore().AddConfig("ascend910b", aicConfig);
        }
    };
    OP_ADD(ValueDependTilingSink);
    optiling::DeviceOpImplRegister deviceOpImplRegister = optiling::DeviceOpImplRegister("ValueDependTilingSink");
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    EXPECT_TRUE(nullptr != so_path);
    setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    char buf[1024];
    char *cur_path = getcwd(buf, 1023);
    std::string gen_file = std::string(cur_path) + "/op_proto.cc";
    std::ifstream gen_if;
    std::stringstream gen_ss;
    gen_if.open(gen_file);
    EXPECT_TRUE(gen_if.is_open());
    gen_ss << gen_if.rdbuf();
    EXPECT_NE(gen_ss.str().find("{gert::TilingPlacement::TILING_ON_HOST, gert::TilingPlacement::TILING_ON_AICPU}"), std::string::npos);
    gen_if.close();
    system(("rm -rf " + gen_file).c_str());
    unsetenv("ENABLE_SOURCE_PACKAGE");
}

TEST_F(TEST_OPBUILD, OpDefCommentUt)
{
    class OpDefComment : public OpDef {
    public:
        OpDefComment(const char* name) : OpDef(name)
        {
            this->Comment(CommentSection::BRIEF, "Brief cmt")
                .Comment(CommentSection::CONSTRAINTS, "Constraints cmt 1")
                .Comment(CommentSection::CONSTRAINTS, "Constraints cmt 2");
            this->Comment(CommentSection::RESTRICTIONS, "Restrictions cmt")
                .Comment(CommentSection::THIRDPARTYFWKCOMPAT, "ThirdPartyFwkCopat cmt")
                .Comment(CommentSection::SEE, "See cmt")
                .Comment(CommentSection::SEE, "See cmt");
            this->Input("x")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .Comment("Input cmt 1")
                .FormatList({ge::FORMAT_ND});
            this->Input("y")
                .ParamType(REQUIRED)
                .Comment("Input cmt 2")
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND});
            this->Output("z")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND})
                .Comment("Output cmt 1");
            this->Output("o_z")
                .ParamType(OPTIONAL)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND})
                .Comment("Output cmt 1");
            this->Attr("VIN vin")
                .Comment("Attr cmt 1");
            this->Attr("VIN vin2")
                .Comment("Attr cmt 2");
            OpAICoreConfig aicConfig;
            this->AICore().AddConfig("ascend910b", aicConfig);
        }
    };
    OP_ADD(OpDefComment);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    EXPECT_TRUE(nullptr != so_path);
    setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    char buf[1024];
    char *cur_path = getcwd(buf, 1023);
    std::string gen_file = std::string(cur_path) + "/op_proto.h";
    std::ifstream gen_if;
    std::stringstream gen_ss;
    gen_if.open(gen_file);
    EXPECT_TRUE(gen_if.is_open());
    gen_ss << gen_if.rdbuf();
    EXPECT_NE(gen_ss.str().find("See cmt"), std::string::npos);
    gen_if.close();
    system(("rm -rf " + gen_file).c_str());
    unsetenv("ENABLE_SOURCE_PACKAGE");
}

TEST_F(TEST_OPBUILD, OpDefForBinQueryUt)
{
    class OpDefForBinQuery : public OpDef {
    public:
        OpDefForBinQuery(const char* name) : OpDef(name)
        {
            this->Input("x")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .DataTypeForBinQuery({ge::DT_FLOAT})
                .Format({ge::FORMAT_ND, ge::FORMAT_NC})
                .FormatForBinQuery({ge::FORMAT_ND, ge::FORMAT_ND});
            this->Input("y")
                .ParamType(REQUIRED)
                .Follow("x")
                .DataTypeForBinQuery({ge::DT_FLOAT});
            this->Output("z")
                .ParamType(REQUIRED)
                .Follow("y");
            OpAICoreConfig aicConfig;
            this->AICore().AddConfig("ascend910b", aicConfig);
        }
    };
    OP_ADD(OpDefForBinQuery);
    char* so_path = getenv("OPS_DSO_FILE_PATH");
    EXPECT_TRUE(nullptr != so_path);
    setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    char buf[1024];
    char *cur_path = getcwd(buf, 1023);
    std::string gen_file = std::string(cur_path) + "/aic-ascend910b-ops-info.ini";
    std::ifstream gen_if;
    std::stringstream gen_ss;
    gen_if.open(gen_file);
    EXPECT_TRUE(gen_if.is_open());
    gen_ss << gen_if.rdbuf();
    EXPECT_NE(gen_ss.str().find("input0.for_bin_dtype=float32,float32"), std::string::npos);
    EXPECT_NE(gen_ss.str().find("input0.for_bin_format=ND,ND"), std::string::npos);
    EXPECT_NE(gen_ss.str().find("input1.for_bin_dtype=float32,float32"), std::string::npos);
    gen_if.close();
    system(("rm -rf " + gen_file).c_str());
    unsetenv("ENABLE_SOURCE_PACKAGE");
}

TEST_F(TEST_OPBUILD, OpDefPathFailTest)
{
    class OpDefPathFail : public OpDef {
    public:
        OpDefPathFail(const char* name) : OpDef(name)
        {
            this->Input("x")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND});
            this->Input("y")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND});
            this->Output("z")
                .ParamType(REQUIRED)
                .DataTypeList({ge::DT_FLOAT})
                .FormatList({ge::FORMAT_ND});
            OpAICoreConfig aicConfig;
            this->AICore().AddConfig("ascend910b", aicConfig);
        }
    };
    OP_ADD(OpDefPathFail);
    std::vector<std::string> opsname = {"OpDefPathFail"};
    OpProtoGenerator generator(opsname);
    std::ofstream opProtoInitH;
    std::ofstream opProtoInitCc;
    MOCKER(realpath, char *(*)(const char *, char *)).times(1).will(returnValue((char*)nullptr));
    generator.GetFile(opProtoInitH, opProtoInitCc, "op_proto");
    EXPECT_TRUE(!opProtoInitH.is_open());
    EXPECT_TRUE(!opProtoInitCc.is_open());
}

TEST_F(TEST_OPBUILD, VirtInputUt)
{
    class VirtInputTest : public OpDef {
    public:
        VirtInputTest(const char* name) : OpDef(name)
        {
            this->Input("x1").ParamType(REQUIRED).DataType({ ge::DT_INT64 });
            this->Input("x2").ParamType(VIRTUAL).DataType({ ge::DT_INT64 });
            this->Output("y1").ParamType(REQUIRED).DataType({ ge::DT_INT64 }).InitValue(0);

            OpAICoreConfig aicConfig;
            this->AICore().AddConfig("ascend910", aicConfig);
        }
    };

    OP_ADD(VirtInputTest);

    char* so_path = getenv("OPS_DSO_FILE_PATH");
    EXPECT_TRUE(nullptr != so_path);
    setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    char buf[1024];
    char *cur_path = getcwd(buf, 1023);
    std::string gen_file = std::string(cur_path) + "/aic-ascend910-ops-info.ini";
    std::ifstream gen_if;
    std::stringstream gen_ss;
    std::cout << "check virtual in file " << gen_file << std::endl;
    gen_if.open(gen_file);
    EXPECT_TRUE(gen_if.is_open());
    gen_ss << gen_if.rdbuf();
    EXPECT_NE(gen_ss.str().find("input1.virtual=true"), std::string::npos);
    gen_if.close();
    system(("rm -rf " + gen_file).c_str());
    unsetenv("ENABLE_SOURCE_PACKAGE");
}

TEST_F(TEST_OPBUILD, VirtInputUtWithInitValue)
{
    class VirtInputScalarTest : public OpDef {
    public:
        VirtInputScalarTest(const char* name) : OpDef(name)
        {
            this->Input("x1").ParamType(REQUIRED).DataType({ ge::DT_INT64 });
            this->Input("x2").ParamType(VIRTUAL).DataType({ ge::DT_INT64 });
            this->Output("y1").ParamType(REQUIRED).DataType({ ge::DT_INT64 })
            .InitValue({ScalarType::FLOAT32, 3.2});

            OpAICoreConfig aicConfig;
            this->AICore().AddConfig("ascend910", aicConfig);
        }
    };

    OP_ADD(VirtInputScalarTest);

    char* so_path = getenv("OPS_DSO_FILE_PATH");
    EXPECT_TRUE(nullptr != so_path);
    setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
    int ret = opbuild_main(3, { "opbuild", so_path, "." });
    char buf[1024];
    char *cur_path = getcwd(buf, 1023);
    std::string gen_file = std::string(cur_path) + "/aic-ascend910-ops-info.ini";
    std::ifstream gen_if;
    std::stringstream gen_ss;
    std::cout << "check virtual in file " << gen_file << std::endl;
    gen_if.open(gen_file);
    EXPECT_TRUE(gen_if.is_open());
    gen_ss << gen_if.rdbuf();
    EXPECT_NE(gen_ss.str().find(
        "output0.initValue={ \"is_list\" : false, \"type\": \"float32\", \"value\": 3.200000}"),
        std::string::npos);
    gen_if.close();
    system(("rm -rf " + gen_file).c_str());
    unsetenv("ENABLE_SOURCE_PACKAGE");
}

extern void GenSingleInitValueTypeAndValue(std::ofstream& outfile, const ScalarVar& scalar);

TEST_F(TEST_OPBUILD, GenMc2InfoCase1)
{
    std::string fileName = "mc2_info_" + std::to_string(getpid()) + ".txt";
    std::ofstream fout(fileName);

    // 创建mc2Grps
    ge::AscendString tempStr0("111");
    ge::AscendString tempStr1("222");
    ge::AscendString tempStr2("333");
    std::vector<ge::AscendString> mc2Grps;
    mc2Grps.emplace_back(tempStr0);
    mc2Grps.emplace_back(tempStr1);
    mc2Grps.emplace_back(tempStr2);

    std::vector<std::string> opsvec({"Adds"});
    CfgGenerator cfgGen(opsvec);
    cfgGen.GenMC2Info(fout, mc2Grps);
    fout.close();

    // check 真值
    std::ifstream resultFile;
    std::stringstream streambuffer;
    resultFile.open(fileName, std::ios::in);
    EXPECT_TRUE(resultFile.is_open());
    streambuffer << resultFile.rdbuf();
    std::string resultString(streambuffer.str());
    std::string golden = "mc2.ctx=mc2_context_0,mc2_context_1,mc2_context_2";
    EXPECT_TRUE(resultString.find(golden) != std::string::npos);
    resultFile.close();
    EXPECT_EQ(remove(fileName.c_str()), 0);
}

void setInputHasErrorMessage(bool &hasErrorMessage, std::vector<std::string> errMessage)
{
    const std::string err = "The dtype size of input[0] of op ErrorInput is 0.";
    const std::string errMsg = "Misaligned format and dtype of x1 of op ErrorInput is not support.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
        if (errMessage[i] == errMsg) {
            hasErrorMessage = true;
            break;
        }
    }
    return;
}

void setOutputHasErrorMessage(bool &hasErrorMessage, std::vector<std::string> errMessage)
{
    const std::string err = "The dtype size of output[0] of op ErrorOutput is 0.";
    const std::string errMsg = "Misaligned format and dtype of x1 of op ErrorOutput is not support.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
        if (errMessage[i] == errMsg) {
            hasErrorMessage = true;
            break;
        }
    }
    return;
}


TEST_F(TEST_OPBUILD, CheckOpTypeName)
{
    std::string optype;
    bool ret = ops::IsVaildOpTypeName(optype);
    EXPECT_EQ(ret, false);
    optype = "add";
    ret = ops::IsVaildOpTypeName(optype);
    EXPECT_EQ(ret, false);
    optype = "Add_custom";
    ret = ops::IsVaildOpTypeName(optype);
    EXPECT_EQ(ret, false);
}

TEST_F(TEST_OPBUILD, ParamsRequired)
{
    auto ret = opbuild::Params::GetInstance().Required(2);
    EXPECT_EQ(ret, "");
    bool flag = opbuild::Params::GetInstance().Check("");
    EXPECT_EQ(flag, false);
}

TEST_F(TEST_OPBUILD, GenerateCodeForComputeUnits)
{
    Generator::SetGenPath("./");
    char* argv[] = {"opbuild", "--compute_unit=ascend910b"};
    opbuild::Params::GetInstance().Parse(2, argv);
    std::vector<std::string> opsvec({"ascend910b;ascend910"});
    CfgGenerator cfgGen(opsvec);
    opbuild::Status res = cfgGen.GenerateCode();
    EXPECT_EQ(res, opbuild::OPBUILD_SUCCESS);
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    char* argv1[] = {"opbuild", "--compute_unit=ascend910x"};
    opbuild::Params::GetInstance().Parse(2, argv1);
    res = cfgGen.GenerateCode();
    EXPECT_EQ(res, opbuild::OPBUILD_FAILED);
    opbuild::Params::GetInstance().optionParams_ = {};
    opbuild::Params::GetInstance().requiredParams_ = {};
    char* argv2[] = {"opbuild", "--compute_unit=ascend910_95"};
    opbuild::Params::GetInstance().Parse(2, argv2);
    res = cfgGen.GenerateCode();
    EXPECT_EQ(res, opbuild::OPBUILD_SUCCESS);
}

} // namespace ops
