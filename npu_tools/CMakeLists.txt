# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.16.0)
  project(npu_tools LANGUAGES CXX)
endif()

option(NPU_COMPUTE_BUILD_TESTS "Build NPU Compute tests" OFF)

set(NPU_TOOLS_LIBRARY_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib64")
set(NPU_TOOLS_RUNTIME_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")

# libboost_filesystem.a of third party use legacy libstdc++ ABI => _GLIBCXX_USE_CXX11_ABI=0
get_target_property(boost_filesystem_target Boost::filesystem ALIASED_TARGET)
if(NOT boost_filesystem_target)
  set(boost_filesystem_target Boost::filesystem)
endif()
# any target that links boost filesystem must set ABI to 0
target_compile_definitions(${boost_filesystem_target} INTERFACE
  _GLIBCXX_USE_CXX11_ABI=0
)

if(NPU_COMPUTE_BUILD_TESTS OR ENABLE_TEST)
  enable_testing()
  set(INJECTION_BUILD_TESTS ON CACHE BOOL "Build Runtime injection tests" FORCE)
endif()

add_subdirectory(injection)
if(ASC_TOOLS_BUILD_NPU_COMPUTE)
  add_subdirectory(npu_compute)
endif()
add_subdirectory(npu_sanitizer)

if(ASC_TOOLS_BUILD_NPU_COMPUTE AND (NPU_COMPUTE_BUILD_TESTS OR ENABLE_TEST))
  add_test(NAME npu_tools.product_build_output_layout
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_product_build_output_layout.sh"
      "${CMAKE_CURRENT_BINARY_DIR}")
endif()
