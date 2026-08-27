if(NOT DEFINED GEN_CTRLBIN OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "GEN_CTRLBIN and OUTPUT are required")
endif()

execute_process(
    COMMAND "${GEN_CTRLBIN}" "${OUTPUT}"
    RESULT_VARIABLE generate_result
    OUTPUT_VARIABLE generate_output
    ERROR_VARIABLE generate_error
)
if(NOT generate_result EQUAL 0)
    message(FATAL_ERROR "gen_ctrlbin failed (${generate_result}): ${generate_error}")
endif()

string(FIND "${generate_output}" "wrote 76 bindings" summary_position)
if(summary_position EQUAL -1)
    message(FATAL_ERROR "gen_ctrlbin did not report 76 bindings")
endif()

file(SHA256 "${OUTPUT}" actual_sha256)
set(expected_sha256 "b1f06d029c0c6ce322047ab6347eef4cdd642edb2020c6b2028f487c0444bc2c")
if(NOT actual_sha256 STREQUAL expected_sha256)
    message(FATAL_ERROR "ctrl.bin compatibility hash changed: ${actual_sha256}")
endif()
