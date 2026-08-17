# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
add_library(acl_runtime_stub SHARED
  runtime_stub.cpp
)
target_include_directories(acl_runtime_stub
  PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
)
target_link_libraries(acl_runtime_stub
  PUBLIC npu_compute_integration_headers
  PRIVATE acl_prof_api_stub ${CMAKE_DL_LIBS}
)
set_target_properties(acl_runtime_stub PROPERTIES
  OUTPUT_NAME runtime
)
npu_compute_set_output(acl_runtime_stub)
