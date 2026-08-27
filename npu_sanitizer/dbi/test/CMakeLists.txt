# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

add_test(NAME aclsan.probe_declaration_contract
  COMMAND ${CMAKE_COMMAND}
    -DDBI_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/..
    -P ${CMAKE_CURRENT_SOURCE_DIR}/probe_declaration_contract_test.cmake)

add_test(NAME aclsan.no_legacy_acl_hook_registration
  COMMAND ${CMAKE_COMMAND}
    -DDBI_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/..
    -P ${CMAKE_CURRENT_SOURCE_DIR}/no_legacy_acl_hook_registration_test.cmake)

if(NPU_CHECK_RUN_REAL_DBI_HARDWARE AND NOT NPU_CHECK_ENABLE_REAL_DBI_FLOW)
  message(FATAL_ERROR "NPU_CHECK_RUN_REAL_DBI_HARDWARE requires NPU_CHECK_ENABLE_REAL_DBI_FLOW=ON")
endif()

if(NOT NPU_CHECK_ENABLE_REAL_DBI_FLOW AND NOT NPU_CHECK_ENABLE_REAL_DBI_TOOLCHAIN)
  return()
endif()

if(NPU_CHECK_ENABLE_REAL_DBI_FLOW AND TARGET acl_runtime_stub)
  message(FATAL_ERROR "NPU_CHECK_ENABLE_REAL_DBI_FLOW requires NPU_COMPUTE_BUILD_TESTS=OFF")
endif()

foreach(required_variable IN ITEMS NPU_CHECK_REAL_DBI_ARCH NPU_CHECK_REAL_DBI_TOOLCHAIN_ROOT)
  if("${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required when real DBI tests are enabled")
  endif()
endforeach()

foreach(tool IN ITEMS bisheng bisheng-tune ld.lld llvm-objdump)
  string(REPLACE "." "_" tool_variable "NPU_CHECK_REAL_${tool}")
  string(REPLACE "-" "_" tool_variable "${tool_variable}")
  find_program(${tool_variable} NAMES ${tool}
    HINTS "${NPU_CHECK_REAL_DBI_TOOLCHAIN_ROOT}/tools/bisheng_compiler/bin"
    NO_DEFAULT_PATH)
  if(NOT ${tool_variable})
    message(FATAL_ERROR "real DBI flow requires executable ${tool}")
  endif()
endforeach()

if(NPU_CHECK_REAL_DBI_KERNEL STREQUAL "")
  set(NPU_CHECK_REAL_DBI_KERNEL "${CMAKE_CURRENT_BINARY_DIR}/full_flow_kernel.o")
  add_custom_command(
    OUTPUT "${NPU_CHECK_REAL_DBI_KERNEL}"
    COMMAND "${CMAKE_COMMAND}" -E env
      "ASCEND_HOME_PATH=${NPU_CHECK_REAL_DBI_TOOLCHAIN_ROOT}"
      "ASCEND_INSTALL_PATH=${NPU_CHECK_REAL_DBI_TOOLCHAIN_ROOT}"
      "${NPU_CHECK_REAL_bisheng}"
      -xasc --cce-aicore-only "--cce-aicore-arch=${NPU_CHECK_REAL_DBI_ARCH}"
      -I "${NPU_CHECK_REAL_DBI_TOOLCHAIN_ROOT}/${CMAKE_SYSTEM_PROCESSOR}-linux/asc"
      -I "${NPU_CHECK_REAL_DBI_TOOLCHAIN_ROOT}/${CMAKE_SYSTEM_PROCESSOR}-linux/asc/include"
      -c "${CMAKE_CURRENT_SOURCE_DIR}/full_flow_kernel.cpp"
      -o "${NPU_CHECK_REAL_DBI_KERNEL}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/full_flow_kernel.cpp"
    VERBATIM)
  add_custom_target(aclsan_full_flow_real_kernel ALL DEPENDS "${NPU_CHECK_REAL_DBI_KERNEL}")
elseif(NOT EXISTS "${NPU_CHECK_REAL_DBI_KERNEL}")
  message(FATAL_ERROR "NPU_CHECK_REAL_DBI_KERNEL does not exist: ${NPU_CHECK_REAL_DBI_KERNEL}")
endif()

set(NPU_CHECK_REAL_DBI_SOURCE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/full-flow-real-sources")
file(MAKE_DIRECTORY "${NPU_CHECK_REAL_DBI_SOURCE_ROOT}/probes")
foreach(probe IN ITEMS fixpipe mte1 mte2 mte3 register sync)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/probes/${probe}.cpp"
    "${NPU_CHECK_REAL_DBI_SOURCE_ROOT}/probes/${probe}.cpp" COPYONLY)
endforeach()
foreach(header IN ITEMS trace_record trace_buffer_abi)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/${header}.h"
    "${NPU_CHECK_REAL_DBI_SOURCE_ROOT}/${header}.h" COPYONLY)
endforeach()

