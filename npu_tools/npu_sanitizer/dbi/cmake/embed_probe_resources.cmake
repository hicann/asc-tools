# Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
string(APPEND content "#include \"embedded_probe_resources.h\"\n\n")
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
