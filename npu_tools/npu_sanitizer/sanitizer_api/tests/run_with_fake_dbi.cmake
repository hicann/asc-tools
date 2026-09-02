# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").

foreach(required IN ITEMS TEST_EXECUTABLE FAKE_TOOL TEST_ROOT INJECTION_LIBRARY_DIR)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

file(REMOVE_RECURSE "${TEST_ROOT}")
set(tool_bin "${TEST_ROOT}/toolchain/tools/bisheng_compiler/bin")
file(MAKE_DIRECTORY "${tool_bin}")

get_filename_component(fake_tool_name "${FAKE_TOOL}" NAME)
foreach(tool IN ITEMS bisheng bisheng-tune ld.lld llvm-objdump)
  file(COPY "${FAKE_TOOL}" DESTINATION "${tool_bin}"
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
  file(RENAME "${tool_bin}/${fake_tool_name}" "${tool_bin}/${tool}")
endforeach()

file(WRITE "${TEST_ROOT}/commands.log" "")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "DBI_FAKE_LOG=${TEST_ROOT}/commands.log"
    "NPU_CHECK_DBI_ARCH=dav-3510"
    "NPU_CHECK_DBI_TOOLCHAIN_ROOT=${TEST_ROOT}/toolchain"
    "NPU_CHECK_DBI_WORK_DIR=${TEST_ROOT}/work"
    "NPU_CHECK_DBI_CACHE_DIR=${TEST_ROOT}/cache"
    "NPU_CHECK_DBI_STRICT=1"
    "NPU_CHECK_TRACE_RECORDS_PER_BLOCK=2"
    "LD_LIBRARY_PATH=${INJECTION_LIBRARY_DIR}:$ENV{LD_LIBRARY_PATH}"
    "${TEST_EXECUTABLE}"
  RESULT_VARIABLE test_result
  OUTPUT_VARIABLE test_stdout
  ERROR_VARIABLE test_stderr
)

if(NOT test_result EQUAL 0)
  message(FATAL_ERROR "${TEST_EXECUTABLE} failed (${test_result}):\n${test_stdout}${test_stderr}")
endif()

file(READ "${TEST_ROOT}/commands.log" tool_log)
string(FIND "${tool_log}" "bisheng-tune <--action=instru-probe>" instrumentation_position)
if(instrumentation_position EQUAL -1)
  message(FATAL_ERROR "${TEST_EXECUTABLE} did not execute the fake DBI instrumentation flow:\n${tool_log}")
endif()
