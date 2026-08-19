# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16.0)
project(aclsan_sanitizer_api)

find_package(Threads REQUIRED)

if(NOT TARGET npu_compute_integration_headers OR NOT TARGET acl_tool_injection)
  message(FATAL_ERROR
    "sanitizer_api requires npu_compute integration targets: npu_compute_integration_headers and acl_tool_injection")
endif()

add_library(acl_san SHARED
  src/aclsan_api.cpp
  src/aclsan_dispatch_cb.cpp
  src/aclsan_hook_cbdata.cpp
  src/aclsan_hook_aclrt.cpp
  src/aclsan_translate_device_data.cpp
  src/ascsan_api_core.cpp
  src/ascsan_callback_registry.cpp
  src/ascsan_api.cpp
  src/ascsan_hook_plan_builder.cpp
  src/ascsan_memory_service.cpp
  src/ascsan_patch_manager.cpp
  src/ascsan_trace_manager.cpp
)

target_compile_features(acl_san PUBLIC cxx_std_17)
target_include_directories(acl_san
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(acl_san
  PRIVATE
    npu_compute_integration_headers
    acl_tool_injection
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

set_target_properties(acl_san PROPERTIES
  BUILD_WITH_INSTALL_RPATH YES
  INSTALL_RPATH "$ORIGIN"
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  OUTPUT_NAME acl_san
)

if(ENABLE_TEST)
  add_executable(aclsan_callback_contract tests/aclsan_callback_contract.cpp)
  target_compile_features(aclsan_callback_contract PRIVATE cxx_std_17)
  target_link_libraries(aclsan_callback_contract PRIVATE acl_san)

  add_executable(aclsan_lifecycle_api_removed tests/aclsan_lifecycle_api_removed.cpp)
  target_compile_features(aclsan_lifecycle_api_removed PRIVATE cxx_std_17)
  target_link_libraries(aclsan_lifecycle_api_removed PRIVATE ${CMAKE_DL_LIBS})
  add_dependencies(aclsan_lifecycle_api_removed acl_san)
endif()

install(TARGETS acl_san
  LIBRARY DESTINATION lib ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
)

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/aclsan/"
  DESTINATION include/aclsan ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
  FILES_MATCHING PATTERN "*.h"
)
