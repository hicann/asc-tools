set(SANITIZER_API_SOURCE_DIR "${CMAKE_SOURCE_DIR}/npu_tools/npu_check/src/acl_san")
set(SANITIZER_API_TEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(ENABLE_TEST)
  add_test(NAME aclsan.public_headers
    COMMAND bash "${SANITIZER_API_TEST_DIR}/check_public_headers.sh"
      "${SANITIZER_API_SOURCE_DIR}/../../include"
      "${CMAKE_C_COMPILER}" "${CMAKE_CXX_COMPILER}")
  add_executable(aclsan_kernel_param_metadata_test dbi/kernel_param_metadata_test.cpp)
  target_compile_features(aclsan_kernel_param_metadata_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_kernel_param_metadata_test PRIVATE -Wall -Wextra -Werror -UNDEBUG)
  target_link_libraries(aclsan_kernel_param_metadata_test PRIVATE npu_check_dbi_engine)
  add_test(NAME aclsan_kernel_param_metadata_test COMMAND aclsan_kernel_param_metadata_test)
  add_test(NAME acl_san_exports
    COMMAND bash "${SANITIZER_API_TEST_DIR}/check_acl_san_exports.sh"
      $<TARGET_FILE:acl_san>)
  add_test(NAME aclsan.host_test_linkage_contract
    COMMAND bash "${SANITIZER_API_TEST_DIR}/check_host_test_linkage_contract.sh")
  add_test(NAME aclsan.probe_contract
    COMMAND bash "${SANITIZER_API_TEST_DIR}/check_probe_contract.sh")
  add_test(NAME aclsan.device_memory_access_producer_contract
    COMMAND bash "${SANITIZER_API_TEST_DIR}/check_device_memory_access_producer_contract.sh")

  add_library(acl_san_test SHARED
    ${SANITIZER_API_SOURCE_DIR}/device_runtime/device_binary_registry.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_runtime/device_symbolizer.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/decoder_registry.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/arch/dav_3510/decoder.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/arch/dav_3510/register_state_manager.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_active_probe_plan.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_memory_cbdata.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_trace_buffer.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_trace_runtime.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_api.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_dispatch.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_hook_aclrt.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_device_data_log.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_translate_device_data.cpp
  )
  target_compile_features(acl_san_test PUBLIC cxx_std_17)
  target_include_directories(acl_san_test
    PUBLIC ${SANITIZER_API_SOURCE_DIR}/../../include
    PRIVATE ${SANITIZER_API_SOURCE_DIR}
  )
  target_link_libraries(acl_san_test PRIVATE
    acl_tool_injection_test
    npu_check_dbi_engine
    npu_sanitizer_plog
    ${CMAKE_DL_LIBS}
    Threads::Threads
    Boost::filesystem
  )
  target_link_options(acl_san_test PRIVATE
    "LINKER:--exclude-libs,ALL"
    "LINKER:--version-script=${SANITIZER_API_SOURCE_DIR}/cmake/acl_san.map"
  )
  set_target_properties(acl_san_test PROPERTIES
    OUTPUT_NAME acl_san_test
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
  )
  injection_set_test_output(acl_san_test)

  add_executable(aclsan_callback_contract ${SANITIZER_API_TEST_DIR}/aclsan_callback_contract.cpp)
  target_compile_features(aclsan_callback_contract PRIVATE cxx_std_17)
  target_link_libraries(aclsan_callback_contract PRIVATE acl_san_test)

  add_executable(aclsan_log_test ${SANITIZER_API_TEST_DIR}/aclsan_log_test.cpp)
  target_compile_features(aclsan_log_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_log_test PRIVATE -Wall -Wextra -Werror)
  target_link_libraries(aclsan_log_test PRIVATE plog_test_library)
  target_include_directories(aclsan_log_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/../../include
    ${SANITIZER_API_SOURCE_DIR}
  )
  target_link_libraries(aclsan_log_test PRIVATE npu_sanitizer_plog acl_rt_headers)
  add_test(NAME aclsan_log_test COMMAND aclsan_log_test)

  add_executable(aclsan_callback_invoke_test
    ${SANITIZER_API_TEST_DIR}/aclsan_callback_invoke_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_active_probe_plan.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_api.cpp
  )
  target_compile_features(aclsan_callback_invoke_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_callback_invoke_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_callback_invoke_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}
    ${SANITIZER_API_SOURCE_DIR}/../../include
  )
  target_link_libraries(aclsan_callback_invoke_test PRIVATE injection_headers npu_sanitizer_plog)
  add_test(NAME aclsan_callback_invoke_test COMMAND aclsan_callback_invoke_test)

  add_executable(aclsan_probe_plan_callback_test
    ${SANITIZER_API_TEST_DIR}/aclsan_probe_plan_callback_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_active_probe_plan.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_api.cpp
  )
  target_compile_features(aclsan_probe_plan_callback_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_probe_plan_callback_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_probe_plan_callback_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}
    ${SANITIZER_API_SOURCE_DIR}/../../include
  )
  target_link_libraries(aclsan_probe_plan_callback_test PRIVATE
    injection_headers
    npu_sanitizer_plog
    npu_check_dbi_engine
    Threads::Threads)
  add_test(NAME aclsan_probe_plan_callback_test COMMAND aclsan_probe_plan_callback_test)

  add_executable(aclsan_hook_cbdata_test
    ${SANITIZER_API_TEST_DIR}/aclsan_hook_cbdata_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_active_probe_plan.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_trace_buffer.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_trace_runtime.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_runtime/device_binary_registry.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_runtime/device_symbolizer.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_device_data_log.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_translate_device_data.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_memory_cbdata.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/decoder_registry.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/arch/dav_3510/decoder.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/arch/dav_3510/register_state_manager.cpp
  )
  target_compile_features(aclsan_hook_cbdata_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_hook_cbdata_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_hook_cbdata_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}
    ${SANITIZER_API_SOURCE_DIR}/../../include
  )
  target_link_libraries(aclsan_hook_cbdata_test PRIVATE
    injection_headers
    npu_sanitizer_plog
    npu_check_dbi_engine
    ${CMAKE_DL_LIBS}
    Threads::Threads
    Boost::filesystem)
  add_test(NAME aclsan_hook_cbdata_test COMMAND aclsan_hook_cbdata_test)

  add_executable(aclsan_device_symbolizer_test
    ${SANITIZER_API_TEST_DIR}/device_symbolizer_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_runtime/device_symbolizer.cpp
  )
  target_compile_features(aclsan_device_symbolizer_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_device_symbolizer_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_device_symbolizer_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/device_runtime
  )
  target_link_libraries(aclsan_device_symbolizer_test PRIVATE Boost::filesystem)
  add_test(NAME aclsan_device_symbolizer_test COMMAND aclsan_device_symbolizer_test)

  add_executable(aclsan_device_binary_registry_test
    ${SANITIZER_API_TEST_DIR}/device_binary_registry_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_runtime/device_binary_registry.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_runtime/device_symbolizer.cpp
  )
  target_compile_features(aclsan_device_binary_registry_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_device_binary_registry_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_device_binary_registry_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}
  )
  target_link_libraries(aclsan_device_binary_registry_test PRIVATE Boost::filesystem)
  add_test(NAME aclsan_device_binary_registry_test COMMAND aclsan_device_binary_registry_test)

  add_executable(aclsan_memory_cbdata_test
    ${SANITIZER_API_TEST_DIR}/aclsan_memory_cbdata_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_memory_cbdata.cpp
  )
  target_compile_features(aclsan_memory_cbdata_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_memory_cbdata_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_memory_cbdata_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/../../include
    ${SANITIZER_API_SOURCE_DIR}
  )
  target_link_libraries(aclsan_memory_cbdata_test PRIVATE injection_headers)
  add_test(NAME aclsan_memory_cbdata_test COMMAND aclsan_memory_cbdata_test)
  add_executable(aclsan_trace_buffer_test
    ${SANITIZER_API_TEST_DIR}/aclsan_trace_buffer.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_trace_buffer.cpp
  )
  target_compile_features(aclsan_trace_buffer_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_trace_buffer_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_trace_buffer_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/../../include
    ${SANITIZER_API_SOURCE_DIR}
  )
  add_test(NAME aclsan_trace_buffer_test COMMAND aclsan_trace_buffer_test)

  add_executable(aclsan_translate_device_data_test
    ${SANITIZER_API_TEST_DIR}/aclsan_translate_device_data_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_device_data_log.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_translate_device_data.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_memory_cbdata.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/decoder_registry.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/arch/dav_3510/decoder.cpp
  )
  target_compile_features(aclsan_translate_device_data_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_translate_device_data_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(aclsan_translate_device_data_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/../../include
    ${SANITIZER_API_SOURCE_DIR}
  )
  target_link_libraries(aclsan_translate_device_data_test PRIVATE injection_headers npu_sanitizer_plog)
  add_test(NAME aclsan_translate_device_data_test COMMAND aclsan_translate_device_data_test)

  add_executable(cce_instr_header_contract_test ${SANITIZER_API_TEST_DIR}/cce_instr_header_contract_test.cpp)
  target_compile_features(cce_instr_header_contract_test PRIVATE cxx_std_17)
  target_compile_options(cce_instr_header_contract_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(cce_instr_header_contract_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/../../include
    ${SANITIZER_API_SOURCE_DIR}
  )
  target_link_libraries(cce_instr_header_contract_test PRIVATE npu_sanitizer_plog plog_test_library)
  add_test(NAME cce_instr_header_contract_test COMMAND cce_instr_header_contract_test)

  add_executable(cce_instruction_pipeline_test
    ${SANITIZER_API_TEST_DIR}/cce_instruction_pipeline_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_device_data_log.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_translate_device_data.cpp
    ${SANITIZER_API_SOURCE_DIR}/aclsan_memory_cbdata.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/decoder_registry.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/arch/dav_3510/decoder.cpp
  )
  target_compile_features(cce_instruction_pipeline_test PRIVATE cxx_std_17)
  target_compile_options(cce_instruction_pipeline_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(cce_instruction_pipeline_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/../../include
    ${SANITIZER_API_SOURCE_DIR}
  )
  target_link_libraries(cce_instruction_pipeline_test PRIVATE npu_sanitizer_plog)
  add_test(NAME cce_instruction_pipeline_test COMMAND cce_instruction_pipeline_test)

  add_executable(device_instr_decoder_test
    ${SANITIZER_API_TEST_DIR}/device_instr_decoder_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/decoder_registry.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/arch/dav_3510/decoder.cpp
  )
  target_compile_features(device_instr_decoder_test PRIVATE cxx_std_17)
  target_compile_options(device_instr_decoder_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(device_instr_decoder_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/../../include
    ${SANITIZER_API_SOURCE_DIR}
  )
  target_link_libraries(device_instr_decoder_test PRIVATE npu_sanitizer_plog)
  add_test(NAME device_instr_decoder_test COMMAND device_instr_decoder_test)

  add_executable(register_state_manager_test
    ${SANITIZER_API_TEST_DIR}/register_state_manager_test.cpp
    ${SANITIZER_API_SOURCE_DIR}/device_instr/arch/dav_3510/register_state_manager.cpp
  )
  target_compile_features(register_state_manager_test PRIVATE cxx_std_17)
  target_compile_options(register_state_manager_test PRIVATE -Wall -Wextra -Werror)
  target_include_directories(register_state_manager_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/../../include
    ${SANITIZER_API_SOURCE_DIR}
  )
  target_link_libraries(register_state_manager_test PRIVATE npu_sanitizer_plog)
  foreach(target IN ITEMS aclsan_hook_cbdata_test aclsan_translate_device_data_test register_state_manager_test)
    target_link_libraries(${target} PRIVATE plog_test_library)
  endforeach()
  add_test(NAME register_state_manager_test COMMAND register_state_manager_test)

  add_executable(aclsan_lifecycle_api_removed ${SANITIZER_API_TEST_DIR}/aclsan_lifecycle_api_removed.cpp)
  target_compile_features(aclsan_lifecycle_api_removed PRIVATE cxx_std_17)
  target_link_libraries(aclsan_lifecycle_api_removed PRIVATE ${CMAKE_DL_LIBS})
  add_dependencies(aclsan_lifecycle_api_removed acl_san)

  add_executable(aclsan_symbolizer_api_test ${SANITIZER_API_TEST_DIR}/aclsan_symbolizer_api_test.cpp)
  target_compile_features(aclsan_symbolizer_api_test PRIVATE cxx_std_17)
  target_compile_options(aclsan_symbolizer_api_test PRIVATE
    -Wall -Wextra -Werror -Wno-error=cast-function-type)
  target_include_directories(aclsan_symbolizer_api_test PRIVATE
    ${SANITIZER_API_SOURCE_DIR}/../../include
    ${SANITIZER_API_SOURCE_DIR}
    ${SANITIZER_API_TEST_DIR}/dbi
  )
  target_link_libraries(aclsan_symbolizer_api_test PRIVATE
    acl_san_test acl_tool_injection_test acl_runtime_stub Boost::filesystem)

  if(TARGET acl_runtime_stub)
    add_executable(aclsan_trace_d2h_hook ${SANITIZER_API_TEST_DIR}/aclsan_trace_d2h_hook.cpp)
    target_compile_features(aclsan_trace_d2h_hook PRIVATE cxx_std_17)
    target_include_directories(aclsan_trace_d2h_hook PRIVATE
      ${SANITIZER_API_SOURCE_DIR}/../../include
      ${SANITIZER_API_SOURCE_DIR}
      ${SANITIZER_API_TEST_DIR}/dbi
    )
    target_link_libraries(aclsan_trace_d2h_hook PRIVATE acl_san_test acl_tool_injection_test acl_runtime_stub)

    add_executable(aclsan_launch_with_host_args_hook ${SANITIZER_API_TEST_DIR}/aclsan_launch_with_host_args_hook.cpp)
    target_compile_features(aclsan_launch_with_host_args_hook PRIVATE cxx_std_17)
    target_link_libraries(aclsan_launch_with_host_args_hook PRIVATE acl_san_test acl_tool_injection_test acl_runtime_stub)
    add_test(NAME aclsan_launch_with_host_args_hook COMMAND aclsan_launch_with_host_args_hook)

    add_executable(aclsan_binary_load_dbi_hook ${SANITIZER_API_TEST_DIR}/aclsan_binary_load_dbi_hook.cpp)
    target_compile_features(aclsan_binary_load_dbi_hook PRIVATE cxx_std_17)
    target_link_libraries(aclsan_binary_load_dbi_hook PRIVATE acl_san_test acl_tool_injection_test acl_runtime_stub)
    add_test(NAME aclsan_binary_load_dbi_hook COMMAND aclsan_binary_load_dbi_hook)

    add_executable(aclsan_full_flow_host_sample
      ${SANITIZER_API_TEST_DIR}/dbi/full_flow_host_sample.cpp)
    target_compile_features(aclsan_full_flow_host_sample PRIVATE cxx_std_17)
    target_compile_options(aclsan_full_flow_host_sample PRIVATE -Wno-error=cast-function-type)
    target_include_directories(aclsan_full_flow_host_sample PRIVATE
      ${SANITIZER_API_SOURCE_DIR}/../../include
      ${SANITIZER_API_SOURCE_DIR}
    )
    target_link_libraries(aclsan_full_flow_host_sample PRIVATE acl_san_test acl_tool_injection_test acl_runtime_stub)

    add_executable(aclsan_full_flow_fake_dbi_tool
      ${SANITIZER_API_TEST_DIR}/dbi/fake_dbi_tool.cpp)
    target_compile_features(aclsan_full_flow_fake_dbi_tool PRIVATE cxx_std_17)
    target_compile_options(aclsan_full_flow_fake_dbi_tool PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(aclsan_full_flow_fake_dbi_tool PRIVATE Boost::filesystem)

    foreach(target IN ITEMS
        aclsan_symbolizer_api_test
        aclsan_trace_d2h_hook
        aclsan_full_flow_host_sample)
      add_dependencies(${target} aclsan_full_flow_fake_dbi_tool)
    endforeach()

    foreach(target IN ITEMS aclsan_symbolizer_api_test aclsan_trace_d2h_hook)
      add_test(NAME ${target}
        COMMAND ${CMAKE_COMMAND}
          -DTEST_EXECUTABLE=$<TARGET_FILE:${target}>
          -DFAKE_TOOL=$<TARGET_FILE:aclsan_full_flow_fake_dbi_tool>
          -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/${target}-dbi
          -DRUNTIME_ROOT=${NPU_TOOLS_TEST_CANN_ROOT}
          -DINJECTION_LIBRARY_DIR=$<TARGET_FILE_DIR:acl_san_test>
          -P ${SANITIZER_API_TEST_DIR}/run_with_fake_dbi.cmake)
    endforeach()

    add_test(NAME aclsan.full_flow_host_sample
      COMMAND ${CMAKE_COMMAND}
        -DSAMPLE=$<TARGET_FILE:aclsan_full_flow_host_sample>
        -DFAKE_TOOL=$<TARGET_FILE:aclsan_full_flow_fake_dbi_tool>
        -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/full-flow-host
        -DRUNTIME_ROOT=${NPU_TOOLS_TEST_CANN_ROOT}
        -P ${SANITIZER_API_TEST_DIR}/dbi/run_full_flow_host_sample.cmake)
  endif()
endif()
if(ENABLE_TEST OR NPU_CHECK_ENABLE_REAL_DBI_FLOW OR NPU_CHECK_ENABLE_REAL_DBI_TOOLCHAIN)
  add_subdirectory("${SANITIZER_API_TEST_DIR}/dbi" "${CMAKE_CURRENT_BINARY_DIR}/tests/dbi")
endif()
