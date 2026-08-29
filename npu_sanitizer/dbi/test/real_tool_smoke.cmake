if(NOT EXISTS "${INPUT_KERNEL}")
  message(FATAL_ERROR "real DBI smoke input does not exist: ${INPUT_KERNEL}")
endif()

file(REMOVE_RECURSE "${WORK_DIRECTORY}" "${CACHE_DIRECTORY}")
file(MAKE_DIRECTORY "${WORK_DIRECTORY}" "${CACHE_DIRECTORY}")
execute_process(
  COMMAND "${SMOKE_EXECUTABLE}"
    "${INPUT_KERNEL}"
    "${OUTPUT_KERNEL}"
    "${ARCH}"
    "${TOOLCHAIN_ROOT}"
    "${SOURCE_ROOT}"
    "${WORK_DIRECTORY}"
    "${CACHE_DIRECTORY}"
  RESULT_VARIABLE smoke_status
  OUTPUT_VARIABLE smoke_output
  ERROR_VARIABLE smoke_error
)
if(NOT smoke_status EQUAL 0)
  message(FATAL_ERROR "real DBI smoke failed (${smoke_status}):\n${smoke_output}${smoke_error}")
endif()

if(NOT EXISTS "${OUTPUT_KERNEL}")
  message(FATAL_ERROR "real DBI smoke did not create ${OUTPUT_KERNEL}")
endif()
file(SIZE "${OUTPUT_KERNEL}" output_size)
if(output_size EQUAL 0)
  message(FATAL_ERROR "real DBI smoke created an empty output")
endif()
file(GLOB probe_objects "${CACHE_DIRECTORY}/*/probe.o")
file(GLOB control_files "${CACHE_DIRECTORY}/*/ctrl.bin")
list(LENGTH probe_objects probe_count)
list(LENGTH control_files control_count)
if(NOT probe_count EQUAL 1 OR NOT control_count EQUAL 1)
  message(FATAL_ERROR "real DBI smoke did not publish exactly one probe.o and ctrl.bin")
endif()
file(SIZE "${probe_objects}" probe_size)
file(SIZE "${control_files}" control_size)
if(probe_size EQUAL 0 OR control_size EQUAL 0)
  message(FATAL_ERROR "real DBI smoke published an empty probe.o or ctrl.bin")
endif()
message(STATUS "real DBI smoke output: ${OUTPUT_KERNEL} (${output_size} bytes)")
