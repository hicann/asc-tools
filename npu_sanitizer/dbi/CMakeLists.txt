# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

if(TARGET npu_check_dbi_engine)
  return()
endif()

find_package(Threads REQUIRED)

set(NPU_CHECK_PROBE_ARCHES "dav-3510" CACHE STRING
  "Public NPU architecture names supported by the runtime Probe generator")

add_library(npu_check_dbi_probe_group STATIC src/probe_group.cpp)
target_compile_features(npu_check_dbi_probe_group PUBLIC cxx_std_17)
target_include_directories(npu_check_dbi_probe_group PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(npu_check_dbi_probe_group PRIVATE -Wall -Wextra -Werror)
set_target_properties(npu_check_dbi_probe_group PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

set(NPU_CHECK_PROBE_RESOURCE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_probe_resources.cpp")
add_custom_command(
  OUTPUT "${NPU_CHECK_PROBE_RESOURCE_SOURCE}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
  COMMAND "${CMAKE_COMMAND}"
    "-DTRACE_RECORD=${CMAKE_CURRENT_SOURCE_DIR}/include/trace_record.h"
    "-DTRACE_BUFFER_ABI=${CMAKE_CURRENT_SOURCE_DIR}/include/trace_buffer_abi.h"
    "-DCTRLBIN_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/dynamic_bind.cpp"
    "-DCTRLBIN_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/include/ctrlbin_generator.h"
    "-DMSBIT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/MSBit.cpp"
    "-DMSBIT_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/MSBit.h"
    "-DMSBIT_FILESYSTEM_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/FileSystem.h"
    "-DMSBIT_SERIALIZE_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/Serialize.h"
    "-DMSBIT_SINGLETON_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/Singleton.h"
    "-DMSBIT_USTRING_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/Ustring.h"
    "-DOUTPUT=${NPU_CHECK_PROBE_RESOURCE_SOURCE}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_probe_resources.cmake"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/trace_record.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/trace_buffer_abi.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dynamic_bind.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ctrlbin_generator.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/MSBit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/MSBit.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/FileSystem.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/Serialize.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/Singleton.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/msbit/Ustring.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_probe_resources.cmake"
  VERBATIM)

add_library(npu_check_dbi_probe_runtime STATIC
  src/probe_source_generator.cpp
  "${NPU_CHECK_PROBE_RESOURCE_SOURCE}"
)
target_compile_features(npu_check_dbi_probe_runtime PUBLIC cxx_std_17)
target_include_directories(npu_check_dbi_probe_runtime PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(npu_check_dbi_probe_runtime PRIVATE -Wall -Wextra -Werror)
target_link_libraries(npu_check_dbi_probe_runtime PUBLIC npu_check_dbi_probe_group)
set_target_properties(npu_check_dbi_probe_runtime PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

add_library(npu_check_dbi_ctrlbin STATIC
  src/dynamic_bind.cpp
  src/msbit/MSBit.cpp
)
target_compile_features(npu_check_dbi_ctrlbin PUBLIC cxx_std_17)
target_include_directories(npu_check_dbi_ctrlbin
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/msbit
)
target_compile_options(npu_check_dbi_ctrlbin PRIVATE -Wall -Wextra -Werror)
target_link_libraries(npu_check_dbi_ctrlbin PUBLIC npu_check_dbi_probe_group)
set_source_files_properties(src/msbit/MSBit.cpp PROPERTIES COMPILE_OPTIONS -Wno-missing-field-initializers)
set_target_properties(npu_check_dbi_ctrlbin PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)

add_library(npu_check_dbi_engine STATIC
  src/binary_instrumenter.cpp
  src/dbi_pipeline.cpp
  src/tool_runner.cpp
)
target_compile_features(npu_check_dbi_engine PUBLIC cxx_std_17)
target_include_directories(npu_check_dbi_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(npu_check_dbi_engine PRIVATE -Wall -Wextra -Werror)
target_link_libraries(npu_check_dbi_engine
  PUBLIC npu_check_dbi_ctrlbin
  PRIVATE npu_check_dbi_probe_runtime ${CMAKE_DL_LIBS} Threads::Threads
)
set_target_properties(npu_check_dbi_engine PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)
