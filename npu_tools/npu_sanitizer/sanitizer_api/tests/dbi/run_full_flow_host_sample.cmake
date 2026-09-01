# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").

foreach(required IN ITEMS SAMPLE FAKE_TOOL TEST_ROOT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${TEST_ROOT}")
get_filename_component(sample_dir "${SAMPLE}" DIRECTORY)
get_filename_component(npu_sanitizer_build_dir "${sample_dir}" DIRECTORY)
get_filename_component(runtime_build_root "${npu_sanitizer_build_dir}" DIRECTORY)
set(tool_bin "${runtime_build_root}/tools/bisheng_compiler/bin")
file(MAKE_DIRECTORY
  "${tool_bin}"
  "${runtime_build_root}/x86_64-linux/asc/include"
  "${runtime_build_root}/x86_64-linux/asc/include/basic_api"
  "${runtime_build_root}/x86_64-linux/ascendc/include/highlevel_api"
)
file(WRITE "${runtime_build_root}/x86_64-linux/asc/include/kernel_operator.h" "// test marker\n")

get_filename_component(fake_tool_name "${FAKE_TOOL}" NAME)
foreach(tool IN ITEMS bisheng bisheng-tune ld.lld llvm-objdump)
  file(COPY "${FAKE_TOOL}" DESTINATION "${tool_bin}"
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  file(RENAME "${tool_bin}/${fake_tool_name}" "${tool_bin}/${tool}")
endforeach()

file(WRITE "${TEST_ROOT}/input.o" "host-full-flow-kernel\n")
file(WRITE "${TEST_ROOT}/commands.log" "")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "DBI_FAKE_LOG=${TEST_ROOT}/commands.log"
    "NPU_CHECK_TRACE_RECORDS_PER_BLOCK=2"
    "${SAMPLE}" "${TEST_ROOT}/input.o"
  RESULT_VARIABLE sample_result
  OUTPUT_VARIABLE sample_stdout
  ERROR_VARIABLE sample_stderr
)
set(sample_output "${sample_stdout}${sample_stderr}")

if(NOT sample_result EQUAL 0)
  message(FATAL_ERROR "host full-flow sample failed (${sample_result}):\n${sample_output}")
endif()

set(required_output
  "[intercept] binary_load"
  "[dbi] patched=yes backend=simulated"
  "[hook] function instrumented=yes"
  "[hook] launch trace_buffer_injected=yes"
  "[device] records=2"
  "[d2h] copies=1"
  "[callback] records=2"
  "[verify] kernel_result=simulated trace_records=pass resources=balanced"
  "FULL_FLOW_SAMPLE_PASS"
)
foreach(fragment IN LISTS required_output)
  string(FIND "${sample_output}" "${fragment}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "missing sample output fragment '${fragment}':\n${sample_output}")
  endif()
endforeach()

file(READ "${TEST_ROOT}/commands.log" tool_log)
set(required_tools
  "llvm-objdump <--syms>"
  "<-execute-probe>"
  "bisheng-tune <--action=instru-probe>"
)
foreach(fragment IN LISTS required_tools)
  string(FIND "${tool_log}" "${fragment}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "missing DBI tool invocation '${fragment}':\n${tool_log}")
  endif()
endforeach()

string(FIND "${tool_log}" "--tune-argsize=" tune_argsize_position)
if(tune_argsize_position EQUAL -1)
  message(FATAL_ERROR "DBI tool invocation omitted tune argument size:\n${tool_log}")
endif()

message(STATUS "host full-flow sample output and DBI commands verified")
