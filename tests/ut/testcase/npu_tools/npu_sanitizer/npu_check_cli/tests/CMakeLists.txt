# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set(NPU_CHECK_CLI_SOURCE_DIR "${CMAKE_SOURCE_DIR}/npu_tools/npu_sanitizer/npu_check_cli")

if(NOT TARGET GTest::gtest)
  find_package(GTest REQUIRED)
endif()

add_executable(npu_check_cli_unit_tests
  gtest_main.cpp
  options_test.cpp
  process_runner_test.cpp
  "${NPU_CHECK_CLI_SOURCE_DIR}/src/process_runner.cpp"
  "${NPU_CHECK_CLI_SOURCE_DIR}/src/uds_client.cpp"
  "${NPU_CHECK_CLI_SOURCE_DIR}/src/options.cpp"
)
target_compile_features(npu_check_cli_unit_tests PRIVATE cxx_std_17)
target_compile_definitions(npu_check_cli_unit_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(npu_check_cli_unit_tests PRIVATE "${NPU_CHECK_CLI_SOURCE_DIR}/src")
target_link_libraries(npu_check_cli_unit_tests PRIVATE plog_test_library npu_check_common npu_sanitizer_plog GTest::gtest Boost::filesystem)
add_test(NAME npu_check_cli_unit_tests COMMAND npu_check_cli_unit_tests)
