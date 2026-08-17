# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
add_executable(npu_compute_cli
  config.cpp
  injection_path.cpp
  launcher.cpp
  main.cpp
  process_launcher.cpp
  staging_directory.cpp
)
target_link_libraries(npu_compute_cli PRIVATE npu_compute_headers)
set_target_properties(npu_compute_cli PROPERTIES OUTPUT_NAME npu-compute)
npu_compute_set_output(npu_compute_cli)

install(TARGETS npu_compute_cli
  RUNTIME DESTINATION bin
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  COMPONENT npu-compute-integration
)
