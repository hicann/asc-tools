# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
add_library(npu_compute SHARED
  hardware_device_api.cpp
  hardware_info_collector.cpp
  hardware_info_device.cpp
  hardware_info_host.cpp
  hardware_info_json.cpp
  hardware_info_writer.cpp
  npu_compute.cpp
  npu_compute_runtime.cpp
  pmu_data_consumer.cpp
  pmu_csv_writer.cpp
  section_config.cpp
)
target_include_directories(npu_compute PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)
set(NPU_COMPUTE_VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/libnpu_compute.map")
if(TARGET npu_compute_integration_headers)
  set(NPU_COMPUTE_LIBRARY_HEADERS npu_compute_integration_headers)
else()
  set(NPU_COMPUTE_LIBRARY_HEADERS npu_compute_headers)
endif()
target_link_libraries(npu_compute
  PUBLIC ${NPU_COMPUTE_LIBRARY_HEADERS}
  PRIVATE ${CMAKE_DL_LIBS} Threads::Threads
)
if(TARGET acl_pti)
  target_link_libraries(npu_compute PRIVATE acl_pti)
endif()
target_link_options(npu_compute PRIVATE
  "LINKER:--version-script=${NPU_COMPUTE_VERSION_SCRIPT}"
)
set_target_properties(npu_compute PROPERTIES
  OUTPUT_NAME npu-compute
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES
  LINK_DEPENDS "${NPU_COMPUTE_VERSION_SCRIPT}"
)
npu_compute_set_output(npu_compute)

install(TARGETS npu_compute
  LIBRARY DESTINATION lib64
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  COMPONENT npu-compute-integration
)
