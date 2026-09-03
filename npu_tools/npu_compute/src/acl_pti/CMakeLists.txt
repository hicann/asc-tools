# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
find_package(Threads REQUIRED)

add_library(acl_pti SHARED
  initialization.cpp
  activity/api.cpp
  activity/control.cpp
  callback/api.cpp
  callback/dispatcher.cpp
  data/api.cpp
  data/module.cpp
  data/raw_data_decoder.cpp
  profiling/api.cpp
  profiling/range_profiler.cpp
  profiling/replay_memory.cpp
  profiling/replay_runtime.cpp
  replacement/runtime_api_replacements.cpp
)
target_include_directories(acl_pti PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(acl_pti
  PUBLIC npu_compute_backend_headers
  PRIVATE
    acl_tool_injection
    acl_prof_api_backend
    Threads::Threads
)
if(NPU_COMPUTE_BUILD_TESTS)
  target_compile_definitions(acl_pti PRIVATE
    NPU_COMPUTE_ENABLE_TEST_CONTROLS=1
  )
endif()
set_target_properties(acl_pti PROPERTIES
  OUTPUT_NAME acl_pti
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)
npu_compute_set_product_output(acl_pti)
