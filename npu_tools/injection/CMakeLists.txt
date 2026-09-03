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
  project(acl_tool_injection LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(INJECTION_BUILD_TESTS "Build Runtime injection tests" OFF)
set(INJECTION_CANN_ROOT "" CACHE PATH "CANN architecture package root used by Runtime injection")
set(INJECTION_INSTALL_ARCH_DIR "${CMAKE_SYSTEM_PROCESSOR}-linux")
set(INJECTION_INSTALL_LIBDIR "${INJECTION_INSTALL_ARCH_DIR}/tools/npu_tools/lib64")
set(INJECTION_INSTALL_INCLUDEDIR "${INJECTION_INSTALL_ARCH_DIR}/include")
if(NOT DEFINED NPU_TOOLS_LIBRARY_OUTPUT_DIR)
  set(NPU_TOOLS_LIBRARY_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib64")
endif()
if(NOT DEFINED NPU_TOOLS_RUNTIME_OUTPUT_DIR)
  set(NPU_TOOLS_RUNTIME_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()

function(injection_set_test_output target)
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${NPU_TOOLS_RUNTIME_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${NPU_TOOLS_RUNTIME_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${NPU_TOOLS_RUNTIME_OUTPUT_DIR}"
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
    SKIP_BUILD_RPATH FALSE
    BUILD_WITH_INSTALL_RPATH FALSE
  )
endfunction()

find_path(INJECTION_ACL_INCLUDE_DIR
  NAMES acl/acl.h
  HINTS
    "${INJECTION_CANN_ROOT}/include"
    "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/include"
    "$ENV{ASCEND_HOME_PATH}/include"
)
if(NOT INJECTION_ACL_INCLUDE_DIR)
  message(FATAL_ERROR "Runtime injection CANN ACL headers were not found. Set INJECTION_CANN_ROOT.")
endif()

if(INJECTION_BUILD_TESTS)
  set(INJECTION_ACL_STUB_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/stubs/runtime/include")
  set(INJECTION_PROFILING_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/tests/stubs/prof_api/include"
    CACHE INTERNAL "Runtime injection test profiling headers" FORCE)
else()
  find_path(INJECTION_PROFILING_INCLUDE_DIR
    NAMES profiling/prof_common.h
    HINTS
      "${INJECTION_CANN_ROOT}/pkg_inc"
      "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/pkg_inc"
      "$ENV{ASCEND_HOME_PATH}/pkg_inc"
  )
  if(NOT INJECTION_PROFILING_INCLUDE_DIR)
    message(FATAL_ERROR "Runtime injection CANN profiling headers were not found. Set INJECTION_CANN_ROOT.")
  endif()
endif()

add_library(injection_headers INTERFACE)
target_include_directories(injection_headers INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<BUILD_INTERFACE:${INJECTION_ACL_INCLUDE_DIR}>"
  "$<BUILD_INTERFACE:${INJECTION_PROFILING_INCLUDE_DIR}>"
  "$<BUILD_INTERFACE:${INJECTION_PROFILING_INCLUDE_DIR}/profiling>"
  "$<INSTALL_INTERFACE:include>"
)
if(INJECTION_BUILD_TESTS)
  target_include_directories(injection_headers BEFORE INTERFACE
    "$<BUILD_INTERFACE:${INJECTION_ACL_STUB_INCLUDE_DIR}>"
  )
endif()

add_library(injection_runtime_backend INTERFACE)
add_library(injection_prof_api_backend INTERFACE)
if(INJECTION_BUILD_TESTS)
  add_subdirectory(tests/stubs/prof_api)
  add_subdirectory(tests/stubs/runtime)
  target_link_libraries(injection_runtime_backend INTERFACE acl_runtime_stub)
  target_link_libraries(injection_prof_api_backend INTERFACE acl_prof_api_stub)
else()
  get_filename_component(INJECTION_ACL_INCLUDE_REALPATH "${INJECTION_ACL_INCLUDE_DIR}" REALPATH)
  get_filename_component(INJECTION_CANN_ARCH_ROOT "${INJECTION_ACL_INCLUDE_REALPATH}" DIRECTORY)
  find_library(INJECTION_ACL_RT_LIBRARY NAMES acl_rt HINTS
    "${INJECTION_CANN_ROOT}/lib64"
    "${INJECTION_CANN_ARCH_ROOT}/lib64"
    "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/lib64"
    "$ENV{ASCEND_HOME_PATH}/lib64" NO_DEFAULT_PATH)
  find_library(INJECTION_PROFAPI_LIBRARY NAMES profapi HINTS
    "${INJECTION_CANN_ROOT}/lib64"
    "${INJECTION_CANN_ARCH_ROOT}/lib64"
    "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/lib64"
    "$ENV{ASCEND_HOME_PATH}/lib64" NO_DEFAULT_PATH)
  if(NOT INJECTION_ACL_RT_LIBRARY OR NOT INJECTION_PROFAPI_LIBRARY)
    message(FATAL_ERROR "Runtime injection CANN libraries were not found. Set INJECTION_CANN_ROOT.")
  endif()
  add_library(Injection::acl_rt SHARED IMPORTED GLOBAL)
  set_target_properties(Injection::acl_rt PROPERTIES IMPORTED_LOCATION "${INJECTION_ACL_RT_LIBRARY}")
  add_library(Injection::profapi SHARED IMPORTED GLOBAL)
  set_target_properties(Injection::profapi PROPERTIES IMPORTED_LOCATION "${INJECTION_PROFAPI_LIBRARY}")
  target_link_libraries(injection_runtime_backend INTERFACE Injection::acl_rt)
  target_link_libraries(injection_prof_api_backend INTERFACE Injection::profapi)
endif()

find_package(Threads REQUIRED)
add_library(acl_tool_injection SHARED src/injection_hook.cpp)
target_compile_definitions(acl_tool_injection PRIVATE ACL_TOOL_INJECTION_BUILD=1)
target_include_directories(acl_tool_injection PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(acl_tool_injection PUBLIC injection_headers PRIVATE
  Threads::Threads injection_runtime_backend injection_prof_api_backend)
target_compile_options(acl_tool_injection PRIVATE -Wall -Wextra -Werror)
target_link_options(acl_tool_injection PRIVATE -Wl,-z,defs)
set_target_properties(acl_tool_injection PROPERTIES
  OUTPUT_NAME acl_tool_injection
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  ARCHIVE_OUTPUT_DIRECTORY "${NPU_TOOLS_LIBRARY_OUTPUT_DIR}"
  LIBRARY_OUTPUT_DIRECTORY "${NPU_TOOLS_LIBRARY_OUTPUT_DIR}"
  RUNTIME_OUTPUT_DIRECTORY "${NPU_TOOLS_RUNTIME_OUTPUT_DIR}"
  BUILD_RPATH "$ORIGIN"
  INSTALL_RPATH "$ORIGIN"
  SKIP_BUILD_RPATH FALSE
  BUILD_WITH_INSTALL_RPATH FALSE
)

install(TARGETS acl_tool_injection
  LIBRARY DESTINATION "${INJECTION_INSTALL_LIBDIR}"
  COMPONENT asc-tools)

if(INJECTION_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
