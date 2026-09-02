# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
add_library(npu_compute_launcher_core STATIC
  collection_file_validator.cpp
  config.cpp
  import_output_directory.cpp
  imported_profile_results.cpp
  injection_path.cpp
  launcher.cpp
  process_launcher.cpp
  rep_decoder.cpp
  rep_directory_packer.cpp
  rep_encoder.cpp
  rep_report_writer.cpp
  report_name.cpp
  staging_directory.cpp
)
target_include_directories(npu_compute_launcher_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(npu_compute_launcher_core PUBLIC npu_compute_headers Boost::filesystem)

add_executable(npu_compute_cli main.cpp)
target_link_libraries(npu_compute_cli PRIVATE npu_compute_launcher_core)
set_target_properties(npu_compute_cli PROPERTIES OUTPUT_NAME npu-compute)
npu_compute_set_output(npu_compute_cli)

install(TARGETS npu_compute_cli
  RUNTIME DESTINATION "${NPU_COMPUTE_INSTALL_BINDIR}"
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  COMPONENT asc-tools
)
