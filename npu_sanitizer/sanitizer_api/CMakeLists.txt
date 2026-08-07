# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16.0)
project(ascsan_sanitizer_api)

add_library(ascsan_api SHARED
  src/api_core.cpp
  src/ascsan_api.cpp
  src/callback_registry.cpp
  src/dummy_backends.cpp
  src/hook_plan_builder.cpp
  src/memory_service.cpp
  src/patch_manager.cpp
  src/trace_manager.cpp
)

target_compile_features(ascsan_api PUBLIC cxx_std_17)
target_include_directories(ascsan_api
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ascsan_api PRIVATE dl pthread)

set_target_properties(ascsan_api PROPERTIES
  OUTPUT_NAME sanitizer_api
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  add_executable(ascsan_api_smoke tests/api_smoke.cpp)
  target_compile_features(ascsan_api_smoke PRIVATE cxx_std_17)
  target_link_libraries(ascsan_api_smoke PRIVATE ascsan_api)
endif()

install(TARGETS ascsan_api
  LIBRARY DESTINATION lib ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
)

install(FILES
  include/ascsan/ascsan_api.h
  include/ascsan/ascsan_callback.h
  include/ascsan/ascsan_memory.h
  include/ascsan/ascsan_patch.h
  include/ascsan/ascsan_types.h
  DESTINATION include/ascsan ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
)