add_executable(aclsan_full_flow_real_tool_smoke real_tool_smoke.cpp)
target_compile_features(aclsan_full_flow_real_tool_smoke PRIVATE cxx_std_17)
if(ENABLE_TEST)
  target_compile_definitions(aclsan_full_flow_real_tool_smoke PRIVATE _GLIBCXX_USE_CXX11_ABI=0)
endif()
target_compile_options(aclsan_full_flow_real_tool_smoke PRIVATE -Wall -Wextra -Werror)
target_link_libraries(aclsan_full_flow_real_tool_smoke PRIVATE npu_check_dbi_engine)
if(TARGET aclsan_full_flow_real_kernel)
  add_dependencies(aclsan_full_flow_real_tool_smoke aclsan_full_flow_real_kernel)
endif()
add_test(NAME aclsan.full_flow_real_toolchain
  COMMAND ${CMAKE_COMMAND}
    -DSMOKE_EXECUTABLE=$<TARGET_FILE:aclsan_full_flow_real_tool_smoke>
    -DINPUT_KERNEL=${NPU_CHECK_REAL_DBI_KERNEL}
    -DOUTPUT_KERNEL=${CMAKE_CURRENT_BINARY_DIR}/full-flow-real-toolchain/patched.o
    -DARCH=${NPU_CHECK_REAL_DBI_ARCH}
    -DARG_SIZE=24
    -DTOOLCHAIN_ROOT=${NPU_CHECK_REAL_DBI_TOOLCHAIN_ROOT}
    -DSOURCE_ROOT=${NPU_CHECK_REAL_DBI_SOURCE_ROOT}
    -DWORK_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/full-flow-real-toolchain/work
    -DCACHE_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}/full-flow-real-toolchain/cache
    -P ${CMAKE_CURRENT_SOURCE_DIR}/real_tool_smoke.cmake)

if(NPU_CHECK_ENABLE_REAL_DBI_FLOW)
  include(CheckCXXSourceCompiles)
  set(saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
  set(saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
  set(CMAKE_REQUIRED_INCLUDES
    "${NPUCOMPUTE_ACL_INCLUDE_DIR}"
    "${NPUCOMPUTE_PROFILING_INCLUDE_DIR}")
  set(CMAKE_REQUIRED_LIBRARIES CANN::acl_rt CANN::profapi)
  check_cxx_source_compiles([[
    #include <acl/acl_rt.h>
    #include <profiling/prof_api.h>
    int main()
    {
        aclrtApiFunc function = nullptr;
        (void)aclrtApiInjectionGetFunc("aclrtMalloc", &function, &function);
        (void)aclrtApiInjectionSetFunc("aclrtMalloc", function);
        return MsprofRegisterDataCallback(0, nullptr);
    }
  ]] NPU_CHECK_CANN_HAS_RUNTIME_INJECTION_ABI)
  set(CMAKE_REQUIRED_INCLUDES "${saved_required_includes}")
  set(CMAKE_REQUIRED_LIBRARIES "${saved_required_libraries}")
  if(NOT NPU_CHECK_CANN_HAS_RUNTIME_INJECTION_ABI)
    message(FATAL_ERROR
      "real NPU DBI flow requires a CANN package that exports the Runtime injection ABI "
      "(aclrtApiInjectionGetFunc/SetFunc and MsprofRegisterDataCallback)")
  endif()

  add_executable(aclsan_full_flow_real_sample full_flow_real_sample.cpp)
  target_compile_features(aclsan_full_flow_real_sample PRIVATE cxx_std_17)
  target_link_libraries(aclsan_full_flow_real_sample
    PRIVATE acl_san acl_tool_injection acl_runtime_backend)
  if(TARGET aclsan_full_flow_real_kernel)
    add_dependencies(aclsan_full_flow_real_sample aclsan_full_flow_real_kernel)
  endif()
  if(NPU_CHECK_RUN_REAL_DBI_HARDWARE)
    add_test(NAME aclsan.full_flow_real_sample
      COMMAND ${CMAKE_COMMAND}
        -DSAMPLE=$<TARGET_FILE:aclsan_full_flow_real_sample>
        -DKERNEL=${NPU_CHECK_REAL_DBI_KERNEL}
        -DDEVICE_ID=${NPU_CHECK_REAL_DBI_DEVICE_ID}
        -DARCH=${NPU_CHECK_REAL_DBI_ARCH}
        -DTOOLCHAIN_ROOT=${NPU_CHECK_REAL_DBI_TOOLCHAIN_ROOT}
        -DSOURCE_ROOT=${NPU_CHECK_REAL_DBI_SOURCE_ROOT}
        -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/full-flow-real
        -P ${CMAKE_CURRENT_SOURCE_DIR}/run_full_flow_real_sample.cmake)
    set_tests_properties(aclsan.full_flow_real_sample PROPERTIES
      LABELS hardware RESOURCE_LOCK npu_device)
  endif()
endif()
