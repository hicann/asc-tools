# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
add_library(acl_tool_injection_test SHARED
  ${INJECTION_SOURCE_DIR}/src/injection_hook.cpp
)
target_compile_definitions(acl_tool_injection_test PRIVATE ACL_TOOL_INJECTION_BUILD=1)
target_include_directories(acl_tool_injection_test PRIVATE "${INJECTION_SOURCE_DIR}/src")
target_link_libraries(acl_tool_injection_test
  PUBLIC injection_test_headers
  PRIVATE Threads::Threads acl_runtime_stub acl_prof_api_stub
)
target_compile_options(acl_tool_injection_test PRIVATE -Wall -Wextra -Werror)
target_link_options(acl_tool_injection_test PRIVATE -Wl,-z,defs)
set_target_properties(acl_tool_injection_test PROPERTIES
  OUTPUT_NAME acl_tool_injection_test
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
)
injection_set_test_output(acl_tool_injection_test)

add_executable(acl_injection_hook_test injection_hook_test.cpp)
target_link_libraries(acl_injection_hook_test
  PRIVATE acl_tool_injection_test acl_runtime_stub acl_prof_api_stub)
add_test(NAME injection.hook COMMAND acl_injection_hook_test)
