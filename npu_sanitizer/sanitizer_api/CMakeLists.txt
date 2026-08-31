# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
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

if(NOT TARGET npu_check_dbi_engine)
  add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../dbi
    ${CMAKE_CURRENT_BINARY_DIR}/npu_check_dbi)
endif()

if(NOT TARGET npu_compute_integration_headers OR NOT TARGET acl_tool_injection)
  message(FATAL_ERROR
    "sanitizer_api requires npu_compute integration targets: npu_compute_integration_headers and acl_tool_injection")
endif()

add_library(acl_san SHARED
  src/device_runtime/device_binary_registry.cpp
  src/device_runtime/device_symbolizer.cpp
  src/device_instr/decoder_registry.cpp
  src/device_instr/arch/dav_3510/decoder.cpp
  src/device_instr/arch/dav_3510/register_state_manager.cpp
  src/aclsan_active_probe_plan.cpp
  src/aclsan_memory_cbdata.cpp
  src/aclsan_trace_buffer.cpp
  src/aclsan_trace_runtime.cpp
  src/aclsan/aclsan_api.cpp
  src/aclsan/aclsan_dispatch.cpp
  src/aclsan/aclsan_hook_aclrt.cpp
  src/aclsan/aclsan_device_data_log.cpp
  src/aclsan/aclsan_translate_device_data.cpp
)

