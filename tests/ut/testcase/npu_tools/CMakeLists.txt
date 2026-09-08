# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set(NPU_TOOLS_TEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(NPU_TOOLS_PY_TEST_ROOT "${CMAKE_SOURCE_DIR}/tests/py_ut/testcase/npu_tools")
set(NPU_TOOLS_TEST_RUNTIME_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")
# Host DBI tests emulate the CANN runtime and toolchain layout under this build-local root.
set(NPU_TOOLS_TEST_CANN_ROOT "${CMAKE_CURRENT_BINARY_DIR}/fake-cann")
set(NPU_TOOLS_TEST_CANN_RUNTIME_DIR "${NPU_TOOLS_TEST_CANN_ROOT}/${CMAKE_SYSTEM_PROCESSOR}-linux/lib64")
set(INJECTION_SOURCE_DIR "${CMAKE_SOURCE_DIR}/npu_tools/injection")
set(NPU_COMPUTE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/npu_tools/npu_compute")
set(NPU_COMPUTE_PY_TEST_DIR "${NPU_TOOLS_PY_TEST_ROOT}/npu_compute/tests")

option(NPU_COMPUTE_BUILD_REAL_HARDWARE_TESTS
  "Build real HardwareInfo integration test" OFF)

find_package(Threads REQUIRED)

function(injection_set_test_output target)
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${NPU_TOOLS_TEST_RUNTIME_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${NPU_TOOLS_TEST_RUNTIME_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${NPU_TOOLS_TEST_RUNTIME_OUTPUT_DIR}"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
  )
endfunction()

function(npu_compute_set_test_output target)
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${NPU_TOOLS_TEST_RUNTIME_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${NPU_TOOLS_TEST_RUNTIME_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${NPU_TOOLS_TEST_RUNTIME_OUTPUT_DIR}"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
  )
endfunction()

add_library(injection_test_headers INTERFACE)
target_include_directories(injection_test_headers INTERFACE
  "$<BUILD_INTERFACE:${INJECTION_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${NPU_TOOLS_TEST_ROOT}/injection/tests/stubs/runtime/include>"
  "$<BUILD_INTERFACE:${NPU_TOOLS_TEST_ROOT}/injection/tests/stubs/prof_api/include>"
  "$<BUILD_INTERFACE:${INJECTION_ACL_INCLUDE_DIR}>"
)

add_library(npu_compute_test_headers INTERFACE)
target_include_directories(npu_compute_test_headers INTERFACE
  "$<BUILD_INTERFACE:${NPU_COMPUTE_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${NPU_TOOLS_TEST_ROOT}/injection/tests/stubs/runtime/include>"
  "$<BUILD_INTERFACE:${NPU_TOOLS_TEST_ROOT}/injection/tests/stubs/prof_api/include>"
  "$<BUILD_INTERFACE:${NPUCOMPUTE_ACL_INCLUDE_DIR}>"
)
target_link_libraries(npu_compute_test_headers INTERFACE injection_test_headers)

add_subdirectory(injection/tests/stubs/prof_api)
add_subdirectory(injection/tests/stubs/runtime)
add_subdirectory(injection/tests)

add_subdirectory(npu_compute/stubs/acl_pti_callback)
add_subdirectory(npu_compute/stubs/hardware_api)
add_subdirectory(npu_compute/tests)

add_subdirectory(npu_sanitizer/common/tests)
add_subdirectory(npu_sanitizer/sanitizer_api/tests)
add_subdirectory(npu_sanitizer/npu_check_cli/tests)
add_subdirectory(npu_sanitizer/npu_check/tests)
