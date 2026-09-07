# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set(NPU_CHECK_SOURCE_DIR "${CMAKE_SOURCE_DIR}/npu_tools/npu_sanitizer/npu_check")
set(NPU_CHECK_DBI_ROOT "${CMAKE_SOURCE_DIR}/npu_tools/npu_sanitizer/sanitizer_api")
set(NPU_CHECK_DBI_SOURCE_DIR "${NPU_CHECK_DBI_ROOT}/src/dbi")
set(NPU_CHECK_DBI_INCLUDE_DIR "${NPU_CHECK_DBI_ROOT}/include")
set(NPU_CHECK_DBI_TEST_DIR "${NPU_TOOLS_TEST_ROOT}/npu_sanitizer/sanitizer_api/tests/dbi")
set(NPU_CHECK_TEST_REPORT_RENDERER_SOURCES
  "${NPU_CHECK_SOURCE_DIR}/src/diagnostic/report_renderer.cpp"
  "${NPU_CHECK_SOURCE_DIR}/src/diagnostic/report/report_catalog.cpp"
  "${NPU_CHECK_SOURCE_DIR}/src/diagnostic/report/report_fields.cpp"
  "${NPU_CHECK_SOURCE_DIR}/src/diagnostic/report/report_normalizer.cpp"
  "${NPU_CHECK_SOURCE_DIR}/src/diagnostic/report/report_summary.cpp"
  "${NPU_CHECK_SOURCE_DIR}/src/diagnostic/report/report_text_renderer.cpp"
  "${NPU_CHECK_SOURCE_DIR}/src/diagnostic/report/synccheck_adapter.cpp"
)

if(NOT TARGET GTest::gtest)
  find_package(GTest REQUIRED)
endif()

add_executable(npu_check_unit_tests
  gtest_main.cpp
  allocation_registry_test.cpp
  logger_test.cpp
  memcheck_test.cpp
  report_buffer_test.cpp
  synccheck_test.cpp
  uds_server_test.cpp
  wire_protocol_test.cpp
  ${NPU_CHECK_TEST_REPORT_RENDERER_SOURCES}
)
target_compile_features(npu_check_unit_tests PRIVATE cxx_std_17)
target_compile_definitions(npu_check_unit_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(npu_check_unit_tests PRIVATE
  "${NPU_CHECK_SOURCE_DIR}/src"
  "${NPU_CHECK_DBI_INCLUDE_DIR}"
)
target_link_libraries(npu_check_unit_tests PRIVATE
  npu_check_core acl_san Threads::Threads GTest::gtest Boost::filesystem)
add_test(NAME npu_check_unit_tests COMMAND npu_check_unit_tests)

add_executable(npu_check_report_renderer_smoke
  report_renderer_smoke.cpp
  ${NPU_CHECK_TEST_REPORT_RENDERER_SOURCES}
)
target_compile_features(npu_check_report_renderer_smoke PRIVATE cxx_std_17)
target_compile_definitions(npu_check_report_renderer_smoke PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(npu_check_report_renderer_smoke PRIVATE
  "${NPU_CHECK_SOURCE_DIR}/src"
  "${NPU_CHECK_DBI_INCLUDE_DIR}"
)
target_link_libraries(npu_check_report_renderer_smoke PRIVATE GTest::gtest)
add_test(NAME npu_check_report_renderer_smoke COMMAND npu_check_report_renderer_smoke)

add_executable(npu_check_dbi_pipeline_tests
  gtest_main.cpp
  "${NPU_CHECK_DBI_TEST_DIR}/dbi_pipeline_test.cpp"
  "${NPU_CHECK_DBI_SOURCE_DIR}/dbi_pipeline.cpp"
  "${NPU_CHECK_DBI_SOURCE_DIR}/tool_runner.cpp"
)
target_compile_features(npu_check_dbi_pipeline_tests PRIVATE cxx_std_17)
target_compile_options(npu_check_dbi_pipeline_tests PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(npu_check_dbi_pipeline_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(npu_check_dbi_pipeline_tests PRIVATE "${NPU_CHECK_DBI_INCLUDE_DIR}")
target_link_libraries(npu_check_dbi_pipeline_tests PRIVATE
  npu_check_dbi_ctrlbin npu_check_dbi_probe_runtime GTest::gtest Boost::filesystem)
add_test(NAME npu_check_dbi_pipeline COMMAND npu_check_dbi_pipeline_tests)

add_executable(npu_check_dbi_integration_tests
  gtest_main.cpp
  "${NPU_CHECK_DBI_TEST_DIR}/dbi_integration_test.cpp"
  "${NPU_CHECK_DBI_SOURCE_DIR}/dbi_pipeline.cpp"
  "${NPU_CHECK_DBI_SOURCE_DIR}/tool_runner.cpp"
)
target_compile_features(npu_check_dbi_integration_tests PRIVATE cxx_std_17)
target_compile_options(npu_check_dbi_integration_tests PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(npu_check_dbi_integration_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(npu_check_dbi_integration_tests PRIVATE "${NPU_CHECK_DBI_INCLUDE_DIR}")
target_link_libraries(npu_check_dbi_integration_tests PRIVATE
  npu_check_dbi_ctrlbin npu_check_dbi_probe_runtime GTest::gtest Boost::filesystem)
add_test(NAME npu_check_dbi_integration COMMAND npu_check_dbi_integration_tests)

add_executable(npu_check_dbi_binary_instrumenter_tests
  gtest_main.cpp
  "${NPU_CHECK_DBI_TEST_DIR}/binary_instrumenter_test.cpp"
)
target_compile_features(npu_check_dbi_binary_instrumenter_tests PRIVATE cxx_std_17)
target_compile_definitions(npu_check_dbi_binary_instrumenter_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(npu_check_dbi_binary_instrumenter_tests PRIVATE "${NPU_CHECK_DBI_INCLUDE_DIR}")
target_link_libraries(npu_check_dbi_binary_instrumenter_tests PRIVATE
  npu_check_dbi_engine GTest::gtest Boost::filesystem)
add_test(NAME npu_check_dbi_binary_instrumenter COMMAND npu_check_dbi_binary_instrumenter_tests)

add_executable(npu_check_probe_source_generator_tests
  "${NPU_CHECK_DBI_TEST_DIR}/probe_source_generator_test.cpp"
)
target_compile_features(npu_check_probe_source_generator_tests PRIVATE cxx_std_17)
target_compile_definitions(npu_check_probe_source_generator_tests PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
target_include_directories(npu_check_probe_source_generator_tests PRIVATE "${NPU_CHECK_DBI_INCLUDE_DIR}")
target_link_libraries(npu_check_probe_source_generator_tests PRIVATE
  npu_check_dbi_probe_runtime npu_check_dbi_ctrlbin)
add_test(NAME npu_check_probe_source_generator COMMAND npu_check_probe_source_generator_tests)
