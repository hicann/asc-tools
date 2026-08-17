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

add_library(acl_tool_injection SHARED
  injection_hook.cpp
)
target_include_directories(acl_tool_injection PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(acl_tool_injection
  PUBLIC npu_compute_integration_headers
  PRIVATE
    Threads::Threads
    acl_runtime_stub
    acl_prof_api_stub
)
target_compile_options(acl_tool_injection PRIVATE -Wall -Wextra -Werror)
target_link_options(acl_tool_injection PRIVATE -Wl,-z,defs)

set_target_properties(acl_tool_injection PROPERTIES
  OUTPUT_NAME acl_tool_injection
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)
npu_compute_set_output(acl_tool_injection)

install(TARGETS acl_tool_injection
  LIBRARY DESTINATION lib64
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  COMPONENT npu-compute-integration
)
