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
project(aclsan_cann_sanitizer)

if(NOT TARGET aclsan_api)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../sanitizer_api
    ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_api)
endif()

add_library(cann_sanitizer SHARED
  src/callback_dispatcher.cpp
  src/cann_sanitizer.cpp
  src/dummy_checker.cpp
  src/instruction_parser.cpp
  src/tool_profiles.cpp
)

add_library(aclsan_report_renderer STATIC
  src/report_renderer.cpp
)

add_executable(cann_sanitizer_report_renderer_demo
  examples/report_renderer_bundle_demo.cpp
)

target_compile_features(cann_sanitizer PUBLIC cxx_std_17)
target_compile_features(aclsan_report_renderer PUBLIC cxx_std_17)
target_compile_features(cann_sanitizer_report_renderer_demo PRIVATE cxx_std_17)
target_include_directories(cann_sanitizer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(aclsan_report_renderer
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_link_libraries(cann_sanitizer PRIVATE aclsan_api dl pthread)
target_link_libraries(cann_sanitizer_report_renderer_demo PRIVATE aclsan_report_renderer)

set_target_properties(cann_sanitizer PROPERTIES
  OUTPUT_NAME cann_sanitizer
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR ENABLE_TEST)
  enable_testing()

  add_executable(cann_sanitizer_smoke tests/cann_sanitizer_smoke.cpp)
  target_compile_features(cann_sanitizer_smoke PRIVATE cxx_std_17)
  target_link_libraries(cann_sanitizer_smoke PRIVATE cann_sanitizer aclsan_api)

  find_package(GTest REQUIRED)
  add_executable(cann_sanitizer_report_renderer_smoke tests/report_renderer_smoke.cpp)
  target_compile_features(cann_sanitizer_report_renderer_smoke PRIVATE cxx_std_17)
  target_include_directories(cann_sanitizer_report_renderer_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_link_libraries(cann_sanitizer_report_renderer_smoke PRIVATE aclsan_report_renderer GTest::gtest)
  add_test(NAME cann_sanitizer_report_renderer_smoke COMMAND cann_sanitizer_report_renderer_smoke)

  add_test(NAME cann_sanitizer_report_renderer_demo COMMAND cann_sanitizer_report_renderer_demo)
  set_tests_properties(cann_sanitizer_report_renderer_demo PROPERTIES
    PASS_REGULAR_EXPRESSION "========= SYNCCHECK SUMMARY: 5 errors, 1 warnings, 0 infos, 1 deadlocks"
  )
endif()

install(TARGETS cann_sanitizer
  LIBRARY DESTINATION lib ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
)

install(FILES
  include/aclsan/cann_sanitizer.h
  DESTINATION include/aclsan ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
)
