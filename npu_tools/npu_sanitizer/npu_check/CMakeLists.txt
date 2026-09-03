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
set(NPU_CHECK_DBI_ROOT "${CMAKE_CURRENT_LIST_DIR}/../sanitizer_api")
set(NPU_CHECK_DBI_SOURCE_DIR "${NPU_CHECK_DBI_ROOT}/src/dbi")
set(NPU_CHECK_DBI_INCLUDE_DIR "${NPU_CHECK_DBI_ROOT}/include")
set(NPU_CHECK_DBI_TEST_DIR "${NPU_CHECK_DBI_ROOT}/tests/dbi")

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
  src/diagnostic/report_buffer.cpp
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
  PRIVATE acl_san Threads::Threads Boost::filesystem
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
  "LINKER:-rpath,$ORIGIN"
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

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR OR ENABLE_TEST)
  enable_testing()
  if(NOT TARGET GTest::gtest)
    find_package(GTest REQUIRED)
  endif()

  target_compile_definitions(npu_check_common PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_compile_definitions(npu_check_core PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_compile_definitions(npu_check PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  foreach(dbi_target IN ITEMS
      npu_check_dbi_probe_group npu_check_dbi_probe_runtime npu_check_dbi_ctrlbin npu_check_dbi_engine)
    target_compile_definitions(${dbi_target} PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  endforeach()

  add_executable(npu_check_unit_tests
    tests/gtest_main.cpp
    tests/allocation_registry_test.cpp
    tests/logger_test.cpp
    tests/memcheck_test.cpp
    tests/report_buffer_test.cpp
    tests/synccheck_test.cpp
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
  target_link_libraries(npu_check_unit_tests PRIVATE
    npu_check_core acl_san Threads::Threads GTest::gtest Boost::filesystem)
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

  add_executable(npu_check_dbi_pipeline_tests
    tests/gtest_main.cpp
    ${NPU_CHECK_DBI_TEST_DIR}/dbi_pipeline_test.cpp
    ${NPU_CHECK_DBI_SOURCE_DIR}/dbi_pipeline.cpp
    ${NPU_CHECK_DBI_SOURCE_DIR}/tool_runner.cpp
  )
  target_compile_features(npu_check_dbi_pipeline_tests PRIVATE cxx_std_17)
  target_compile_options(npu_check_dbi_pipeline_tests PRIVATE -Wall -Wextra -Werror)
  target_compile_definitions(npu_check_dbi_pipeline_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_include_directories(npu_check_dbi_pipeline_tests PRIVATE ${NPU_CHECK_DBI_INCLUDE_DIR})
  target_link_libraries(npu_check_dbi_pipeline_tests PRIVATE
    npu_check_dbi_ctrlbin npu_check_dbi_probe_runtime GTest::gtest Boost::filesystem)
  add_test(NAME npu_check_dbi_pipeline COMMAND npu_check_dbi_pipeline_tests)

  add_executable(npu_check_dbi_integration_tests
    tests/gtest_main.cpp
    ${NPU_CHECK_DBI_TEST_DIR}/dbi_integration_test.cpp
    ${NPU_CHECK_DBI_SOURCE_DIR}/dbi_pipeline.cpp
    ${NPU_CHECK_DBI_SOURCE_DIR}/tool_runner.cpp
  )
  target_compile_features(npu_check_dbi_integration_tests PRIVATE cxx_std_17)
  target_compile_options(npu_check_dbi_integration_tests PRIVATE -Wall -Wextra -Werror)
  target_compile_definitions(npu_check_dbi_integration_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_include_directories(npu_check_dbi_integration_tests PRIVATE ${NPU_CHECK_DBI_INCLUDE_DIR})
  target_link_libraries(npu_check_dbi_integration_tests PRIVATE
    npu_check_dbi_ctrlbin npu_check_dbi_probe_runtime GTest::gtest Boost::filesystem)
  add_test(NAME npu_check_dbi_integration COMMAND npu_check_dbi_integration_tests)

  add_executable(npu_check_dbi_binary_instrumenter_tests
    tests/gtest_main.cpp
    ${NPU_CHECK_DBI_TEST_DIR}/binary_instrumenter_test.cpp
  )
  target_compile_features(npu_check_dbi_binary_instrumenter_tests PRIVATE cxx_std_17)
  target_compile_definitions(npu_check_dbi_binary_instrumenter_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_include_directories(npu_check_dbi_binary_instrumenter_tests PRIVATE ${NPU_CHECK_DBI_INCLUDE_DIR})
  target_link_libraries(npu_check_dbi_binary_instrumenter_tests PRIVATE
    npu_check_dbi_engine GTest::gtest Boost::filesystem)
  add_test(NAME npu_check_dbi_binary_instrumenter COMMAND npu_check_dbi_binary_instrumenter_tests)

  add_executable(npu_check_probe_source_generator_tests
    ${NPU_CHECK_DBI_TEST_DIR}/probe_source_generator_test.cpp
  )
  target_compile_features(npu_check_probe_source_generator_tests PRIVATE cxx_std_17)
  target_compile_definitions(npu_check_probe_source_generator_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
  target_include_directories(npu_check_probe_source_generator_tests PRIVATE ${NPU_CHECK_DBI_INCLUDE_DIR})
  target_link_libraries(npu_check_probe_source_generator_tests PRIVATE npu_check_dbi_probe_runtime)
  add_test(NAME npu_check_probe_source_generator COMMAND npu_check_probe_source_generator_tests)
endif()
