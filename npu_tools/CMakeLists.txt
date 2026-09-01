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

option(ASC_TOOLS_BUILD_NPU_COMPUTE "Build NPU Compute profiling support" OFF)
option(BUILD_NPU_SANITIZER "Build npu sanitizer prototype libraries" OFF)
option(NPU_COMPUTE_BUILD_TESTS "Build NPU Compute tests" OFF)

set(NPU_TOOLS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")
set(INJECTION_OUTPUT_DIR "${NPU_TOOLS_OUTPUT_DIR}")

if(NPU_COMPUTE_BUILD_TESTS OR ENABLE_TEST)
  enable_testing()
  set(INJECTION_BUILD_TESTS ON CACHE BOOL "Build Runtime injection tests" FORCE)
endif()

if(ASC_TOOLS_BUILD_NPU_COMPUTE OR BUILD_NPU_SANITIZER)
  add_subdirectory(injection)
endif()

if(ASC_TOOLS_BUILD_NPU_COMPUTE)
  add_subdirectory(npu_compute)
endif()

if(BUILD_NPU_SANITIZER)
  add_subdirectory(npu_sanitizer/common)
  add_subdirectory(npu_sanitizer/sanitizer_api)
  add_subdirectory(npu_sanitizer/npu_check_cli)
  add_subdirectory(npu_sanitizer/npu_check)
endif()
