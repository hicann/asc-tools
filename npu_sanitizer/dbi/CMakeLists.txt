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

add_library(npu_check_dbi_ctrlbin STATIC
  src/dynamic_bind.cpp
  src/msbit/MSBit.cpp
)
target_compile_features(npu_check_dbi_ctrlbin PUBLIC cxx_std_17)
target_compile_definitions(npu_check_dbi_ctrlbin PRIVATE ASCSAN_CTRLBIN_NO_MAIN=1)
target_include_directories(npu_check_dbi_ctrlbin
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/msbit
)
target_compile_options(npu_check_dbi_ctrlbin PRIVATE -Wall -Wextra -Werror)
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
  PRIVATE ${CMAKE_DL_LIBS} Threads::Threads
)
set_target_properties(npu_check_dbi_engine PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)
