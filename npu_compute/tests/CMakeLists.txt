# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
set(NPU_COMPUTE_ACLPTI_TEST_BACKENDS
  acl_tool_injection
  acl_prof_api_backend
  acl_runtime_stub
)

find_package(Threads REQUIRED)

add_executable(npu_compute_import_output_directory_test import_output_directory_test.cpp)
target_link_libraries(npu_compute_import_output_directory_test PRIVATE npu_compute_launcher_core)
add_test(NAME npu_compute.import_output_directory COMMAND npu_compute_import_output_directory_test)

add_executable(npu_compute_rep_encoder_test
  rep_encoder_test.cpp
  rep_test_decoder.cpp
)
target_link_libraries(npu_compute_rep_encoder_test PRIVATE npu_compute_launcher_core)
add_test(NAME npu_compute.rep_encoder COMMAND npu_compute_rep_encoder_test)

add_executable(npu_compute_collection_file_validator_test collection_file_validator_test.cpp)
target_link_libraries(npu_compute_collection_file_validator_test PRIVATE npu_compute_launcher_core)
add_test(NAME npu_compute.collection_file_validator COMMAND npu_compute_collection_file_validator_test)

add_executable(npu_compute_rep_directory_packer_test
  rep_directory_packer_test.cpp
  rep_test_decoder.cpp
)
target_link_libraries(npu_compute_rep_directory_packer_test PRIVATE npu_compute_launcher_core)
add_test(NAME npu_compute.rep_directory_packer COMMAND npu_compute_rep_directory_packer_test)

add_executable(npu_compute_report_name_test report_name_test.cpp)
target_link_libraries(npu_compute_report_name_test PRIVATE npu_compute_launcher_core)
add_test(NAME npu_compute.report_name COMMAND npu_compute_report_name_test)

add_executable(npu_compute_rep_report_writer_test
  rep_report_writer_test.cpp
  rep_test_decoder.cpp
)
target_link_libraries(npu_compute_rep_report_writer_test PRIVATE npu_compute_launcher_core)
add_test(NAME npu_compute.rep_report_writer COMMAND npu_compute_rep_report_writer_test)

add_executable(npu_compute_cli_test cli_test.cpp)
target_link_libraries(npu_compute_cli_test PRIVATE npu_compute_launcher_core)
add_test(NAME npu_compute.cli COMMAND npu_compute_cli_test)

add_executable(npu_compute_rep_decoder_test rep_decoder_test.cpp)
target_link_libraries(npu_compute_rep_decoder_test PRIVATE npu_compute_launcher_core)
add_test(NAME npu_compute.rep_decoder COMMAND npu_compute_rep_decoder_test)

add_executable(npu_compute_imported_profile_results_test imported_profile_results_test.cpp)
target_link_libraries(npu_compute_imported_profile_results_test PRIVATE npu_compute_launcher_core)
add_test(NAME npu_compute.imported_profile_results
  COMMAND npu_compute_imported_profile_results_test $<TARGET_FILE:npu_compute_cli>
)

add_executable(npu_compute_launcher_test launcher_test.cpp)
target_link_libraries(npu_compute_launcher_test PRIVATE npu_compute_launcher_core)
add_dependencies(npu_compute_launcher_test npu_compute)
npu_compute_set_output(npu_compute_launcher_test)
add_test(NAME npu_compute.launcher COMMAND npu_compute_launcher_test)

add_executable(npu_compute_rep_fixture_app rep_fixture_app.cpp)
add_dependencies(npu_compute_rep_fixture_app npu_compute_cli npu_compute)
npu_compute_set_output(npu_compute_rep_fixture_app)

add_executable(acl_pti_public_api_test public_api_test.cpp)
target_link_libraries(acl_pti_public_api_test
  PRIVATE acl_pti ${NPU_COMPUTE_ACLPTI_TEST_BACKENDS}
)
set_target_properties(acl_pti_public_api_test PROPERTIES ENABLE_EXPORTS YES)
add_test(NAME npu_compute.public_api COMMAND acl_pti_public_api_test)

