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

option(ACLSAN_BUILD_DEVICE_PROBE_RESOURCES "Build ACLSan Device probe resources" OFF)

if(NOT TARGET npu_compute_integration_headers OR NOT TARGET acl_tool_injection)
  message(FATAL_ERROR
    "sanitizer_api requires npu_compute integration targets: npu_compute_integration_headers and acl_tool_injection")
endif()

add_library(acl_san SHARED
  probe/device_symbolizer.cpp
  probe/image_transformer.cpp
  probe/probe_parser.cpp
  probe/probe_runtime.cpp
  src/cce_instr_struct_dma.cpp
  src/cce_instr_types.cpp
  src/aclsan_api.cpp
  src/aclsan_dispatch_cb.cpp
  src/aclsan_hook_aclrt.cpp
  src/aclsan_translate_device_data.cpp
)

target_compile_features(acl_san PUBLIC cxx_std_17)
target_include_directories(acl_san
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(acl_san
  PRIVATE
    npu_compute_integration_headers
    acl_tool_injection
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

set(ACLSAN_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/acl_san.map")
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

if(ACLSAN_BUILD_DEVICE_PROBE_RESOURCES)
  find_program(ACLSAN_BISHENG_EXECUTABLE bisheng REQUIRED)
  if(NOT CMAKE_ASC_ARCHITECTURES)
    message(FATAL_ERROR "ACLSAN_BUILD_DEVICE_PROBE_RESOURCES requires CMAKE_ASC_ARCHITECTURES")
  endif()

  set(ACLSAN_PROBE_RESOURCE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/probe_resources")
  set(ACLSAN_PROBE_OBJECT "${ACLSAN_PROBE_RESOURCE_DIRECTORY}/probe.o")
  set(ACLSAN_PROBE_CTRL_BINARY "${ACLSAN_PROBE_RESOURCE_DIRECTORY}/ctrl.bin")

  add_executable(aclsan_probe_ctrlgen
    probe/gen_ctrlbin.cpp
    probe/msbit/MSBit.cpp
  )
  target_compile_features(aclsan_probe_ctrlgen PRIVATE cxx_std_17)
  target_compile_options(aclsan_probe_ctrlgen PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_probe_ctrlgen PRIVATE probe/msbit)

  add_custom_command(
    OUTPUT "${ACLSAN_PROBE_OBJECT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ACLSAN_PROBE_RESOURCE_DIRECTORY}"
    COMMAND "${ACLSAN_BISHENG_EXECUTABLE}"
      "--npu-arch=${CMAKE_ASC_ARCHITECTURES}"
      --cce-aicore-only
      -std=c++17
      -O2
      -mllvm -cce-aicore-record-overflow=false
      -mllvm -cce-aicore-record-stack-size=false
      -mllvm -cce-aicore-addr-transform
      -DBUILD_DYNAMIC_PROBE
      "-I${CMAKE_CURRENT_SOURCE_DIR}/probe"
      -c "${CMAKE_CURRENT_SOURCE_DIR}/probe/probe.asc"
      -o "${ACLSAN_PROBE_OBJECT}"
    DEPENDS probe/probe.asc probe/probe_record.h
    VERBATIM
  )

  add_custom_command(
    OUTPUT "${ACLSAN_PROBE_CTRL_BINARY}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ACLSAN_PROBE_RESOURCE_DIRECTORY}"
    COMMAND $<TARGET_FILE:aclsan_probe_ctrlgen> "${ACLSAN_PROBE_CTRL_BINARY}"
    DEPENDS aclsan_probe_ctrlgen probe/gen_ctrlbin.cpp
    VERBATIM
  )

  add_custom_target(aclsan_probe_resources ALL
    DEPENDS "${ACLSAN_PROBE_OBJECT}" "${ACLSAN_PROBE_CTRL_BINARY}"
  )
  add_dependencies(acl_san aclsan_probe_resources)
endif()

if(ENABLE_TEST)
  enable_testing()
  add_test(NAME acl_san_exports
    COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/check_acl_san_exports.sh" $<TARGET_FILE:acl_san>)

  add_executable(aclsan_callback_contract tests/aclsan_callback_contract.cpp)
  target_compile_features(aclsan_callback_contract PRIVATE cxx_std_17)
  target_link_libraries(aclsan_callback_contract PRIVATE acl_san)

  add_executable(aclsan_hook_cbdata_test
    tests/aclsan_hook_cbdata_test.cpp
    probe/device_symbolizer.cpp
    probe/image_transformer.cpp
    probe/probe_parser.cpp
    probe/probe_runtime.cpp
    src/aclsan_translate_device_data.cpp
    src/cce_instr_struct_dma.cpp
    src/cce_instr_types.cpp
  )
  target_compile_features(aclsan_hook_cbdata_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_hook_cbdata_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_hook_cbdata_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(aclsan_hook_cbdata_test PRIVATE
    npu_compute_integration_headers
    ${CMAKE_DL_LIBS}
    Threads::Threads)

  add_executable(aclsan_probe_parser_test
    tests/probe_parser_test.cpp
    probe/probe_parser.cpp
    src/cce_instr_types.cpp
  )
  target_compile_features(aclsan_probe_parser_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_probe_parser_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_probe_parser_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/probe
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

  add_executable(aclsan_probe_runtime_test
    tests/probe_runtime_test.cpp
    probe/device_symbolizer.cpp
    probe/image_transformer.cpp
    probe/probe_parser.cpp
    probe/probe_runtime.cpp
    src/cce_instr_types.cpp
  )
  target_compile_features(aclsan_probe_runtime_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_probe_runtime_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_probe_runtime_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/probe
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(aclsan_probe_runtime_test PRIVATE
    npu_compute_integration_headers
    Threads::Threads)
  add_test(NAME aclsan_probe_runtime_test COMMAND aclsan_probe_runtime_test)

  add_executable(aclsan_device_symbolizer_test
    tests/device_symbolizer_test.cpp
    probe/device_symbolizer.cpp
    probe/image_transformer.cpp
  )
  target_compile_features(aclsan_device_symbolizer_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_device_symbolizer_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_device_symbolizer_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/probe)
  add_test(NAME aclsan_device_symbolizer_test COMMAND aclsan_device_symbolizer_test)

  add_executable(aclsan_translate_device_data_test
    tests/aclsan_translate_device_data_test.cpp
    src/aclsan_translate_device_data.cpp
    src/cce_instr_struct_dma.cpp
    src/cce_instr_types.cpp
  )
  target_compile_features(aclsan_translate_device_data_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_translate_device_data_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_translate_device_data_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(aclsan_translate_device_data_test PRIVATE npu_compute_integration_headers)

  add_executable(cce_instr_header_contract_test tests/cce_instr_header_contract_test.cpp)
  target_compile_features(cce_instr_header_contract_test PRIVATE cxx_std_17)
  target_compile_options(cce_instr_header_contract_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(cce_instr_header_contract_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME cce_instr_header_contract_test COMMAND cce_instr_header_contract_test)

  add_executable(cce_instruction_pipeline_test
    tests/cce_instruction_pipeline_test.cpp
    src/cce_instr_types.cpp
  )
  target_compile_features(cce_instruction_pipeline_test PRIVATE cxx_std_17)
  target_compile_options(cce_instruction_pipeline_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(cce_instruction_pipeline_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME cce_instruction_pipeline_test COMMAND cce_instruction_pipeline_test)

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
