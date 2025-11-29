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
#include "op_aclnn_fallback_generator.h"
#include "op_custom_registry_generator.h"
#include "register/op_def_registry.h"
#include "register/device_op_impl_registry.h"

extern int opbuild_main(int argc, std::vector<std::string> args);

namespace ops {
class TEST_OPBUILD : public testing::Test {
protected:
    void SetUp() {}
    void TearDown() {
        GlobalMockObject::verify();
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

// TEST_F(TEST_OPBUILD, NotGenerateAclnnInterface)
// {
//     setenv("OPS_ACLNN_GEN", "0", 1);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     EXPECT_EQ(ret, 0);
//     unsetenv("OPS_ACLNN_GEN");
// }
// TEST_F(TEST_OPBUILD, OpBuildFailLogTest)
// {
//     char buf[1024];
//     char* cur_path = getcwd(buf, 1023);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     char* src_path = getenv("OPS_SRC_FILE_PATH");
//     std::cout << "OPS DSO: " << so_path << std::endl;
//     std::cout << "OPS SRC: " << src_path << std::endl;
//     EXPECT_TRUE(nullptr != so_path);
//     EXPECT_TRUE(nullptr != src_path);
//     setenv("ASCEND_VENDOR_NAME", "customize", 1);
//     int ret = opbuild_main(3, { "opbuild", "aabb", cur_path });
//     EXPECT_EQ(ret, 1);
//     ret = opbuild_main(3, { "opbuild", so_path, "" });
//     EXPECT_EQ(ret, 1);
//     unsetenv("ASCEND_VENDOR_NAME");
//     ret = opbuild_main(3, { "opbuild", so_path, cur_path });
//     EXPECT_EQ(ret, 0);
//     setenv("ASCEND_VENDOR_NAME", "customize", 1);
//     std::vector<std::string> opvec = {"aa", "bb"};
//     Generator gen(opvec);
//     EXPECT_EQ(gen.GenerateCode(), opbuild::OPBUILD_SUCCESS);
//     OpCustomGenerator custGen(opvec);
//     MOCKER(realpath, char *(*)(const char *, char *)).times(2).will(returnValue((char*)nullptr));
//     EXPECT_EQ(custGen.GenerateCode(), opbuild::OPBUILD_FAILED);
//     ret = opbuild_main(3, { "opbuild", so_path, cur_path });
//     EXPECT_EQ(ret, 1);
// }
// TEST_F(TEST_OPBUILD, OpBuildProtoFailLog)
// {
//     char buf[1024];
//     char* cur_path = getcwd(buf, 1023);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     char* src_path = getenv("OPS_SRC_FILE_PATH");
//     std::cout << "OPS DSO: " << so_path << std::endl;
//     std::cout << "OPS SRC: " << src_path << std::endl;
//     EXPECT_TRUE(nullptr != so_path);
//     EXPECT_TRUE(nullptr != src_path);
//     setenv("ASCEND_VENDOR_NAME", "customize", 1);
//     std::vector<std::string> opvec = {"ProtoLogFail"};
//     OpProtoGenerator protoGen(opvec);
//     protoGen.fileGenPath = cur_path;
//     std::ofstream logH, logC;
//     EXPECT_EQ(protoGen.GetFile(logH, logC, std::string("invalid:filename*")), opbuild::OPBUILD_FAILED);
// }
// TEST_F(TEST_OPBUILD, OpBuildRun)
// {
//     char buf[1024];
//     char* cur_path = getcwd(buf, 1023);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     char* src_path = getenv("OPS_SRC_FILE_PATH");
//     std::cout << "OPS DSO: " << so_path << std::endl;
//     std::cout << "OPS SRC: " << src_path << std::endl;
//     EXPECT_TRUE(nullptr != so_path);
//     EXPECT_TRUE(nullptr != src_path);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     EXPECT_EQ(ret, 0);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     std::vector<std::string> files = { "/op_proto.cc", "/op_proto.h", "/aic-ascend310p-ops-info.ini",
//         "/aic-ascend910-ops-info.ini" };
//     std::string insConvert = "ABcd_Efg";
//     std::string covertStr;
//     auto convertString = ConvertToSnakeCase(insConvert);
//     std::string emptyStringIn = "";
//     std::string emptyStringOut = ConvertToSnakeCase(emptyStringIn);
//     EXPECT_EQ(emptyStringOut, "");
//     for (auto& file : files) {
//         src_file = std::string(src_path) + file + ".txt";
//         gen_file = std::string(cur_path) + file;
//         std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//         src_if.open(src_file);
//         EXPECT_TRUE(src_if.is_open());
//         src_ss << src_if.rdbuf();
//         gen_if.open(gen_file);
//         EXPECT_TRUE(gen_if.is_open());
//         gen_ss << gen_if.rdbuf();
//         EXPECT_EQ(src_ss.str(), gen_ss.str());
//         src_if.close();
//         gen_if.close();
//     }
// }

// TEST_F(TEST_OPBUILD, CustomOpRegistrySuccess)
// {
//     char buf[1024];
//     char* cur_path = getcwd(buf, 1023);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     char* src_path = getenv("OPS_SRC_FILE_PATH");
//     std::cout << "OPS DSO: " << so_path << std::endl;
//     std::cout << "OPS SRC: " << src_path << std::endl;
//     EXPECT_TRUE(nullptr != so_path);
//     EXPECT_TRUE(nullptr != src_path);
//     setenv("ASCEND_VENDOR_NAME", "customize", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     EXPECT_EQ(ret, 0);
//     setenv("ASCEND_VENDOR_NAME", "", 1);
//     ret = opbuild_main(3, { "opbuild", so_path, "." });
//     EXPECT_EQ(ret, 0);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     std::string file = "/custom_op_registry.cpp";
//     src_file = std::string(src_path) + file + ".txt";
//     gen_file = std::string(cur_path) + file;
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
// }

// TEST_F(TEST_OPBUILD, AclnnAddTik2RunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     std::vector<std::string> src_files = {"/aclnn_add_tik2.h.txt", "/aclnn_add_tik2.cpp.txt"};
//     std::vector<std::string> gen_files = {"/aclnn_add_tik2.h", "/aclnn_add_tik2.cpp"};
//     for (size_t i = 0U; i < src_files.size(); i++) {
//         src_file = std::string(src_path) + src_files[i];
//         gen_file = std::string(cur_path) + gen_files[i];
//         std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//         src_if.open(src_file);
//         EXPECT_TRUE(src_if.is_open());
//         src_ss << src_if.rdbuf();
//         gen_if.open(gen_file);
//         EXPECT_TRUE(gen_if.is_open());
//         gen_ss << gen_if.rdbuf();
//         EXPECT_EQ(src_ss.str(), gen_ss.str());
//         src_if.close();
//         gen_if.close();
//         system(("rm -rf " + gen_file).c_str());
//     }
// }

// TEST_F(TEST_OPBUILD, AclnnValueDependScopeRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     std::vector<std::string> src_files = {
//         "/aclnn_value_depend_scope_test.h.txt", "/aclnn_value_depend_scope_test.cpp.txt"};
//     std::vector<std::string> gen_files = {"/aclnn_value_depend_scope_test.h", "/aclnn_value_depend_scope_test.cpp"};
//     for (size_t i = 0U; i < src_files.size(); i++) {
//         src_file = std::string(src_path) + src_files[i];
//         gen_file = std::string(cur_path) + gen_files[i];
//         std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//         src_if.open(src_file);
//         EXPECT_TRUE(src_if.is_open());
//         src_ss << src_if.rdbuf();
//         gen_if.open(gen_file);
//         EXPECT_TRUE(gen_if.is_open());
//         gen_ss << gen_if.rdbuf();
//         EXPECT_EQ(src_ss.str(), gen_ss.str());
//         src_if.close();
//         gen_if.close();
//         system(("rm -rf " + gen_file).c_str());
//     }
// }


// TEST_F(TEST_OPBUILD, AclnnOnlyInputRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     src_file = std::string(src_path) + "/aclnn_input_test.cpp.txt";
//     gen_file = std::string(cur_path) + "/aclnn_input_test.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
// }

// TEST_F(TEST_OPBUILD, AclnnOnlyOutputRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     std::vector<std::string> src_files = {"/aclnn_output_test.cpp.txt"};
//     std::vector<std::string> gen_files = {"/aclnn_output_test.cpp"};
//     for (size_t i = 0U; i < src_files.size(); i++) {
//         src_file = std::string(src_path) + src_files[i];
//         gen_file = std::string(cur_path) + gen_files[i];
//         std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//         src_if.open(src_file);
//         EXPECT_TRUE(src_if.is_open());
//         src_ss << src_if.rdbuf();
//         gen_if.open(gen_file);
//         EXPECT_TRUE(gen_if.is_open());
//         gen_ss << gen_if.rdbuf();
//         EXPECT_EQ(src_ss.str(), gen_ss.str());
//         src_if.close();
//         gen_if.close();
//         system(("rm -rf " + gen_file).c_str());
//     }
// }

// TEST_F(TEST_OPBUILD, OpBuildRunProtoSeparate)
// {
//     char buf[1024];
//     char* cur_path = getcwd(buf, 1023);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     char* src_path = getenv("OPS_SRC_FILE_PATH");
//     std::cout << "OPS DSO: " << so_path << std::endl;
//     std::cout << "OPS SRC: " << src_path << std::endl;
//     EXPECT_TRUE(nullptr != so_path);
//     EXPECT_TRUE(nullptr != src_path);
//     setenv("OPS_PROTO_SEPARATE", "1", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     EXPECT_EQ(ret, 0);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     std::vector<std::string> files = {
//         "/add_tik2_proto.cpp",
//         "/add_tik2_proto.h",
//         "/auto_contiguous_test_proto.cpp",
//         "/auto_contiguous_test_proto.h",
//         "/dynamic_ref_test_proto.cpp",
//         "/dynamic_ref_test_proto.h",
//         "/input_test_proto.cpp",
//         "/input_test_proto.h",
//         "/output_test_proto.cpp",
//         "/output_test_proto.h",
//         "/ref_test_proto.cpp",
//         "/ref_test_proto.h",
//         "/scalar_test_proto.cpp",
//         "/scalar_test_proto.h",
//         "/value_depend_test_proto.cpp",
//         "/value_depend_test_proto.h",
//         "/version_test_proto.cpp",
//         "/version_test_proto.h",
//     };
//     for (auto& file : files) {
//         src_file = std::string(src_path) + "/op_proto_separate" + file + ".txt";
//         gen_file = std::string(cur_path) + file;
//         std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//         src_if.open(src_file);
//         EXPECT_TRUE(src_if.is_open());
//         src_ss << src_if.rdbuf();
//         gen_if.open(gen_file);
//         EXPECT_TRUE(gen_if.is_open());
//         gen_ss << gen_if.rdbuf();
//         EXPECT_EQ(src_ss.str(), gen_ss.str());
//         src_if.close();
//         gen_if.close();
//     }
//     unsetenv("OPS_PROTO_SEPARATE");
// }

// TEST_F(TEST_OPBUILD, AclnnfloatAttrRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *so_path = getenv("OPS_DSO_FILE_PATH");
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     EXPECT_TRUE(nullptr != src_path);
//     setenv("OPS_PROJECT_NAME", "test", 1);
//     setenv("OPS_DIRECT_ACCESS_PREFIX", "test", 1);
//     setenv("OPS_PRODUCT_NAME", "ascend910b;ascend310p", 1);
//     int ret = opbuild_main(3, {"opbuild", so_path, "."});
//     EXPECT_EQ(ret, 0);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     std::vector<std::string> src_files = {"/test_add_tik2.h.txt", "/test_add_tik2.cpp.txt"};
//     std::vector<std::string> gen_files = {"/test_add_tik2.h", "/test_add_tik2.cpp"};
//     for (size_t i = 0U; i < src_files.size(); i++) {
//         src_file = std::string(src_path) + src_files[i];
//         gen_file = std::string(cur_path) + gen_files[i];
//         std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//         src_if.open(src_file);
//         EXPECT_TRUE(src_if.is_open());
//         src_ss << src_if.rdbuf();
//         gen_if.open(gen_file);
//         EXPECT_TRUE(gen_if.is_open());
//         gen_ss << gen_if.rdbuf();
//         EXPECT_EQ(src_ss.str(), gen_ss.str());
//         src_if.close();
//         gen_if.close();
//         system(("rm -rf " + gen_file).c_str());
//     }
//     unsetenv("OPS_PROJECT_NAME");
//     unsetenv("OPS_DIRECT_ACCESS_PREFIX");
//     unsetenv("OPS_PRODUCT_NAME");
// }

// TEST_F(TEST_OPBUILD, AclnnFallBackRunSuccess)
// {
// class FallBackTest : public OpDef {
// public:
//     FallBackTest(const char* name) : OpDef(name)
//     {
//         this->Input("x1").DataType({ ge::DT_FLOAT16 }).ParamType(OPTIONAL);
//         this->Input("x2").DataType({ ge::DT_FLOAT16 }).ParamType(DYNAMIC);
//         this->Input("x3").DataType({ ge::DT_FLOAT }).ParamType(REQUIRED).ValueDepend(REQUIRED);
//         this->Input("x4").DataType({ ge::DT_BOOL }).ParamType(REQUIRED).ValueDepend(REQUIRED);
//         this->Input("x5").DataType({ ge::DT_INT64 }).ParamType(REQUIRED).ValueDepend(REQUIRED);
//         this->Input("x6").DataType({ ge::DT_FLOAT16 }).ParamType(OPTIONAL).Scalar();
//         this->Input("x7").DataType({ ge::DT_FLOAT16 }).ParamType(OPTIONAL).ScalarList();
//         this->Output("x1").DataType({ ge::DT_FLOAT16 });
//         this->Output("y1").DataType({ ge::DT_FLOAT16 }).ParamType(DYNAMIC);
//         this->Output("y2").DataType({ ge::DT_FLOAT16 }).OutputShapeDependOnCompute();
//         this->Attr("bias0").AttrType(OPTIONAL).Int(0);
//         this->Attr("bias1").AttrType(REQUIRED).Float(0.0);
//         this->Attr("bias2").AttrType(OPTIONAL).ListBool({ true, false });
//         this->Attr("bias3").AttrType(OPTIONAL).ListFloat({ 0.1, 0.2 });
//         this->Attr("bias4").AttrType(REQUIRED).ListInt({ 1, 2 });
//         this->Attr("bias5").AttrType(OPTIONAL).String("ssss");
//         this->Attr("bias6").AttrType(OPTIONAL).Bool(true);

//         this->AICore().AddConfig("ascend910");
//         this->EnableFallBack();
//     }
// };
// OP_ADD(FallBackTest);

//     char buf[1024];
//     char* cur_path = getcwd(buf, 1023);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     char* src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     EXPECT_TRUE(nullptr != src_path);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     EXPECT_EQ(ret, 0);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     src_file = std::string(src_path) + "/fallback_fall_back_test.cpp.txt";
//     gen_file = std::string(cur_path) + "/fallback_fall_back_test.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
// }

TEST_F(TEST_OPBUILD, TestErrorMessage)
{
    Generator::SetErrorMessage("get error");
    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    EXPECT_EQ(errMessage[0], "get error");
}

// TEST_F(TEST_OPBUILD, AclnnValueDependInputRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     src_file = std::string(src_path) + "/aclnn_value_depend_test.cpp.txt";
//     gen_file = std::string(cur_path) + "/aclnn_value_depend_test.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
// }

// TEST_F(TEST_OPBUILD, AclnnAutoContiguousRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     src_file = std::string(src_path) + "/aclnn_auto_contiguous_test.cpp.txt";
//     gen_file = std::string(cur_path) + "/aclnn_auto_contiguous_test.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
// }

// TEST_F(TEST_OPBUILD, AclnnRefContiguousRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     src_file = std::string(src_path) + "/aclnn_ref_contiguous_test.cpp.txt";
//     gen_file = std::string(cur_path) + "/aclnn_ref_contiguous_test.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
// }

// TEST_F(TEST_OPBUILD, TestValueDependError)
// {
//     OpDef opDef("TestError");
//     opDef.Input("x1").DataType({ge::DT_FLOAT16}).ValueDepend(REQUIRED);
//     OpAICoreConfig aicConfig;
//     opDef.AICore().AddConfig("ascend910b", aicConfig);
//     char buf[1024];
//     char* cur_path = getcwd(buf, 1023);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     char* src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     EXPECT_TRUE(nullptr != src_path);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     EXPECT_EQ(ret, 1);
// }

// TEST_F(TEST_OPBUILD, AclnnScalarRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     src_file = std::string(src_path) + "/aclnn_scalar_test.cpp.txt";
//     gen_file = std::string(cur_path) + "/aclnn_scalar_test.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
// }

// TEST_F(TEST_OPBUILD, AclnnRefRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     src_file = std::string(src_path) + "/aclnn_ref_test.cpp.txt";
//     gen_file = std::string(cur_path) + "/aclnn_ref_test.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
// }

// TEST_F(TEST_OPBUILD, AclnnDynamicRefRunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     src_file = std::string(src_path) + "/aclnn_dynamic_ref_test.cpp.txt";
//     gen_file = std::string(cur_path) + "/aclnn_dynamic_ref_test.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
// }

// TEST_F(TEST_OPBUILD, AclnnSupportVersionUt)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     std::vector<std::string> src_files = {"/aclnn_version_test.cpp.txt",
//         "/aclnn_version_test_v1.cpp.txt",
//         "/aclnn_version_test_v2.cpp.txt",
//         "/aclnn_version_test_v1.h.txt",
//         "/aclnn_version_test_v2.h.txt"};
//     std::vector<std::string> gen_files = {"/aclnn_version_test.cpp",
//         "/aclnn_version_test_v1.cpp",
//         "/aclnn_version_test_v2.cpp",
//         "/aclnn_version_test_v1.h",
//         "/aclnn_version_test_v2.h"};
//     for (size_t i = 0U; i < src_files.size(); i++) {
//         src_file = std::string(src_path) + src_files[i];
//         gen_file = std::string(cur_path) + gen_files[i];
//         std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//         src_if.open(src_file);
//         EXPECT_TRUE(src_if.is_open());
//         src_ss << src_if.rdbuf();
//         gen_if.open(gen_file);
//         EXPECT_TRUE(gen_if.is_open());
//         gen_ss << gen_if.rdbuf();
//         EXPECT_EQ(src_ss.str(), gen_ss.str());
//         src_if.close();
//         gen_if.close();
//         system(("rm -rf " + gen_file).c_str());
//     }
// }

// TEST_F(TEST_OPBUILD, AclnnMC2RunSuccess)
// {
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     char *src_path = getenv("OPS_SRC_FILE_PATH");
//     EXPECT_TRUE(nullptr != src_path);
//     std::string src_file, gen_file;
//     std::ifstream src_if, gen_if;
//     std::stringstream src_ss, gen_ss;
//     src_file = std::string(src_path) + "/aclnn_mc2_test.cpp.txt";
//     gen_file = std::string(cur_path) + "/aclnn_mc2_test.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
// }

// TEST_F(TEST_OPBUILD, OutputShapeDependOnCompute)
// {
//     char buf[1024];
//     char* cur_path = getcwd(buf, 1023);
//     char* src_path = getenv("OPS_SRC_FILE_PATH");
//     std::string src_file, gen_file, ops_file;
//     std::ifstream src_if, gen_if, ops_if;
//     std::stringstream src_ss, gen_ss, ops_ss;
//     ops_file = std::string(cur_path) + "/aic-ascend910-ops-info.ini";
//     std::cout << "check virtual in file " << ops_file << std::endl;
//     ops_if.open(ops_file);
//     EXPECT_TRUE(ops_if.is_open());
//     ops_ss << ops_if.rdbuf();
//     EXPECT_NE(ops_ss.str().find("output0.outputShapeDependOnCompute"), std::string::npos);
//     ops_if.close();
//     system(("rm -rf " + ops_file).c_str());

//     src_file = std::string(src_path) + "/aclnn_out_shape_dep_on_compute.cpp.txt";
//     gen_file = std::string(cur_path) + "/aclnn_out_shape_dep_on_compute.cpp";
//     std::cout << "compare " << src_file << " and " << gen_file << std::endl;
//     src_if.open(src_file);
//     EXPECT_TRUE(src_if.is_open());
//     src_ss << src_if.rdbuf();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_EQ(src_ss.str(), gen_ss.str());
//     src_if.close();
//     gen_if.close();

//     unsetenv("ENABLE_SOURCE_PACKAGE");
// }

// TEST_F(TEST_OPBUILD, OutPutFollowInputUt)
// {
//     class OutputFollowInput : public OpDef {
//     public:
//         OutputFollowInput(const char* name) : OpDef(name)
//         {
//             this->Input("x")
//                 .ParamType(REQUIRED)
//                 .DataTypeList({ge::DT_FLOAT})
//                 .FormatList({ge::FORMAT_ND})
//                 .ValueDepend(OPTIONAL, DependScope::TILING);
//             this->Input("y")
//                 .ParamType(REQUIRED)
//                 .DataTypeList({ge::DT_FLOAT})
//                 .FormatList({ge::FORMAT_ND})
//                 .ValueDepend(OPTIONAL, DependScope::TILING);
//             this->Output("z")
//                 .ParamType(REQUIRED)
//                 .Follow("y");
//             OpAICoreConfig aicConfig;
//             this->AICore().AddConfig("ascend910b", aicConfig);
//         }
//     };
//     OP_ADD(OutputFollowInput);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     std::string gen_file = std::string(cur_path) + "/op_proto.cc";
//     std::ifstream gen_if;
//     std::stringstream gen_ss;
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_NE(gen_ss.str().find("InferShapeOutputFollowInput(gert::InferShapeContext* context)"), std::string::npos);
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
//     unsetenv("ENABLE_SOURCE_PACKAGE");
// }

// TEST_F(TEST_OPBUILD, InputFollowInputUt)
// {
//     class InputFollowInput : public OpDef {
//     public:
//         InputFollowInput(const char* name) : OpDef(name)
//         {
//             this->Input("x")
//                 .ParamType(REQUIRED)
//                 .DataTypeList({ge::DT_FLOAT})
//                 .FormatList({ge::FORMAT_ND})
//                 .ValueDepend(OPTIONAL, DependScope::TILING);
//             this->Input("y")
//                 .ParamType(REQUIRED)
//                 .Follow("x");
//             this->Output("z")
//                 .ParamType(REQUIRED)
//                 .Follow("y");
//             OpAICoreConfig aicConfig;
//             this->AICore().AddConfig("ascend910b", aicConfig);
//         }
//     };
//     OP_ADD(InputFollowInput);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     std::string gen_file = std::string(cur_path) + "/op_proto.cc";
//     std::ifstream gen_if;
//     std::stringstream gen_ss;
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_NE(gen_ss.str().find("InferShapeInputFollowInput(gert::InferShapeContext* context)"), std::string::npos);
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
//     unsetenv("ENABLE_SOURCE_PACKAGE");
// }

// TEST_F(TEST_OPBUILD, ValueDependTilingSinkUt)
// {
//     class ValueDependTilingSink : public OpDef {
//     public:
//         ValueDependTilingSink(const char* name) : OpDef(name)
//         {
//             this->Input("x")
//                 .ParamType(REQUIRED)
//                 .DataTypeList({ge::DT_FLOAT})
//                 .FormatList({ge::FORMAT_ND})
//                 .ValueDepend(OPTIONAL, DependScope::TILING);
//             this->Input("y")
//                 .ParamType(REQUIRED)
//                 .DataTypeList({ge::DT_FLOAT})
//                 .FormatList({ge::FORMAT_ND});
//             this->Output("z")
//                 .ParamType(REQUIRED)
//                 .Follow("y");
//             OpAICoreConfig aicConfig;
//             this->AICore().AddConfig("ascend910b", aicConfig);
//         }
//     };
//     OP_ADD(ValueDependTilingSink);
//     optiling::DeviceOpImplRegister deviceOpImplRegister = optiling::DeviceOpImplRegister("ValueDependTilingSink");
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     std::string gen_file = std::string(cur_path) + "/op_proto.cc";
//     std::ifstream gen_if;
//     std::stringstream gen_ss;
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_NE(gen_ss.str().find("{gert::TilingPlacement::TILING_ON_HOST, gert::TilingPlacement::TILING_ON_AICPU}"), std::string::npos);
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
//     unsetenv("ENABLE_SOURCE_PACKAGE");
// }

// TEST_F(TEST_OPBUILD, OpDefCommentUt)
// {
//     class OpDefComment : public OpDef {
//     public:
//         OpDefComment(const char* name) : OpDef(name)
//         {
//             this->Comment(CommentSection::BRIEF, "Brief cmt")
//                 .Comment(CommentSection::CONSTRAINTS, "Constraints cmt 1")
//                 .Comment(CommentSection::CONSTRAINTS, "Constraints cmt 2");
//             this->Comment(CommentSection::RESTRICTIONS, "Restrictions cmt")
//                 .Comment(CommentSection::THIRDPARTYFWKCOMPAT, "ThirdPartyFwkCopat cmt")
//                 .Comment(CommentSection::SEE, "See cmt")
//                 .Comment(CommentSection::SEE, "See cmt");
//             this->Input("x")
//                 .ParamType(REQUIRED)
//                 .DataTypeList({ge::DT_FLOAT})
//                 .Comment("Input cmt 1")
//                 .FormatList({ge::FORMAT_ND});
//             this->Input("y")
//                 .ParamType(REQUIRED)
//                 .Comment("Input cmt 2")
//                 .DataTypeList({ge::DT_FLOAT})
//                 .FormatList({ge::FORMAT_ND});
//             this->Output("z")
//                 .ParamType(REQUIRED)
//                 .DataTypeList({ge::DT_FLOAT})
//                 .FormatList({ge::FORMAT_ND})
//                 .Comment("Output cmt 1");
//             this->Output("o_z")
//                 .ParamType(OPTIONAL)
//                 .DataTypeList({ge::DT_FLOAT})
//                 .FormatList({ge::FORMAT_ND})
//                 .Comment("Output cmt 1");
//             this->Attr("VIN vin")
//                 .Comment("Attr cmt 1");
//             this->Attr("VIN vin2")
//                 .Comment("Attr cmt 2");
//             OpAICoreConfig aicConfig;
//             this->AICore().AddConfig("ascend910b", aicConfig);
//         }
//     };
//     OP_ADD(OpDefComment);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     std::string gen_file = std::string(cur_path) + "/op_proto.h";
//     std::ifstream gen_if;
//     std::stringstream gen_ss;
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_NE(gen_ss.str().find("See cmt"), std::string::npos);
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
//     unsetenv("ENABLE_SOURCE_PACKAGE");
// }

// TEST_F(TEST_OPBUILD, OpDefForBinQueryUt)
// {
//     class OpDefForBinQuery : public OpDef {
//     public:
//         OpDefForBinQuery(const char* name) : OpDef(name)
//         {
//             this->Input("x")
//                 .ParamType(REQUIRED)
//                 .DataTypeList({ge::DT_FLOAT})
//                 .DataTypeForBinQuery({ge::DT_FLOAT})
//                 .Format({ge::FORMAT_ND, ge::FORMAT_NC})
//                 .FormatForBinQuery({ge::FORMAT_ND, ge::FORMAT_ND});
//             this->Input("y")
//                 .ParamType(REQUIRED)
//                 .Follow("x")
//                 .DataTypeForBinQuery({ge::DT_FLOAT});
//             this->Output("z")
//                 .ParamType(REQUIRED)
//                 .Follow("y");
//             OpAICoreConfig aicConfig;
//             this->AICore().AddConfig("ascend910b", aicConfig);
//         }
//     };
//     OP_ADD(OpDefForBinQuery);
//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     std::string gen_file = std::string(cur_path) + "/aic-ascend910b-ops-info.ini";
//     std::ifstream gen_if;
//     std::stringstream gen_ss;
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_NE(gen_ss.str().find("input0.for_bin_dtype=float32,float32"), std::string::npos);
//     EXPECT_NE(gen_ss.str().find("input0.for_bin_format=ND,ND"), std::string::npos);
//     EXPECT_NE(gen_ss.str().find("input1.for_bin_dtype=float32,float32"), std::string::npos);
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
//     unsetenv("ENABLE_SOURCE_PACKAGE");
// }

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

// TEST_F(TEST_OPBUILD, VirtInputUt)
// {
//     class VirtInputTest : public OpDef {
//     public:
//         VirtInputTest(const char* name) : OpDef(name)
//         {
//             this->Input("x1").ParamType(REQUIRED).DataType({ ge::DT_INT64 });
//             this->Input("x2").ParamType(VIRTUAL).DataType({ ge::DT_INT64 });
//             this->Output("y1").ParamType(REQUIRED).DataType({ ge::DT_INT64 }).InitValue(0);

//             OpAICoreConfig aicConfig;
//             this->AICore().AddConfig("ascend910", aicConfig);
//         }
//     };

//     OP_ADD(VirtInputTest);

//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     std::string gen_file = std::string(cur_path) + "/aic-ascend910-ops-info.ini";
//     std::ifstream gen_if;
//     std::stringstream gen_ss;
//     std::cout << "check virtual in file " << gen_file << std::endl;
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_NE(gen_ss.str().find("input1.virtual=true"), std::string::npos);
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
//     unsetenv("ENABLE_SOURCE_PACKAGE");
// }

// TEST_F(TEST_OPBUILD, VirtInputUtWithInitValue)
// {
//     class VirtInputScalarTest : public OpDef {
//     public:
//         VirtInputScalarTest(const char* name) : OpDef(name)
//         {
//             this->Input("x1").ParamType(REQUIRED).DataType({ ge::DT_INT64 });
//             this->Input("x2").ParamType(VIRTUAL).DataType({ ge::DT_INT64 });
//             this->Output("y1").ParamType(REQUIRED).DataType({ ge::DT_INT64 })
//             .InitValue({ScalarType::FLOAT32, 3.2});

//             OpAICoreConfig aicConfig;
//             this->AICore().AddConfig("ascend910", aicConfig);
//         }
//     };

//     OP_ADD(VirtInputScalarTest);

//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     std::string gen_file = std::string(cur_path) + "/aic-ascend910-ops-info.ini";
//     std::ifstream gen_if;
//     std::stringstream gen_ss;
//     std::cout << "check virtual in file " << gen_file << std::endl;
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_NE(gen_ss.str().find(
//         "output0.initValue={ \"is_list\" : false, \"type\": \"float32\", \"value\": 3.200000}"),
//         std::string::npos);
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
//     unsetenv("ENABLE_SOURCE_PACKAGE");
// }

// extern void GenSingleInitValueTypeAndValue(std::ofstream& outfile, const ScalarVar& scalar);
// extern bool inline CfgDtypeFormatCheck(const std::string& paramterName, const std::string& firstArgName,
//     const std::string& secondArgName, const size_t firstArgSize, const size_t secondArgSize);
// TEST_F(TEST_OPBUILD, VirtInputUtWithInitValueVec)
// {
//     class VirtInputScalarVecTest : public OpDef {
//     public:
//         VirtInputScalarVecTest(const char* name) : OpDef(name)
//         {
//             this->Input("x")
//                 .ParamType(REQUIRED)
//                 .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT32})
//                 .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND})
//                 .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});
//             this->Input("y")
//                 .ParamType(REQUIRED)
//                 .DataType({ge::DT_FLOAT16, ge::DT_FLOAT, ge::DT_INT32})
//                 .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND})
//                 .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND});
//             this->Output("z")
//                 .ParamType(REQUIRED)
//                 .DataType({ge::DT_INT32, ge::DT_FLOAT, ge::DT_INT32})
//                 .Format({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND})
//                 .UnknownShapeFormat({ge::FORMAT_ND, ge::FORMAT_ND, ge::FORMAT_ND})
//                 .InitValue({{ScalarType::INT16, 1}, {ScalarType::FLOAT32, 3.2}, {ScalarType::INT16, 1}});

//             OpAICoreConfig aicConfig;
//             aicConfig.ExtendCfgInfo("prebuildPattern.value", "Opaque");
//             aicConfig.ExtendCfgInfo("coreType.value", "AiCore");
//             aicConfig.ExtendCfgInfo("jitCompile.flag", "static_false,dynamic_false");
//             this->AICore().AddConfig("ascend910", aicConfig);
//         }
//     };

//     OP_ADD(VirtInputScalarVecTest);

//     char* so_path = getenv("OPS_DSO_FILE_PATH");
//     EXPECT_TRUE(nullptr != so_path);
//     setenv("ENABLE_SOURCE_PACKAGE", "False", 1);
//     int ret = opbuild_main(3, { "opbuild", so_path, "." });
//     char buf[1024];
//     char *cur_path = getcwd(buf, 1023);
//     std::string gen_file = std::string(cur_path) + "/aic-ascend910-ops-info.ini";
//     std::ifstream gen_if;
//     std::stringstream gen_ss;
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_NE(gen_ss.str().find(
//         "{ \"is_list\" : true, \"float32\": { \"type\": \"float32\", \"value\": 3.200000 }, \"int32\": { \"type\": \"int16\", \"value\": 1 }}"),
//         std::string::npos);
//     gen_if.close();
//     std::ofstream outfile;
//     outfile.open(gen_file, std::ofstream::out | std::ofstream::trunc);
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::UINT64, 1});
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::INT64, 1});
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::UINT32, 1});
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::INT32, 1});
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::UINT16, 1});
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::INT16, 1});
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::UINT8, 1});
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::INT8, 1});
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::FLOAT32, 1});
//     GenSingleInitValueTypeAndValue(outfile, {ScalarType::FLOAT16, 1});
//     CfgDtypeFormatCheck("a", "b", "c", 1, 2);
//     (void)opbuild_main(3, { "opbuild", so_path, "." });
//     outfile.close();
//     gen_if.open(gen_file);
//     EXPECT_TRUE(gen_if.is_open());
//     gen_ss << gen_if.rdbuf();
//     EXPECT_NE(gen_ss.str().find(
//         "\"type\": \"uint64\", \"value\": 1\"type\": \"int64\", \"value\": 1\"type\": \"uint32\""),
//         std::string::npos);
//     gen_if.close();
//     system(("rm -rf " + gen_file).c_str());
//     unsetenv("ENABLE_SOURCE_PACKAGE");
// }

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

