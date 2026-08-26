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
set(NPU_COMPUTE_INSTALL_LIBDIR "${NPU_COMPUTE_INSTALL_ARCH_DIR}/lib64")
set(NPU_COMPUTE_INSTALL_INCLUDEDIR "${NPU_COMPUTE_INSTALL_ARCH_DIR}/include")

find_package(Threads REQUIRED)
set(CMAKE_SKIP_RPATH FALSE)

option(NPU_COMPUTE_BUILD_TESTS "Build NPU Compute tests" OFF)
option(NPU_COMPUTE_BUILD_REAL_HARDWARE_TESTS
  "Build real HardwareInfo integration test" OFF)

# Stubs are test-only. All non-test builds link against the installed CANN
# Runtime and profiling libraries.
set(NPU_COMPUTE_USE_STUB_BACKEND ${NPU_COMPUTE_BUILD_TESTS})

set(NPUCOMPUTE_CANN_ROOT "" CACHE PATH
  "CANN architecture package root used by NPU Compute"
)
find_path(NPUCOMPUTE_ACL_INCLUDE_DIR
  NAMES acl/acl.h
  HINTS
    "${NPUCOMPUTE_CANN_ROOT}/include"
    "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/include"
    "$ENV{ASCEND_HOME_PATH}/include"
)
find_path(NPUCOMPUTE_PROFILING_INCLUDE_DIR
  NAMES profiling/prof_common.h
  HINTS
    "${NPUCOMPUTE_CANN_ROOT}/pkg_inc"
    "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/pkg_inc"
    "$ENV{ASCEND_HOME_PATH}/pkg_inc"
)

if(NOT NPUCOMPUTE_ACL_INCLUDE_DIR OR NOT NPUCOMPUTE_PROFILING_INCLUDE_DIR)
  message(FATAL_ERROR
    "NPU Compute CANN headers were not found. Set NPUCOMPUTE_CANN_ROOT "
    "to the architecture package directory containing include/ and pkg_inc/."
  )
endif()

if(NPU_COMPUTE_BUILD_REAL_HARDWARE_TESTS AND NOT NPU_COMPUTE_BUILD_TESTS)
  message(FATAL_ERROR
    "NPU_COMPUTE_BUILD_REAL_HARDWARE_TESTS requires NPU_COMPUTE_BUILD_TESTS")
endif()

set(NPU_COMPUTE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")

function(npu_compute_set_output target)
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${NPU_COMPUTE_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${NPU_COMPUTE_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${NPU_COMPUTE_OUTPUT_DIR}"
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
target_link_libraries(npu_compute_backend_headers INTERFACE npu_compute_headers)

add_library(npu_compute_integration_headers INTERFACE)
target_link_libraries(npu_compute_integration_headers INTERFACE npu_compute_backend_headers)

if(NPU_COMPUTE_USE_STUB_BACKEND)
  target_include_directories(npu_compute_backend_headers BEFORE INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/stubs/runtime/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/stubs/prof_api/include>"
  )
endif()

add_library(acl_runtime_backend INTERFACE)
add_library(acl_prof_api_backend INTERFACE)

if(NPU_COMPUTE_USE_STUB_BACKEND)
  add_subdirectory(stubs/prof_api)
  add_subdirectory(stubs/runtime)

  target_link_libraries(acl_runtime_backend INTERFACE acl_runtime_stub)
  target_link_libraries(acl_prof_api_backend INTERFACE acl_prof_api_stub)
else()
  get_filename_component(
    NPUCOMPUTE_CANN_ARCH_ROOT "${NPUCOMPUTE_ACL_INCLUDE_DIR}" DIRECTORY)
  find_library(NPUCOMPUTE_ACL_RT_LIBRARY
    NAMES acl_rt
    HINTS
      "${NPUCOMPUTE_CANN_ROOT}/lib64"
      "${NPUCOMPUTE_CANN_ARCH_ROOT}/lib64"
      "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/lib64"
      "$ENV{ASCEND_HOME_PATH}/lib64"
    NO_DEFAULT_PATH
  )
  find_library(NPUCOMPUTE_PROFAPI_LIBRARY
    NAMES profapi
    HINTS
      "${NPUCOMPUTE_CANN_ROOT}/lib64"
      "${NPUCOMPUTE_CANN_ARCH_ROOT}/lib64"
      "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/lib64"
      "$ENV{ASCEND_HOME_PATH}/lib64"
    NO_DEFAULT_PATH
  )
  if(NOT NPUCOMPUTE_ACL_RT_LIBRARY OR NOT NPUCOMPUTE_PROFAPI_LIBRARY)
    message(FATAL_ERROR
      "NPU Compute CANN libraries were not found. Set NPUCOMPUTE_CANN_ROOT "
      "to the architecture package directory containing lib64/libacl_rt.so "
      "and lib64/libprofapi.so."
    )
  endif()

  add_library(CANN::acl_rt SHARED IMPORTED GLOBAL)
  set_target_properties(CANN::acl_rt PROPERTIES
    IMPORTED_LOCATION "${NPUCOMPUTE_ACL_RT_LIBRARY}"
  )
  add_library(CANN::profapi SHARED IMPORTED GLOBAL)
  set_target_properties(CANN::profapi PROPERTIES
    IMPORTED_LOCATION "${NPUCOMPUTE_PROFAPI_LIBRARY}"
  )

  target_link_libraries(acl_runtime_backend INTERFACE CANN::acl_rt)
  target_link_libraries(acl_prof_api_backend INTERFACE CANN::profapi)
endif()

add_subdirectory(src/injection_hook)
add_subdirectory(src/acl_pti)

add_subdirectory(src/npu_compute)
add_subdirectory(src/compute_launcher)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/sections/"
  DESTINATION share/npu-compute/sections
  COMPONENT asc-tools
)

if(NPU_COMPUTE_USE_STUB_BACKEND)
  add_subdirectory(demo)
endif()

if(NPU_COMPUTE_BUILD_TESTS)
  add_subdirectory(stubs/acl_pti_callback)
  add_subdirectory(stubs/hardware_api)
  enable_testing()
  add_subdirectory(tests)
endif()