add_executable(acl_pti_callback_api_test callback_api_test.cpp)
target_link_libraries(acl_pti_callback_api_test
  PRIVATE acl_pti ${NPU_COMPUTE_ACLPTI_TEST_BACKENDS}
)
set_target_properties(acl_pti_callback_api_test PROPERTIES ENABLE_EXPORTS YES)
add_test(NAME npu_compute.callback_api COMMAND acl_pti_callback_api_test)

add_executable(acl_pti_replay_test replay_test.cpp)
target_link_libraries(acl_pti_replay_test
  PRIVATE acl_pti ${NPU_COMPUTE_ACLPTI_TEST_BACKENDS}
)
set_target_properties(acl_pti_replay_test PROPERTIES ENABLE_EXPORTS YES)
add_test(NAME npu_compute.replay COMMAND acl_pti_replay_test)

add_executable(acl_pti_shadow_memory_test shadow_memory_test.cpp)
target_link_libraries(acl_pti_shadow_memory_test
  PRIVATE acl_pti ${NPU_COMPUTE_ACLPTI_TEST_BACKENDS}
)
set_target_properties(acl_pti_shadow_memory_test PROPERTIES ENABLE_EXPORTS YES)
add_test(NAME npu_compute.shadow_memory COMMAND acl_pti_shadow_memory_test)

add_executable(acl_pti_launch_without_config_test launch_without_config_test.cpp)
target_link_libraries(acl_pti_launch_without_config_test
  PRIVATE acl_pti ${NPU_COMPUTE_ACLPTI_TEST_BACKENDS}
)
set_target_properties(acl_pti_launch_without_config_test PROPERTIES ENABLE_EXPORTS YES)
add_test(NAME npu_compute.launch_without_config
  COMMAND acl_pti_launch_without_config_test
)

add_executable(acl_pti_initialize_failure_test pti_initialize_failure_test.cpp)
target_link_libraries(acl_pti_initialize_failure_test
  PRIVATE acl_pti ${NPU_COMPUTE_ACLPTI_TEST_BACKENDS}
)
set_target_properties(acl_pti_initialize_failure_test PROPERTIES ENABLE_EXPORTS YES)
add_test(NAME npu_compute.pti_initialize_failure
  COMMAND acl_pti_initialize_failure_test
)

add_executable(acl_pti_runtime_failure_short_circuit_test
  runtime_failure_short_circuit_test.cpp
)
target_link_libraries(acl_pti_runtime_failure_short_circuit_test
  PRIVATE acl_pti ${NPU_COMPUTE_ACLPTI_TEST_BACKENDS}
)
set_target_properties(acl_pti_runtime_failure_short_circuit_test
  PROPERTIES ENABLE_EXPORTS YES
)
add_test(NAME npu_compute.runtime_failure_short_circuit
  COMMAND acl_pti_runtime_failure_short_circuit_test
)

add_library(acl_prof_api_loader_test_plugin SHARED loader_test_plugin.cpp)
set_target_properties(acl_prof_api_loader_test_plugin PROPERTIES
  OUTPUT_NAME loader_test_plugin
  PREFIX ""
)

add_executable(acl_prof_api_loader_test prof_api_loader_test.cpp)
target_link_libraries(acl_prof_api_loader_test PRIVATE acl_prof_api_backend)
add_test(NAME npu_compute.prof_api_loader
  COMMAND acl_prof_api_loader_test $<TARGET_FILE:acl_prof_api_loader_test_plugin>
)

add_executable(acl_prof_api_stub_test prof_api_stub_test.cpp)
target_link_libraries(acl_prof_api_stub_test PRIVATE acl_prof_api_stub)
add_test(NAME npu_compute.prof_api_stub COMMAND acl_prof_api_stub_test)

add_executable(acl_injection_hook_test injection_hook_test.cpp)
target_link_libraries(acl_injection_hook_test
  PRIVATE acl_tool_injection acl_runtime_stub acl_prof_api_backend
)
add_test(NAME npu_compute.injection_hook
  COMMAND acl_injection_hook_test
)