TEST_F(TEST_OPBUILD, GenMc2Failed01)
{
    std::string fileName = "mc2_failed_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("MC2TestFail");
    opDef.Attr("group_test").AttrType(REQUIRED).Bool();
    opDef.MC2().HcclGroup({"group_test"});
    std::vector<std::string> name = {"group_test"};
    std::vector<int32_t> attrTypes = {1};

    std::vector<std::string> opsvec({"MC2TestFail"});
    AclnnOpGenerator opGen(opsvec);
    opGen.AclnnOpGenCodeHcclGroup(opDef, name, attrTypes, outfile);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err =
        "HcclGroup params of op MC2TestFail must be String attr name, but group_test is not String attr.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, GenMc2Failed02)
{
    std::string fileName = "mc2_failed_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("MC2TestNameFail");
    opDef.Attr("group_test").AttrType(OPTIONAL).String();
    opDef.MC2().HcclGroup({"groupTestOptional"});
    std::vector<std::string> name = {"groupTestOptional"};
    std::vector<int32_t> attrTypes = {0};

    std::vector<std::string> opsvec({"MC2TestNameFail"});
    AclnnOpGenerator opGen(opsvec);
    opGen.AclnnOpGenCodeHcclGroup(opDef, name, attrTypes, outfile);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err =
        "HcclGroup params of op MC2TestNameFail must be String attr name, but groupTestOptional is not attr name.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        std::cout << errMessage[i] << std::endl;
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, GenMc2Failed03)
{

    std::string fileName = "mc2_failed_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("MC2TestHcclServerTypeFail");
    OpAICoreConfig aicConfig;
    opDef.Input("x").ParamType(REQUIRED).DataType({ ge::DT_FLOAT }).Format({ ge::FORMAT_ND });
    opDef.Output("y").ParamType(REQUIRED).DataType({ ge::DT_FLOAT }).Format({ ge::FORMAT_ND });
    opDef.Attr("group1").AttrType(REQUIRED).String();
    opDef.Attr("group2").AttrType(OPTIONAL).String();
    opDef.AICore().AddConfig("ascend910b", aicConfig);
    opDef.MC2().HcclGroup({"group2", "group1"});
    opDef.MC2().HcclServerType((HcclServerType)5, "ascend910b");
    std::vector<std::string> name = {"groupTestOptional"};
    std::vector<int32_t> attrTypes = {0};

    std::vector<std::string> opsvec({"MC2TestHcclServerTypeFail"});
    AclnnOpGenerator opGen(opsvec);
    opGen.AclnnOpGenHcclServerTypeList(opDef, outfile);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err =
        "HcclServerType params of op MC2TestHcclServerTypeFail must be an enumeration value of enum class HcclServerType.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        std::cout << errMessage[i] << std::endl;
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnGenFailedWithoutSoc)
{
    std::string fileName = "test_failed_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("TestFail");

    std::vector<std::string> opsvec({"TestFail"});
    AclnnOpGenerator opGen(opsvec);
    opGen.AclnnOpGenOpSupportListAll(opDef, outfile);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err =
        "The soc version of op TestFail is not configured,"
        " at least one soc version must be configured."
        " Check whether the soc version is added to the op proto type"
        " definition in the host implementation(Opdef, through AddConfig).";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnGenVersionFailed01)
{
    OpDef opDef("VersionFail");
    opDef.Input("x1").DataType({ge::DT_FLOAT16}).Version(1);
    opDef.Input("x2").DataType({ge::DT_FLOAT16});

    std::vector<std::string> opsvec({"VersionFail"});
    AclnnOpGenerator opGen(opsvec);
    std::set<uint32_t> versions;
    uint32_t maxVersion = 0U;
    opGen.GetConfigVerion(opDef, versions, maxVersion);

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err = "VersionFail only optional input can configured version.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnGenVersionFailed02)
{
    OpDef opDef("VersionFail");
    opDef.Input("x1").DataType({ge::DT_FLOAT16});
    opDef.Output("x2").DataType({ge::DT_FLOAT16});
    opDef.Attr("attr").AttrType(REQUIRED).Bool().Version(1);

    std::vector<std::string> opsvec({"VersionFail"});
    AclnnOpGenerator opGen(opsvec);
    std::set<uint32_t> versions;
    uint32_t maxVersion = 0U;
    opGen.GetConfigVerion(opDef, versions, maxVersion);

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err = "VersionFail only optional attr can configured version.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnGenVersionFailed03)
{
    OpDef opDef("VersionFail");
    opDef.Input("x1").DataType({ge::DT_FLOAT16});
    opDef.Output("x2").DataType({ge::DT_FLOAT16}).Version(1);

    std::vector<std::string> opsvec({"VersionFail"});
    AclnnOpGenerator opGen(opsvec);
    std::set<uint32_t> versions;
    uint32_t maxVersion = 0U;
    opGen.GetConfigVerion(opDef, versions, maxVersion);

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err = "The output version of op VersionFail does not support.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnGenValudeDependFailed01)
{
    std::string fileName = "depend_failed_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("ValudeDependFail");
    opDef.Input("x1").DataType({ge::DT_FLOAT16}).ValueDepend(OPTIONAL);
    opDef.Output("x2").DataType({ge::DT_FLOAT16});

    std::vector<std::string> opsvec({"ValudeDependFail"});
    AclnnOpGenerator opGen(opsvec);
    OpDefName opdefName;
    std::vector<std::string> paramNames = {"x1"};
    opdefName.inputsName = paramNames;
    opGen.AclnnOpGenFunProtoInputParams(opDef, opdefName, outfile, 0, false);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err = "ValueDepend input dtype of op ValudeDependFail must be float, int64 or bool.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnGenValudeDependFailed02)
{
    std::string fileName = "depend_failed_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("ValudeDependFail");
    opDef.Input("x1").DataType({ge::DT_FLOAT, ge::DT_FLOAT16}).ValueDepend(OPTIONAL);
    opDef.Output("x2").DataType({ge::DT_FLOAT, ge::DT_FLOAT16});

    std::vector<std::string> opsvec({"ValudeDependFail"});
    AclnnOpGenerator opGen(opsvec);
    OpDefName opdefName;
    std::vector<std::string> paramNames = {"x1"};
    opdefName.inputsName = paramNames;
    opGen.AclnnOpGenFunProtoInputParams(opDef, opdefName, outfile, 0, false);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err =
        "ValueDepend input dtype of op ValudeDependFail must be the same and must be float, int64, or bool.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnGenValudeDependFailed03)
{
    std::string fileName = "depend_failed_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("ValudeDependFail");
    opDef.Input("x1").DataType({ge::DT_FLOAT16}).ValueDepend(OPTIONAL).Scalar();
    opDef.Output("x2").DataType({ge::DT_FLOAT16});

    std::vector<std::string> opsvec({"ValudeDependFail"});
    AclnnOpGenerator opGen(opsvec);
    OpDefName opdefName;
    std::vector<std::string> paramNames = {"x1"};
    opdefName.inputsName = paramNames;
    opGen.AclnnOpGenFunProtoInputParams(opDef, opdefName, outfile, 0, false);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err =
        "Valuedepend and Scalar/ScalarList of op ValudeDependFail cannot be configured at the same time.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnGenScalarFailed02)
{
    std::string fileName = "scalar_failed_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("ScalarFail");
    opDef.Input("x1").DataType({ge::DT_FLOAT16}).Scalar().To(ge::DT_MAX);
    opDef.Input("x2").DataType({ge::DT_FLOAT16});

    std::vector<std::string> opsvec({"ScalarFail"});
    AclnnOpGenerator opGen(opsvec);
    OpDefName paramNames;
    paramNames.inputsName = {"x1", "x2"};
    paramNames.originInputName = {"x1", "x2"};
    opGen.AclnnOpGenCodeAddInputTensors(opDef, paramNames, outfile, false);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err = "Dtype of input x1 of op ScalarFail is not support.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnGenAttrFailed01)
{
    std::string fileName = "attr_failed_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("AttrFail");
    opDef.Input("x1").DataType({ge::DT_FLOAT16});
    opDef.Output("x2").DataType({ge::DT_FLOAT16});
    opDef.Attr("bias").ListListInt();

    std::vector<std::string> opsvec({"AttrFail"});
    AclnnOpGenerator opGen(opsvec);
    std::vector<std::string> paramNames = {"bias"};
    opGen.AclnnOpGenFunProtoAttrParams(opDef, paramNames, outfile, 0);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err = "Data type of attr bias of op AttrFail is not support.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
}

TEST_F(TEST_OPBUILD, AclnnInvalidsocVersion)
{
    std::string fileName = "invalid_soc_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("ErrorSoc");
    opDef.Input("x1").DataType({ge::DT_FLOAT16});
    opDef.Output("x2").DataType({ge::DT_FLOAT16});
    opDef.AICore().AddConfig("ascend510");

    std::vector<std::string> opsvec({"ErrorSoc"});
    AclnnOpGenerator opGen(opsvec);
    opGen.AclnnOpGenSocSupportList(opDef, outfile);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    const std::string err = "Invalid socVersion ascend510 of op ErrorSoc, "
                            "please check whether AddConfig are correctly configured in Opdef.";
    for (size_t i = 0U; i < errMessage.size(); i++) {
        if (errMessage[i] == err) {
            hasErrorMessage = true;
            break;
        }
    }
    EXPECT_TRUE(hasErrorMessage);
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

TEST_F(TEST_OPBUILD, AclnnErrorInputDtypeSize)
{
    std::string fileName = "error_input_dtype_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("ErrorInput");
    opDef.Input("x1").DataType({ge::DT_FLOAT16, ge::DT_FLOAT}).Format({ ge::FORMAT_ND});
    opDef.AICore().AddConfig("ascend910");

    std::vector<std::string> opsvec({"ErrorInput"});
    AclnnOpGenerator opGen(opsvec);
    opGen.AclnnOpGenOpSupportListAll(opDef, outfile);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    setInputHasErrorMessage(hasErrorMessage, errMessage);
    EXPECT_EQ(hasErrorMessage, true);
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

TEST_F(TEST_OPBUILD, AclnnErrorOutputDtypeSize)
{
    std::string fileName = "error_output_dtype_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("ErrorOutput");
    opDef.Output("x1").DataType({ge::DT_FLOAT16, ge::DT_FLOAT}).Format({ ge::FORMAT_ND});
    opDef.AICore().AddConfig("ascend910");

    std::vector<std::string> opsvec({"ErrorOutput"});
    AclnnOpGenerator opGen(opsvec);
    opGen.AclnnOpGenOpSupportListAll(opDef, outfile);
    outfile.close();

    std::vector<std::string> errMessage = Generator::GetErrorMessage();
    bool hasErrorMessage = false;
    setOutputHasErrorMessage(hasErrorMessage, errMessage);
    EXPECT_EQ(hasErrorMessage, true);
}

TEST_F(TEST_OPBUILD, AclnnErrorAttrDtype)
{
    std::string fileName = "error_attr_dtype_" + std::to_string(getpid()) + ".txt";
    std::ofstream outfile = std::ofstream(fileName);
    OpDef opDef("ErrorAttr");
    opDef.Output("x1").DataType({ge::DT_FLOAT16}).Format({ ge::FORMAT_ND});
    opDef.Attr("bias").ListListInt();
    opDef.AICore().AddConfig("ascend910");

    std::vector<std::string> opsvec({"ErrorAttr"});
    AclnnFallBackGenerator opGen(opsvec);
    opGen.AclnnGenFallBackAttrFuncImpl(opDef.GetAttrs()[0], 7, outfile);
    outfile.close();
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

} // namespace ops