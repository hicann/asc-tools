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

set(NPU_CHECK_EMBED_PROBE_RESOURCES_SCRIPT_CONTENT [=[# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.

cmake_policy(SET CMP0054 NEW)

foreach(required IN ITEMS
    TRACE_RECORD TRACE_BUFFER_ABI CTRLBIN_SOURCE CTRLBIN_HEADER MSBIT_SOURCE MSBIT_HEADER
    MSBIT_FILESYSTEM_HEADER MSBIT_SERIALIZE_HEADER MSBIT_SINGLETON_HEADER MSBIT_USTRING_HEADER OUTPUT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
  if(NOT "${required}" STREQUAL "OUTPUT" AND NOT EXISTS "${${required}}")
    message(FATAL_ERROR "missing Probe resource: ${${required}}")
  endif()
endforeach()

file(READ "${TRACE_RECORD}" trace_record_hex HEX)
file(READ "${TRACE_BUFFER_ABI}" trace_buffer_abi_hex HEX)
file(SHA256 "${TRACE_RECORD}" trace_record_sha)
file(SHA256 "${TRACE_BUFFER_ABI}" trace_buffer_abi_sha)
string(SHA256 resource_identity "trace_record.h:${trace_record_sha}\ntrace_buffer_abi.h:${trace_buffer_abi_sha}\n")
set(ctrlbin_identity_input "")
foreach(ctrlbin_input IN ITEMS
    CTRLBIN_SOURCE CTRLBIN_HEADER MSBIT_SOURCE MSBIT_HEADER MSBIT_FILESYSTEM_HEADER
    MSBIT_SERIALIZE_HEADER MSBIT_SINGLETON_HEADER MSBIT_USTRING_HEADER)
  file(SHA256 "${${ctrlbin_input}}" ctrlbin_input_sha)
  get_filename_component(ctrlbin_input_name "${${ctrlbin_input}}" NAME)
  string(APPEND ctrlbin_identity_input "${ctrlbin_input_name}:${ctrlbin_input_sha}\n")
endforeach()
string(SHA256 ctrlbin_implementation_identity "${ctrlbin_identity_input}")

string(LENGTH "${trace_record_hex}" trace_record_hex_length)
string(LENGTH "${trace_buffer_abi_hex}" trace_buffer_abi_hex_length)
math(EXPR trace_record_size "${trace_record_hex_length} / 2")
math(EXPR trace_buffer_abi_size "${trace_buffer_abi_hex_length} / 2")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," trace_record_bytes "${trace_record_hex}")
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," trace_buffer_abi_bytes "${trace_buffer_abi_hex}")

set(content "// Generated file. Do not edit.\n")
string(APPEND content "#include \"dbi/embedded_probe_resources.h\"\n\n")
string(APPEND content "#include <cstddef>\n\n")
string(APPEND content "namespace aclsan {\nnamespace {\n")
string(APPEND content "const unsigned char kTraceRecord[] = {${trace_record_bytes}};\n")
string(APPEND content "const unsigned char kTraceBufferAbi[] = {${trace_buffer_abi_bytes}};\n")
string(APPEND content "constexpr std::size_t kTraceRecordSize = ${trace_record_size}U;\n")
string(APPEND content "constexpr std::size_t kTraceBufferAbiSize = ${trace_buffer_abi_size}U;\n")
string(APPEND content "constexpr std::string_view kResourceIdentity = \"${resource_identity}\";\n")
string(APPEND content
  "constexpr std::string_view kCtrlBinImplementationIdentity = \"${ctrlbin_implementation_identity}\";\n")
string(APPEND content "} // namespace\n\n")
string(APPEND content "std::string_view EmbeddedTraceRecordHeader()\n{\n")
string(APPEND content "    return {reinterpret_cast<const char*>(kTraceRecord), kTraceRecordSize};\n}\n\n")
string(APPEND content "std::string_view EmbeddedTraceBufferAbiHeader()\n{\n")
string(APPEND content "    return {reinterpret_cast<const char*>(kTraceBufferAbi), kTraceBufferAbiSize};\n}\n\n")
string(APPEND content "std::string_view EmbeddedProbeResourceIdentity() { return kResourceIdentity; }\n\n")
string(APPEND content
  "std::string_view EmbeddedCtrlBinImplementationIdentity() { return kCtrlBinImplementationIdentity; }\n\n")
string(APPEND content "} // namespace aclsan\n")

file(WRITE "${OUTPUT}" "${content}")
]=])
set(NPU_CHECK_EMBED_PROBE_RESOURCES_SCRIPT
  "${CMAKE_CURRENT_BINARY_DIR}/generated/embed_probe_resources.cmake")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(GENERATE
  OUTPUT "${NPU_CHECK_EMBED_PROBE_RESOURCES_SCRIPT}"
  CONTENT "${NPU_CHECK_EMBED_PROBE_RESOURCES_SCRIPT_CONTENT}")

find_package(Threads REQUIRED)

set(NPU_CHECK_PROBE_ARCHES "dav-3510" CACHE STRING
  "Public NPU architecture names supported by the runtime Probe generator")

add_library(npu_check_dbi_probe_group STATIC src/dbi/probe_group.cpp)
target_compile_features(npu_check_dbi_probe_group PUBLIC cxx_std_17)
target_include_directories(npu_check_dbi_probe_group PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(npu_check_dbi_probe_group PRIVATE -Wall -Wextra -Werror)
set_target_properties(npu_check_dbi_probe_group PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

set(NPU_CHECK_PROBE_RESOURCE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_probe_resources.cpp")
add_custom_command(
  OUTPUT "${NPU_CHECK_PROBE_RESOURCE_SOURCE}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND "${CMAKE_COMMAND}"
    "-DTRACE_RECORD=${CMAKE_CURRENT_SOURCE_DIR}/include/dbi/trace_record.h"
    "-DTRACE_BUFFER_ABI=${CMAKE_CURRENT_SOURCE_DIR}/include/dbi/trace_buffer_abi.h"
    "-DCTRLBIN_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/dynamic_bind.cpp"
    "-DCTRLBIN_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/dbi/ctrlbin_generator.h"
    "-DMSBIT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/MSBit.cpp"
    "-DMSBIT_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/MSBit.h"
    "-DMSBIT_FILESYSTEM_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/FileSystem.h"
    "-DMSBIT_SERIALIZE_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/Serialize.h"
    "-DMSBIT_SINGLETON_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/Singleton.h"
    "-DMSBIT_USTRING_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/Ustring.h"
    "-DOUTPUT=${NPU_CHECK_PROBE_RESOURCE_SOURCE}"
    -P "${NPU_CHECK_EMBED_PROBE_RESOURCES_SCRIPT}"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/dbi/trace_record.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/dbi/trace_buffer_abi.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/dynamic_bind.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/dbi/ctrlbin_generator.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/MSBit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/MSBit.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/FileSystem.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/Serialize.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/Singleton.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit/Ustring.h"
    "${NPU_CHECK_EMBED_PROBE_RESOURCES_SCRIPT}"
  VERBATIM)

add_library(npu_check_dbi_probe_runtime STATIC
  src/dbi/probe_source_generator.cpp
  "${NPU_CHECK_PROBE_RESOURCE_SOURCE}"
)
target_compile_features(npu_check_dbi_probe_runtime PUBLIC cxx_std_17)
target_include_directories(npu_check_dbi_probe_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(npu_check_dbi_probe_runtime PRIVATE -Wall -Wextra -Werror)
target_link_libraries(npu_check_dbi_probe_runtime PUBLIC npu_check_dbi_probe_group)
set_target_properties(npu_check_dbi_probe_runtime PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

add_library(npu_check_dbi_ctrlbin STATIC
  src/dbi/dynamic_bind.cpp
  src/dbi/msbit/MSBit.cpp
)
target_compile_features(npu_check_dbi_ctrlbin PUBLIC cxx_std_17)
target_include_directories(npu_check_dbi_ctrlbin
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/dbi/msbit
)
target_compile_options(npu_check_dbi_ctrlbin PRIVATE -Wall -Wextra -Werror)
target_link_libraries(npu_check_dbi_ctrlbin PUBLIC npu_check_dbi_probe_group)
set_source_files_properties(src/dbi/msbit/MSBit.cpp PROPERTIES COMPILE_OPTIONS -Wno-missing-field-initializers)
set_target_properties(npu_check_dbi_ctrlbin PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

add_library(npu_check_dbi_engine STATIC
  src/dbi/binary_instrumenter.cpp
  src/dbi/dbi_pipeline.cpp
  src/dbi/tool_runner.cpp
)
target_compile_features(npu_check_dbi_engine PUBLIC cxx_std_17)
target_include_directories(npu_check_dbi_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(npu_check_dbi_engine PRIVATE -Wall -Wextra -Werror)
target_link_libraries(npu_check_dbi_engine
  PUBLIC npu_check_dbi_ctrlbin
  PRIVATE npu_check_dbi_probe_runtime ${CMAKE_DL_LIBS} Threads::Threads Boost::filesystem
)
set_target_properties(npu_check_dbi_engine PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

if(NOT TARGET injection_headers OR NOT TARGET acl_tool_injection)
  message(FATAL_ERROR
    "sanitizer_api requires Runtime injection targets: injection_headers and acl_tool_injection")
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
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(acl_san
  PRIVATE
    injection_headers
    acl_tool_injection
    npu_check_dbi_engine
    ${CMAKE_DL_LIBS}
    Threads::Threads
    Boost::filesystem
)
target_link_options(acl_san PRIVATE "LINKER:--exclude-libs,ALL")

set(ACLSAN_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/acl_san.map")
if(ENABLE_TEST)
  set(ACLSAN_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/acl_san_test.map")
endif()
target_link_options(acl_san PRIVATE
  "LINKER:--version-script=${ACLSAN_VERSION_SCRIPT}"
  "LINKER:-rpath,$ORIGIN"
)
set_property(TARGET acl_san APPEND PROPERTY
  LINK_DEPENDS "${ACLSAN_VERSION_SCRIPT}"
)

set_target_properties(acl_san PROPERTIES
  OUTPUT_NAME acl_san
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  BUILD_WITH_INSTALL_RPATH YES
  INSTALL_RPATH "$ORIGIN"
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(aclsan_callback_invoke_test PRIVATE injection_headers)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(aclsan_probe_plan_callback_test PRIVATE
    injection_headers
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(aclsan_hook_cbdata_test PRIVATE
    injection_headers
    npu_check_dbi_engine
    ${CMAKE_DL_LIBS}
    Threads::Threads
    Boost::filesystem)
  add_test(NAME aclsan_hook_cbdata_test COMMAND aclsan_hook_cbdata_test)

  add_executable(aclsan_device_symbolizer_test
    tests/device_symbolizer_test.cpp
    src/device_runtime/device_symbolizer.cpp
  )
  target_compile_features(aclsan_device_symbolizer_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_device_symbolizer_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_device_symbolizer_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/device_runtime)
  target_link_libraries(aclsan_device_symbolizer_test PRIVATE Boost::filesystem)
  add_test(NAME aclsan_device_symbolizer_test COMMAND aclsan_device_symbolizer_test)

  add_executable(aclsan_device_binary_registry_test
    tests/device_binary_registry_test.cpp
    src/device_runtime/device_binary_registry.cpp
    src/device_runtime/device_symbolizer.cpp
  )
  target_compile_features(aclsan_device_binary_registry_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_device_binary_registry_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_device_binary_registry_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(aclsan_device_binary_registry_test PRIVATE Boost::filesystem)
  add_test(NAME aclsan_device_binary_registry_test COMMAND aclsan_device_binary_registry_test)

  add_executable(aclsan_memory_cbdata_test
    tests/aclsan_memory_cbdata_test.cpp
    src/aclsan_memory_cbdata.cpp
  )
  target_compile_features(aclsan_memory_cbdata_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_memory_cbdata_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_memory_cbdata_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(aclsan_memory_cbdata_test PRIVATE injection_headers)
  add_test(NAME aclsan_memory_cbdata_test COMMAND aclsan_memory_cbdata_test)
  add_executable(aclsan_trace_buffer_test
    tests/aclsan_trace_buffer.cpp
    src/aclsan_trace_buffer.cpp
  )
  target_compile_features(aclsan_trace_buffer_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_trace_buffer_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_trace_buffer_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(aclsan_translate_device_data_test PRIVATE injection_headers)
  add_test(NAME aclsan_translate_device_data_test COMMAND aclsan_translate_device_data_test)

  add_executable(cce_instr_header_contract_test tests/cce_instr_header_contract_test.cpp)
  target_compile_features(cce_instr_header_contract_test PRIVATE cxx_std_17)
  target_compile_options(cce_instr_header_contract_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(cce_instr_header_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME device_instr_decoder_test COMMAND device_instr_decoder_test)

  add_executable(register_state_manager_test
    tests/register_state_manager_test.cpp
    src/device_instr/arch/dav_3510/register_state_manager.cpp
  )
  target_compile_features(register_state_manager_test PRIVATE cxx_std_17)
  target_compile_options(register_state_manager_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(register_state_manager_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME register_state_manager_test COMMAND register_state_manager_test)

  add_executable(aclsan_lifecycle_api_removed tests/aclsan_lifecycle_api_removed.cpp)
  target_compile_features(aclsan_lifecycle_api_removed PRIVATE cxx_std_17)
  target_link_libraries(aclsan_lifecycle_api_removed PRIVATE ${CMAKE_DL_LIBS})
  add_dependencies(aclsan_lifecycle_api_removed acl_san)

  add_executable(aclsan_symbolizer_api_test tests/aclsan_symbolizer_api_test.cpp)
  target_compile_features(aclsan_symbolizer_api_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_symbolizer_api_test PRIVATE
    -Wall -Wextra -Werror -Wno-error=cast-function-type)
  target_include_directories(aclsan_symbolizer_api_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/dbi)
  target_link_libraries(aclsan_symbolizer_api_test PRIVATE
    acl_san acl_tool_injection acl_runtime_stub Boost::filesystem)
  add_test(NAME aclsan_symbolizer_api_test COMMAND aclsan_symbolizer_api_test)
  set_tests_properties(aclsan_symbolizer_api_test PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:acl_tool_injection>")

  if(TARGET acl_runtime_stub)
    add_executable(aclsan_trace_d2h_hook tests/aclsan_trace_d2h_hook.cpp)
    target_compile_features(aclsan_trace_d2h_hook PRIVATE cxx_std_17)
    target_include_directories(aclsan_trace_d2h_hook PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/dbi)
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
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/dbi/full_flow_host_sample.cpp)
    target_compile_features(aclsan_full_flow_host_sample PRIVATE cxx_std_17)
    target_compile_options(aclsan_full_flow_host_sample PRIVATE -Wno-error=cast-function-type)
    target_include_directories(aclsan_full_flow_host_sample PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(aclsan_full_flow_host_sample PRIVATE acl_san acl_tool_injection acl_runtime_stub)

    add_executable(aclsan_full_flow_fake_dbi_tool
      ${CMAKE_CURRENT_SOURCE_DIR}/tests/dbi/fake_dbi_tool.cpp)
    target_compile_features(aclsan_full_flow_fake_dbi_tool PRIVATE cxx_std_17)
    target_compile_options(aclsan_full_flow_fake_dbi_tool PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(aclsan_full_flow_fake_dbi_tool PRIVATE Boost::filesystem)
    add_test(NAME aclsan.full_flow_host_sample
      COMMAND ${CMAKE_COMMAND}
        -DSAMPLE=$<TARGET_FILE:aclsan_full_flow_host_sample>
        -DFAKE_TOOL=$<TARGET_FILE:aclsan_full_flow_fake_dbi_tool>
        -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/full-flow-host
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/dbi/run_full_flow_host_sample.cmake)
  endif()
endif()
if(ENABLE_TEST OR NPU_CHECK_ENABLE_REAL_DBI_FLOW OR NPU_CHECK_ENABLE_REAL_DBI_TOOLCHAIN)
  add_subdirectory(tests/dbi)
endif()