add_executable(acl_pti_data_module_test pti_data_module_impl_test.cpp)
target_sources(acl_pti_data_module_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/acl_pti/data/api.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/acl_pti/data/module.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/acl_pti/data/raw_data_decoder.cpp
)
target_include_directories(acl_pti_data_module_test BEFORE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_SOURCE_DIR}/../stubs/runtime/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../stubs/prof_api/include
)
target_link_libraries(acl_pti_data_module_test
  PRIVATE npu_compute_integration_headers Threads::Threads
)
add_test(NAME npu_compute.acl_pti_data_module
  COMMAND acl_pti_data_module_test
)

add_executable(npu_compute_pmu_data_consumer_test pmu_data_consumer_test.cpp)
target_include_directories(npu_compute_pmu_data_consumer_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
)
target_link_libraries(npu_compute_pmu_data_consumer_test
  PRIVATE acl_pti Threads::Threads
)
target_sources(npu_compute_pmu_data_consumer_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute/pmu_data_consumer.cpp
)
add_test(NAME npu_compute.pmu_data_consumer
  COMMAND npu_compute_pmu_data_consumer_test
)

add_executable(npu_compute_pmu_csv_writer_test pmu_csv_writer_test.cpp)
target_include_directories(npu_compute_pmu_csv_writer_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
)
target_link_libraries(npu_compute_pmu_csv_writer_test
  PRIVATE acl_pti
)
target_sources(npu_compute_pmu_csv_writer_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute/pmu_csv_writer.cpp
)
add_test(NAME npu_compute.pmu_csv_writer
  COMMAND npu_compute_pmu_csv_writer_test
)