target_compile_features(acl_san PUBLIC cxx_std_17)
target_include_directories(acl_san
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../dbi/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(acl_san
  PRIVATE
    npu_compute_integration_headers
    acl_tool_injection
    npu_check_dbi_engine
    ${CMAKE_DL_LIBS}
    Threads::Threads
)
target_link_options(acl_san PRIVATE "LINKER:--exclude-libs,ALL")

set(ACLSAN_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/acl_san.map")
if(ENABLE_TEST)
  set(ACLSAN_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/acl_san_test.map")
endif()
target_link_options(acl_san PRIVATE
  "LINKER:--version-script=${ACLSAN_VERSION_SCRIPT}"
)
set_property(TARGET acl_san APPEND PROPERTY
  LINK_DEPENDS "${ACLSAN_VERSION_SCRIPT}"
)

set_target_properties(acl_san PROPERTIES
  BUILD_WITH_INSTALL_RPATH YES
  INSTALL_RPATH "$ORIGIN"
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  OUTPUT_NAME acl_san
)

if(ENABLE_TEST)
  enable_testing()
  target_compile_definitions(acl_san PRIVATE ACLSAN_ENABLE_TEST_API=1)
  add_test(NAME acl_san_exports
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_acl_san_exports.sh"
      $<TARGET_FILE:acl_san> --with-test-api)
  add_test(NAME acl_san_version_scripts
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_acl_san_version_scripts.sh"
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/acl_san.map"
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/acl_san_test.map")
  add_test(NAME aclsan.probe_contract
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_probe_contract.sh")
  add_test(NAME aclsan.device_memory_access_producer_contract
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_device_memory_access_producer_contract.sh")

  add_executable(aclsan_callback_contract tests/aclsan_callback_contract.cpp)
  target_compile_features(aclsan_callback_contract PRIVATE cxx_std_17)
  target_link_libraries(aclsan_callback_contract PRIVATE acl_san)

  add_executable(aclsan_log_test tests/aclsan_log_test.cpp)
  target_compile_features(aclsan_log_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_log_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_log_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME aclsan_log_test COMMAND aclsan_log_test)

  add_executable(aclsan_callback_invoke_test
    tests/aclsan_callback_invoke_test.cpp
    src/aclsan_active_probe_plan.cpp
    src/aclsan/aclsan_api.cpp
  )
  target_compile_features(aclsan_callback_invoke_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_callback_invoke_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_callback_invoke_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  target_link_libraries(aclsan_callback_invoke_test PRIVATE npu_compute_integration_headers)
  add_test(NAME aclsan_callback_invoke_test COMMAND aclsan_callback_invoke_test)

  add_executable(aclsan_probe_plan_callback_test
    tests/aclsan_probe_plan_callback_test.cpp
    src/aclsan_active_probe_plan.cpp
    src/aclsan/aclsan_api.cpp
  )
  target_compile_features(aclsan_probe_plan_callback_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_probe_plan_callback_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_probe_plan_callback_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  target_link_libraries(aclsan_probe_plan_callback_test PRIVATE
    npu_compute_integration_headers
    npu_check_dbi_engine
    Threads::Threads)
  add_test(NAME aclsan_probe_plan_callback_test COMMAND aclsan_probe_plan_callback_test)

  add_executable(aclsan_hook_cbdata_test
    tests/aclsan_hook_cbdata_test.cpp
    src/aclsan_active_probe_plan.cpp
    src/aclsan_trace_buffer.cpp
    src/aclsan_trace_runtime.cpp
    src/device_runtime/device_binary_registry.cpp
    src/device_runtime/device_symbolizer.cpp
    src/aclsan/aclsan_device_data_log.cpp
    src/aclsan/aclsan_translate_device_data.cpp
    src/aclsan_memory_cbdata.cpp
    src/device_instr/decoder_registry.cpp
    src/device_instr/arch/dav_3510/decoder.cpp
    src/device_instr/arch/dav_3510/register_state_manager.cpp
  )
  target_compile_features(aclsan_hook_cbdata_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_hook_cbdata_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_hook_cbdata_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  target_link_libraries(aclsan_hook_cbdata_test PRIVATE
    npu_compute_integration_headers
    npu_check_dbi_engine
    ${CMAKE_DL_LIBS}
    Threads::Threads)

  add_executable(aclsan_device_symbolizer_test
    tests/device_symbolizer_test.cpp
    src/device_runtime/device_symbolizer.cpp
  )
  target_compile_features(aclsan_device_symbolizer_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_device_symbolizer_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_device_symbolizer_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/device_runtime)
  add_test(NAME aclsan_device_symbolizer_test COMMAND aclsan_device_symbolizer_test)

  add_executable(aclsan_device_binary_registry_test
    tests/device_binary_registry_test.cpp
    src/device_runtime/device_binary_registry.cpp
    src/device_runtime/device_symbolizer.cpp
  )
  target_compile_features(aclsan_device_binary_registry_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_device_binary_registry_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_device_binary_registry_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  add_test(NAME aclsan_device_binary_registry_test COMMAND aclsan_device_binary_registry_test)

  add_executable(aclsan_memory_cbdata_test
    tests/aclsan_memory_cbdata_test.cpp
    src/aclsan_memory_cbdata.cpp
  )
  target_compile_features(aclsan_memory_cbdata_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_memory_cbdata_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_memory_cbdata_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  target_link_libraries(aclsan_memory_cbdata_test PRIVATE npu_compute_integration_headers)
  add_test(NAME aclsan_memory_cbdata_test COMMAND aclsan_memory_cbdata_test)
  add_executable(aclsan_trace_buffer_test
    tests/aclsan_trace_buffer.cpp
    src/aclsan_trace_buffer.cpp
  )
  target_compile_features(aclsan_trace_buffer_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_trace_buffer_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_trace_buffer_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  add_test(NAME aclsan_trace_buffer_test COMMAND aclsan_trace_buffer_test)

  add_executable(aclsan_translate_device_data_test
    tests/aclsan_translate_device_data_test.cpp
    src/aclsan/aclsan_device_data_log.cpp
    src/aclsan/aclsan_translate_device_data.cpp
    src/aclsan_memory_cbdata.cpp
    src/device_instr/decoder_registry.cpp
    src/device_instr/arch/dav_3510/decoder.cpp
  )
  target_compile_features(aclsan_translate_device_data_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_translate_device_data_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_translate_device_data_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  target_link_libraries(aclsan_translate_device_data_test PRIVATE npu_compute_integration_headers)

  add_executable(cce_instr_header_contract_test tests/cce_instr_header_contract_test.cpp)
  target_compile_features(cce_instr_header_contract_test PRIVATE cxx_std_17)
  target_compile_options(cce_instr_header_contract_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(cce_instr_header_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  add_test(NAME cce_instr_header_contract_test COMMAND cce_instr_header_contract_test)

  add_executable(cce_instruction_pipeline_test
    tests/cce_instruction_pipeline_test.cpp
    src/aclsan/aclsan_device_data_log.cpp
    src/aclsan/aclsan_translate_device_data.cpp
    src/aclsan_memory_cbdata.cpp
    src/device_instr/decoder_registry.cpp
    src/device_instr/arch/dav_3510/decoder.cpp
  )
  target_compile_features(cce_instruction_pipeline_test PRIVATE cxx_std_17)
  target_compile_options(cce_instruction_pipeline_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(cce_instruction_pipeline_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  add_test(NAME cce_instruction_pipeline_test COMMAND cce_instruction_pipeline_test)

  add_executable(device_instr_decoder_test
    tests/device_instr_decoder_test.cpp
    src/device_instr/decoder_registry.cpp
    src/device_instr/arch/dav_3510/decoder.cpp
  )
  target_compile_features(device_instr_decoder_test PRIVATE cxx_std_17)
  target_compile_options(device_instr_decoder_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(device_instr_decoder_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  add_test(NAME device_instr_decoder_test COMMAND device_instr_decoder_test)

  add_executable(register_state_manager_test
    tests/register_state_manager_test.cpp
    src/device_instr/arch/dav_3510/register_state_manager.cpp
  )
  target_compile_features(register_state_manager_test PRIVATE cxx_std_17)
  target_compile_options(register_state_manager_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(register_state_manager_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
  add_test(NAME register_state_manager_test COMMAND register_state_manager_test)

  add_executable(aclsan_lifecycle_api_removed tests/aclsan_lifecycle_api_removed.cpp)
  target_compile_features(aclsan_lifecycle_api_removed PRIVATE cxx_std_17)
  target_link_libraries(aclsan_lifecycle_api_removed PRIVATE ${CMAKE_DL_LIBS})
  add_dependencies(aclsan_lifecycle_api_removed acl_san)

  add_executable(aclsan_symbolizer_api_test tests/aclsan_symbolizer_api_test.cpp)
  target_compile_features(aclsan_symbolizer_api_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_symbolizer_api_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_symbolizer_api_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(aclsan_symbolizer_api_test PRIVATE acl_san)
  add_test(NAME aclsan_symbolizer_api_test COMMAND aclsan_symbolizer_api_test)

  if(TARGET acl_runtime_stub)
    add_executable(aclsan_trace_d2h_hook tests/aclsan_trace_d2h_hook.cpp)
    target_compile_features(aclsan_trace_d2h_hook PRIVATE cxx_std_17)
    target_include_directories(aclsan_trace_d2h_hook PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
    target_link_libraries(aclsan_trace_d2h_hook PRIVATE acl_san acl_tool_injection acl_runtime_stub)
    add_test(NAME aclsan_trace_d2h_hook COMMAND aclsan_trace_d2h_hook)

    add_executable(aclsan_launch_with_host_args_hook tests/aclsan_launch_with_host_args_hook.cpp)
    target_compile_features(aclsan_launch_with_host_args_hook PRIVATE cxx_std_17)
    target_link_libraries(aclsan_launch_with_host_args_hook PRIVATE acl_san acl_tool_injection acl_runtime_stub)
    add_test(NAME aclsan_launch_with_host_args_hook COMMAND aclsan_launch_with_host_args_hook)

    add_executable(aclsan_binary_load_dbi_hook tests/aclsan_binary_load_dbi_hook.cpp)
    target_compile_features(aclsan_binary_load_dbi_hook PRIVATE cxx_std_17)
    target_link_libraries(aclsan_binary_load_dbi_hook PRIVATE acl_san acl_tool_injection acl_runtime_stub)
    add_test(NAME aclsan_binary_load_dbi_hook COMMAND aclsan_binary_load_dbi_hook)

    add_executable(aclsan_full_flow_host_sample
      ${CMAKE_CURRENT_LIST_DIR}/../dbi/test/full_flow_host_sample.cpp)
    target_compile_features(aclsan_full_flow_host_sample PRIVATE cxx_std_17)
    target_compile_options(aclsan_full_flow_host_sample PRIVATE -Wno-error=cast-function-type)
    target_include_directories(aclsan_full_flow_host_sample PRIVATE
      ${CMAKE_CURRENT_LIST_DIR}/../dbi/include)
    target_link_libraries(aclsan_full_flow_host_sample PRIVATE acl_san acl_tool_injection acl_runtime_stub)

    add_executable(aclsan_full_flow_fake_dbi_tool
      ${CMAKE_CURRENT_LIST_DIR}/../dbi/test/fake_dbi_tool.cpp)
    target_compile_features(aclsan_full_flow_fake_dbi_tool PRIVATE cxx_std_17)
    target_compile_options(aclsan_full_flow_fake_dbi_tool PRIVATE -Wall -Wextra -Werror)
    add_test(NAME aclsan.full_flow_host_sample
      COMMAND ${CMAKE_COMMAND}
        -DSAMPLE=$<TARGET_FILE:aclsan_full_flow_host_sample>
        -DFAKE_TOOL=$<TARGET_FILE:aclsan_full_flow_fake_dbi_tool>
        -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/full-flow-host
        -P ${CMAKE_CURRENT_LIST_DIR}/../dbi/test/run_full_flow_host_sample.cmake)
  endif()
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
