# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").

foreach(required IN ITEMS SAMPLE KERNEL DEVICE_ID ARCH TOOLCHAIN_ROOT SOURCE_ROOT TEST_ROOT)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()
if(NOT EXISTS "${KERNEL}")
  message(FATAL_ERROR "real full-flow kernel does not exist: ${KERNEL}")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
file(MAKE_DIRECTORY "${TEST_ROOT}/work" "${TEST_ROOT}/cache")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "NPU_CHECK_DBI_ARCH=${ARCH}"
    "NPU_CHECK_DBI_ARG_SIZE=24"
    "NPU_CHECK_DBI_PROBE_SET=mte2,sync"
    "NPU_CHECK_DBI_TOOLCHAIN_ROOT=${TOOLCHAIN_ROOT}"
    "NPU_CHECK_DBI_SOURCE_ROOT=${SOURCE_ROOT}"
    "NPU_CHECK_DBI_WORK_DIR=${TEST_ROOT}/work"
    "NPU_CHECK_DBI_CACHE_DIR=${TEST_ROOT}/cache"
    "NPU_CHECK_DBI_STRICT=1"
    "NPU_CHECK_TRACE_RECORDS_PER_BLOCK=16"
    "${SAMPLE}" "${KERNEL}" "FullFlowKernel" "${DEVICE_ID}"
  RESULT_VARIABLE sample_result
  OUTPUT_VARIABLE sample_stdout
  ERROR_VARIABLE sample_stderr
)
set(sample_output "${sample_stdout}${sample_stderr}")
if(NOT sample_result EQUAL 0)
  message(FATAL_ERROR "real full-flow sample failed (${sample_result}):\n${sample_output}")
endif()

set(required_output
  "[intercept] binary_load"
  "[dbi] patched=yes backend=real"
  "[hook] function instrumented=yes"
  "[hook] launch trace_buffer_injected=yes"
  "[device] records="
  "[d2h] copies=1"
  "[verify] kernel_result=pass trace_records=pass resources=balanced"
  "FULL_FLOW_SAMPLE_PASS"
)
foreach(fragment IN LISTS required_output)
  string(FIND "${sample_output}" "${fragment}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "missing real sample output fragment '${fragment}':\n${sample_output}")
  endif()
endforeach()
string(REGEX MATCH "\\[callback\\] records=[1-9][0-9]*" callback_count "${sample_output}")
if(callback_count STREQUAL "")
  message(FATAL_ERROR "real sample did not report a positive callback count:\n${sample_output}")
endif()

file(GLOB probe_objects "${TEST_ROOT}/cache/*/probe.o")
file(GLOB control_files "${TEST_ROOT}/cache/*/ctrl.bin")
list(LENGTH probe_objects probe_count)
list(LENGTH control_files control_count)
if(NOT probe_count EQUAL 1 OR NOT control_count EQUAL 1)
  message(FATAL_ERROR "real flow did not publish exactly one probe.o and ctrl.bin")
endif()
file(SIZE "${probe_objects}" probe_size)
file(SIZE "${control_files}" control_size)
if(probe_size EQUAL 0 OR control_size EQUAL 0)
  message(FATAL_ERROR "real flow published an empty probe.o or ctrl.bin")
endif()

message(STATUS "real NPU full-flow output verified")
