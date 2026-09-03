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
  project(npu_compute LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NPU_COMPUTE_INSTALL_ARCH_DIR "${CMAKE_SYSTEM_PROCESSOR}-linux")
set(NPU_COMPUTE_INSTALL_BINDIR "${NPU_COMPUTE_INSTALL_ARCH_DIR}/bin")
set(NPU_COMPUTE_INSTALL_INTERNAL_DIR "${NPU_COMPUTE_INSTALL_ARCH_DIR}/tools/npu_tools")
set(NPU_COMPUTE_INSTALL_INTERNAL_BINDIR "${NPU_COMPUTE_INSTALL_INTERNAL_DIR}/bin")
set(NPU_COMPUTE_INSTALL_LIBDIR "${NPU_COMPUTE_INSTALL_INTERNAL_DIR}/lib64")

find_package(Threads REQUIRED)
set(CMAKE_SKIP_RPATH FALSE)

option(NPU_COMPUTE_BUILD_TESTS "Build NPU Compute tests" OFF)
option(NPU_COMPUTE_BUILD_REAL_HARDWARE_TESTS
  "Build real HardwareInfo integration test" OFF)

set(NPU_COMPUTE_USE_STUB_BACKEND ${NPU_COMPUTE_BUILD_TESTS})

set(NPUCOMPUTE_CANN_ROOT "" CACHE PATH
  "CANN architecture package root used by NPU Compute"
)
if(NPU_COMPUTE_USE_STUB_BACKEND)
  set(NPUCOMPUTE_ACL_INCLUDE_DIR "${INJECTION_ACL_INCLUDE_DIR}")
  set(NPUCOMPUTE_PROFILING_INCLUDE_DIR "${INJECTION_PROFILING_INCLUDE_DIR}")
else()
  find_path(NPUCOMPUTE_ACL_INCLUDE_DIR NAMES acl/acl.h HINTS
    "${NPUCOMPUTE_CANN_ROOT}/include"
    "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/include"
    "$ENV{ASCEND_HOME_PATH}/include")
  find_path(NPUCOMPUTE_PROFILING_INCLUDE_DIR NAMES profiling/prof_common.h HINTS
    "${NPUCOMPUTE_CANN_ROOT}/pkg_inc"
    "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/pkg_inc"
    "$ENV{ASCEND_HOME_PATH}/pkg_inc")
  if(NOT NPUCOMPUTE_ACL_INCLUDE_DIR OR NOT NPUCOMPUTE_PROFILING_INCLUDE_DIR)
    message(FATAL_ERROR "NPU Compute CANN headers were not found. Set NPUCOMPUTE_CANN_ROOT.")
  endif()
endif()

if(NPU_COMPUTE_BUILD_REAL_HARDWARE_TESTS AND NOT NPU_COMPUTE_BUILD_TESTS)
  message(FATAL_ERROR
    "NPU_COMPUTE_BUILD_REAL_HARDWARE_TESTS requires NPU_COMPUTE_BUILD_TESTS")
endif()

if(DEFINED NPU_TOOLS_LIBRARY_OUTPUT_DIR AND DEFINED NPU_TOOLS_RUNTIME_OUTPUT_DIR)
  set(NPU_COMPUTE_LIBRARY_OUTPUT_DIR "${NPU_TOOLS_LIBRARY_OUTPUT_DIR}")
  set(NPU_COMPUTE_RUNTIME_OUTPUT_DIR "${NPU_TOOLS_RUNTIME_OUTPUT_DIR}")
else()
  set(NPU_COMPUTE_LIBRARY_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib64")
  set(NPU_COMPUTE_RUNTIME_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()

function(npu_compute_set_test_output target)
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${NPU_COMPUTE_RUNTIME_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${NPU_COMPUTE_RUNTIME_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${NPU_COMPUTE_RUNTIME_OUTPUT_DIR}"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
  )
endfunction()

function(npu_compute_set_product_output target)
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${NPU_COMPUTE_LIBRARY_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${NPU_COMPUTE_LIBRARY_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${NPU_COMPUTE_RUNTIME_OUTPUT_DIR}"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
  )
endfunction()

add_library(npu_compute_headers INTERFACE)
target_include_directories(npu_compute_headers INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${NPUCOMPUTE_ACL_INCLUDE_DIR}>"
  "$<BUILD_INTERFACE:${NPUCOMPUTE_PROFILING_INCLUDE_DIR}>"
  "$<BUILD_INTERFACE:${NPUCOMPUTE_PROFILING_INCLUDE_DIR}/profiling>"
  "$<INSTALL_INTERFACE:include>"
)

add_library(npu_compute_backend_headers INTERFACE)
target_link_libraries(npu_compute_backend_headers INTERFACE npu_compute_headers injection_headers)

add_library(npu_compute_integration_headers INTERFACE)
target_link_libraries(npu_compute_integration_headers INTERFACE npu_compute_backend_headers)

add_library(acl_runtime_backend INTERFACE)
add_library(acl_prof_api_backend INTERFACE)
target_link_libraries(acl_runtime_backend INTERFACE injection_runtime_backend)
target_link_libraries(acl_prof_api_backend INTERFACE injection_prof_api_backend)

add_subdirectory(src/acl_pti)

add_subdirectory(src/npu_compute)
add_subdirectory(src/compute_launcher)

install(TARGETS acl_pti npu_compute
  LIBRARY DESTINATION "${NPU_COMPUTE_INSTALL_LIBDIR}"
  COMPONENT asc-tools
)

install(TARGETS npu_compute_cli
  RUNTIME DESTINATION "${NPU_COMPUTE_INSTALL_INTERNAL_BINDIR}"
  COMPONENT asc-tools
)

install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/src/compute_launcher/npu_compute.sh"
  DESTINATION "${NPU_COMPUTE_INSTALL_BINDIR}"
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  RENAME npu-compute
  COMPONENT asc-tools
)

if(NPU_COMPUTE_BUILD_TESTS)
  add_subdirectory(stubs/acl_pti_callback)
  add_subdirectory(stubs/hardware_api)
  enable_testing()
  add_subdirectory(tests)
endif()