add_executable(npu_compute_debug_log_test debug_log_test.cpp)
target_include_directories(npu_compute_debug_log_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
add_test(NAME npu_compute.debug_log COMMAND npu_compute_debug_log_test)

add_executable(npu_compute_hardware_info_json_test
  hardware_info_json_test.cpp
  ../src/npu_compute/hardware_info_json.cpp
)
target_include_directories(npu_compute_hardware_info_json_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
)
add_test(NAME npu_compute.hardware_info_json
  COMMAND npu_compute_hardware_info_json_test
)

add_executable(npu_compute_hardware_info_host_test
  hardware_info_host_test.cpp
  ../src/npu_compute/hardware_info_host.cpp
)
target_include_directories(npu_compute_hardware_info_host_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
)
add_test(NAME npu_compute.hardware_info_host
  COMMAND npu_compute_hardware_info_host_test
)

add_executable(npu_compute_hardware_info_device_test
  hardware_info_device_test.cpp
  ../src/npu_compute/hardware_info_device.cpp
)
target_include_directories(npu_compute_hardware_info_device_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
)
add_test(NAME npu_compute.hardware_info_device
  COMMAND npu_compute_hardware_info_device_test
)

add_executable(npu_compute_hardware_device_api_test
  hardware_device_api_test.cpp
  ../src/npu_compute/hardware_device_api.cpp
)
target_include_directories(npu_compute_hardware_device_api_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
)
target_link_libraries(npu_compute_hardware_device_api_test PRIVATE
  npu_compute_headers
  ${CMAKE_DL_LIBS}
)
add_test(NAME npu_compute.hardware_device_api
  COMMAND npu_compute_hardware_device_api_test
)

add_executable(npu_compute_hardware_info_writer_test
  hardware_info_writer_test.cpp
  ../src/npu_compute/hardware_info_writer.cpp
)
target_include_directories(npu_compute_hardware_info_writer_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
)
add_test(NAME npu_compute.hardware_info_writer
  COMMAND npu_compute_hardware_info_writer_test
)

add_executable(npu_compute_hardware_info_collector_test
  hardware_info_collector_test.cpp
  ../src/npu_compute/hardware_info_collector.cpp
  ../src/npu_compute/hardware_device_api.cpp
  ../src/npu_compute/hardware_info_device.cpp
  ../src/npu_compute/hardware_info_host.cpp
  ../src/npu_compute/hardware_info_json.cpp
  ../src/npu_compute/hardware_info_writer.cpp
)
target_include_directories(npu_compute_hardware_info_collector_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
)
target_link_libraries(npu_compute_hardware_info_collector_test PRIVATE
  npu_compute_headers
  ${CMAKE_DL_LIBS}
  Threads::Threads
)
add_test(NAME npu_compute.hardware_info_collector
  COMMAND npu_compute_hardware_info_collector_test
)

add_executable(acl_pti_callback_stub_test
  acl_pti_callback_stub_test.cpp
)
target_link_libraries(acl_pti_callback_stub_test PRIVATE
  acl_pti_callback_stub
)
add_test(NAME npu_compute.acl_pti_callback_stub
  COMMAND acl_pti_callback_stub_test
)

set(NPU_COMPUTE_RUNTIME_IMPLEMENTATION_SOURCES
  ../src/npu_compute/hardware_device_api.cpp
  ../src/npu_compute/hardware_info_collector.cpp
  ../src/npu_compute/hardware_info_device.cpp
  ../src/npu_compute/hardware_info_host.cpp
  ../src/npu_compute/hardware_info_json.cpp
  ../src/npu_compute/hardware_info_writer.cpp
  ../src/npu_compute/npu_compute.cpp
  ../src/npu_compute/npu_compute_runtime.cpp
  ../src/npu_compute/pmu_data_consumer.cpp
  ../src/npu_compute/pmu_csv_writer.cpp
  ../src/npu_compute/section_config.cpp
)

add_executable(npu_compute_runtime_callback_test
  npu_compute_runtime_callback_test.cpp
  ${NPU_COMPUTE_RUNTIME_IMPLEMENTATION_SOURCES}
)
target_include_directories(npu_compute_runtime_callback_test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
)
target_link_libraries(npu_compute_runtime_callback_test PRIVATE
  acl_pti_callback_stub
  npu_compute_integration_headers
  acl_pti
  ${CMAKE_DL_LIBS}
  Threads::Threads
)
set_target_properties(npu_compute_runtime_callback_test PROPERTIES
  ENABLE_EXPORTS YES
)
add_test(NAME npu_compute.runtime_callback
  COMMAND npu_compute_runtime_callback_test
)

if(NPU_COMPUTE_BUILD_REAL_HARDWARE_TESTS)
  add_library(npu_compute_real_hardware_test_library SHARED
    ${NPU_COMPUTE_RUNTIME_IMPLEMENTATION_SOURCES}
  )
  target_include_directories(
    npu_compute_real_hardware_test_library PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute
  )
  target_link_libraries(npu_compute_real_hardware_test_library PRIVATE
    acl_pti_callback_stub
    npu_compute_integration_headers
    acl_pti
    ${CMAKE_DL_LIBS}
    Threads::Threads
  )
  set(NPU_COMPUTE_REAL_HARDWARE_VERSION_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/npu_compute/libnpu_compute.map"
  )
  target_link_options(npu_compute_real_hardware_test_library PRIVATE
    "LINKER:--version-script=${NPU_COMPUTE_REAL_HARDWARE_VERSION_SCRIPT}"
  )
  set_target_properties(
    npu_compute_real_hardware_test_library PROPERTIES
    OUTPUT_NAME npu-compute-real-hardware-test
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
    LINK_DEPENDS "${NPU_COMPUTE_REAL_HARDWARE_VERSION_SCRIPT}"
  )
  npu_compute_set_output(npu_compute_real_hardware_test_library)

  find_library(NPU_COMPUTE_REAL_ASCENDCL_LIBRARY
    NAMES ascendcl
    HINTS
      "${NPUCOMPUTE_CANN_ROOT}/lib64"
      "${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/lib64"
      "$ENV{ASCEND_HOME_PATH}/lib64"
    NO_DEFAULT_PATH
  )
  if(NOT NPU_COMPUTE_REAL_ASCENDCL_LIBRARY)
    message(FATAL_ERROR
      "Real HardwareInfo tests require libascendcl.so from the CANN installation")
  endif()

  add_executable(npu_compute_real_hardware_callback_app
    real_hardware_callback_app.cpp
  )
  target_link_libraries(
    npu_compute_real_hardware_callback_app PRIVATE
    acl_pti_callback_stub
    npu_compute_headers
    "${NPU_COMPUTE_REAL_ASCENDCL_LIBRARY}"
    ${CMAKE_DL_LIBS}
  )
  set_target_properties(
    npu_compute_real_hardware_callback_app PROPERTIES
    OUTPUT_NAME npu_compute_real_hardware_callback_app
  )
  npu_compute_set_output(npu_compute_real_hardware_callback_app)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_test(
  NAME npu_compute.rep_packaging_integration
  COMMAND ${CMAKE_COMMAND} -E env
    "NPU_COMPUTE_TEST_CLI=$<TARGET_FILE:npu_compute_cli>"
    "NPU_COMPUTE_REP_FIXTURE_APP=$<TARGET_FILE:npu_compute_rep_fixture_app>"
    ${Python3_EXECUTABLE} -m pytest -q -p no:cacheprovider
    ${CMAKE_CURRENT_SOURCE_DIR}/test_rep_packaging_integration.py
)

if(NPU_COMPUTE_BUILD_REAL_HARDWARE_TESTS)
  add_test(
    NAME npu_compute.real_hardware_with_callback_stub
    COMMAND ${CMAKE_COMMAND} -E env
      "NPU_COMPUTE_REAL_HW_APP=$<TARGET_FILE:npu_compute_real_hardware_callback_app>"
      "NPU_COMPUTE_REAL_HW_LIBRARY=$<TARGET_FILE:npu_compute_real_hardware_test_library>"
      ${Python3_EXECUTABLE} -m pytest -q -p no:cacheprovider
      ${CMAKE_CURRENT_SOURCE_DIR}/test_real_hardware_collection.py
  )
  set_tests_properties(
    npu_compute.real_hardware_with_callback_stub PROPERTIES
    LABELS "requires_npu"
  )
endif()

add_test(
  NAME npu_compute.hardware_info_integration
  COMMAND ${CMAKE_COMMAND} -E env
    "NPU_COMPUTE_TEST_BIN_DIR=$<TARGET_FILE_DIR:npu_compute>"
    "NPU_COMPUTE_CALLBACK_STUB=$<TARGET_FILE:acl_pti_callback_stub>"
    "NPU_COMPUTE_HARDWARE_API_STUB=$<TARGET_FILE:npu_compute_hardware_api_stub>"
    ${Python3_EXECUTABLE} -m pytest -q -p no:cacheprovider
    ${CMAKE_CURRENT_SOURCE_DIR}/test_hardware_info_integration.py
)

add_test(
  NAME npu_compute.subscribe_lifecycle
  COMMAND ${CMAKE_COMMAND} -E env
    "NPU_COMPUTE_TEST_BIN_DIR=$<TARGET_FILE_DIR:npu_compute>"
    ${Python3_EXECUTABLE} -m pytest -q -p no:cacheprovider
    ${CMAKE_CURRENT_SOURCE_DIR}/test_subscriber_lifecycle.py
)

add_test(
  NAME npu_compute.integration_stubs
  COMMAND ${CMAKE_COMMAND} -E env
    "NPU_COMPUTE_TEST_BIN_DIR=$<TARGET_FILE_DIR:npu_compute>"
    ${Python3_EXECUTABLE} -m pytest -q -p no:cacheprovider
    ${CMAKE_CURRENT_SOURCE_DIR}/test_integration_stubs.py
)

add_test(
  NAME npu_compute.connected_integration
  COMMAND ${CMAKE_COMMAND} -E env
    "NPU_COMPUTE_TEST_BIN_DIR=$<TARGET_FILE_DIR:npu_compute>"
    ${Python3_EXECUTABLE} -m pytest -q -p no:cacheprovider
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/py_ut/testcase/npu_compute/test_npu_compute_integration.py
)

add_test(
  NAME npu_compute.install_layout
  COMMAND ${CMAKE_COMMAND} -E env
    "NPU_COMPUTE_TEST_BUILD_DIR=${CMAKE_BINARY_DIR}"
    "NPU_COMPUTE_TEST_BIN_DIR=$<TARGET_FILE_DIR:npu_compute>"
    ${Python3_EXECUTABLE} -m pytest -q -p no:cacheprovider
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/py_ut/testcase/npu_compute/test_npu_compute_install.py
)
