# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").

foreach(required IN ITEMS SAMPLE FAKE_TOOL SOURCE_ROOT TEST_ROOT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${TEST_ROOT}")
set(tool_bin "${TEST_ROOT}/toolchain/tools/bisheng_compiler/bin")
set(staged_sources "${TEST_ROOT}/sources")
file(MAKE_DIRECTORY
  "${tool_bin}"
  "${TEST_ROOT}/toolchain/x86_64-linux/asc/include"
  "${TEST_ROOT}/toolchain/x86_64-linux/ascendc/include/highlevel_api/kernel_tiling"
  "${staged_sources}/probes"
)

get_filename_component(fake_tool_name "${FAKE_TOOL}" NAME)
foreach(tool IN ITEMS bisheng bisheng-tune ld.lld llvm-objdump)
  file(COPY "${FAKE_TOOL}" DESTINATION "${tool_bin}"
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  file(RENAME "${tool_bin}/${fake_tool_name}" "${tool_bin}/${tool}")
endforeach()

file(WRITE "${TEST_ROOT}/toolchain/x86_64-linux/asc/include/kernel_operator.h" "// marker\n")
file(WRITE
  "${TEST_ROOT}/toolchain/x86_64-linux/ascendc/include/highlevel_api/kernel_tiling/kernel_tiling.h"
  "// marker\n")
foreach(probe IN ITEMS fixpipe mte1 mte2 mte3 register sync)
  file(COPY "${SOURCE_ROOT}/src/probes/${probe}.cpp" DESTINATION "${staged_sources}/probes")
endforeach()
foreach(header IN ITEMS trace_record trace_buffer_abi)
  file(COPY "${SOURCE_ROOT}/include/${header}.h" DESTINATION "${staged_sources}")
endforeach()
file(WRITE "${TEST_ROOT}/input.o" "host-full-flow-kernel\n")
file(WRITE "${TEST_ROOT}/commands.log" "")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "DBI_FAKE_LOG=${TEST_ROOT}/commands.log"
    "NPU_CHECK_DBI_ARCH=dav-c220"
    "NPU_CHECK_DBI_ARG_SIZE=16"
    "NPU_CHECK_DBI_PROBE_SET=mte2,sync"
    "NPU_CHECK_DBI_TOOLCHAIN_ROOT=${TEST_ROOT}/toolchain"
    "NPU_CHECK_DBI_SOURCE_ROOT=${staged_sources}"
    "NPU_CHECK_DBI_WORK_DIR=${TEST_ROOT}/work"
    "NPU_CHECK_DBI_CACHE_DIR=${TEST_ROOT}/cache"
    "NPU_CHECK_DBI_STRICT=1"
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
  "bisheng <-xcce>"
  "ld.lld <-r>"
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

file(GLOB probe_objects "${TEST_ROOT}/cache/*/probe.o")
file(GLOB control_files "${TEST_ROOT}/cache/*/ctrl.bin")
list(LENGTH probe_objects probe_count)
list(LENGTH control_files control_count)
if(NOT probe_count EQUAL 1 OR NOT control_count EQUAL 1)
  message(FATAL_ERROR "host flow did not publish exactly one probe.o and ctrl.bin")
endif()
file(SIZE "${probe_objects}" probe_size)
file(SIZE "${control_files}" control_size)
if(probe_size EQUAL 0 OR control_size EQUAL 0)
  message(FATAL_ERROR "host flow published an empty probe.o or ctrl.bin")
endif()

message(STATUS "host full-flow sample output and DBI commands verified")
