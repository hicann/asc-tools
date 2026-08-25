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
project(npu_check_tool LANGUAGES C CXX)

find_package(Threads REQUIRED)

set(NPU_CHECK_REPORT_RENDERER_SOURCES
  src/diagnostic/report_renderer.cpp
  src/diagnostic/report/report_catalog.cpp
  src/diagnostic/report/report_fields.cpp
  src/diagnostic/report/report_normalizer.cpp
  src/diagnostic/report/report_summary.cpp
  src/diagnostic/report/report_text_renderer.cpp
  src/diagnostic/report/synccheck_adapter.cpp
)

if(NOT TARGET npu_check_common)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../common
    ${CMAKE_CURRENT_BINARY_DIR}/npu_check_common)
endif()

if(NOT TARGET acl_san)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../sanitizer_api
    ${CMAKE_CURRENT_BINARY_DIR}/sanitizer_api)
endif()

add_library(npu_check_core STATIC
  src/checker/allocation_registry.cpp
  src/checker/memcheck.cpp
  src/checker/synccheck.cpp
  src/ipc/uds_server.cpp
  src/logging/logger.cpp
  src/tool_manager/tool_manager.cpp
)
target_compile_features(npu_check_core PUBLIC cxx_std_17)
target_include_directories(npu_check_core
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
  PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../sanitizer_api/include
)
target_link_libraries(npu_check_core
  PUBLIC npu_check_common
  PRIVATE acl_san Threads::Threads
)
set_target_properties(npu_check_core PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

add_library(npu_check SHARED
  src/tool_manager/entry.cpp
  ${NPU_CHECK_REPORT_RENDERER_SOURCES}
)
target_compile_features(npu_check PRIVATE cxx_std_17)
target_include_directories(npu_check
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../sanitizer_api/include
)
target_link_libraries(npu_check
  PRIVATE npu_check_core acl_san Threads::Threads
)
target_compile_options(npu_check PRIVATE -Wall -Wextra -Wpedantic)
target_link_options(npu_check PRIVATE
  "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/cmake/npu_check.map"
  "LINKER:-z,nodelete"
)
set_target_properties(npu_check PROPERTIES
  OUTPUT_NAME npu_check
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  BUILD_RPATH "\$ORIGIN"
  INSTALL_RPATH "\$ORIGIN"
  SKIP_BUILD_RPATH FALSE
  SKIP_INSTALL_RPATH FALSE
)

install(TARGETS npu_check
  LIBRARY DESTINATION lib ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
)
install(FILES include/npu_check.h
  DESTINATION include/npu_check ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR ENABLE_TEST)
  enable_testing()
  find_package(GTest REQUIRED)

  target_compile_definitions(npu_check_common PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_compile_definitions(npu_check_core PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_compile_definitions(npu_check PRIVATE _GLIBCXX_USE_CXX11_ABI=0)

  add_executable(npu_check_unit_tests
    tests/gtest_main.cpp
    tests/allocation_registry_test.cpp
    tests/logger_test.cpp
    tests/memcheck_test.cpp
    tests/uds_server_test.cpp
    tests/wire_protocol_test.cpp
    ${NPU_CHECK_REPORT_RENDERER_SOURCES}
  )
  target_compile_features(npu_check_unit_tests PRIVATE cxx_std_17)
  target_compile_definitions(npu_check_unit_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_include_directories(npu_check_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../sanitizer_api/include
  )
  target_link_libraries(npu_check_unit_tests PRIVATE npu_check_core acl_san Threads::Threads GTest::gtest)
  add_test(NAME npu_check_unit_tests COMMAND npu_check_unit_tests)

  add_executable(npu_check_report_renderer_smoke
    tests/report_renderer_smoke.cpp
    ${NPU_CHECK_REPORT_RENDERER_SOURCES}
  )
  target_compile_features(npu_check_report_renderer_smoke PRIVATE cxx_std_17)
  target_compile_definitions(npu_check_report_renderer_smoke PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_include_directories(npu_check_report_renderer_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/../sanitizer_api/include
  )
  target_link_libraries(npu_check_report_renderer_smoke PRIVATE GTest::gtest)
  add_test(NAME npu_check_report_renderer_smoke COMMAND npu_check_report_renderer_smoke)
endif()
