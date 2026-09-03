# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
set(NPU_SANITIZER_INSTALL_BASE_DIR "${CMAKE_SYSTEM_PROCESSOR}-linux")
# Fixed path for user. Only store npu-check of shell form to call real npu-check in internal dir
set(NPU_SANITIZER_INSTALL_BINDIR "${NPU_SANITIZER_INSTALL_BASE_DIR}/bin")
# Internal directory to store lib / bin
set(NPU_SANITIZER_INSTALL_INTERNAL_DIR "${NPU_SANITIZER_INSTALL_BASE_DIR}/tools/npu_tools")

if(ENABLE_TEST AND COMMAND add_cann_third_party)
  add_cann_third_party(gtest)
endif()

add_subdirectory(common)
add_subdirectory(sanitizer_api)
add_subdirectory(npu_check_cli)
add_subdirectory(npu_check)

if(DEFINED NPU_TOOLS_LIBRARY_OUTPUT_DIR AND DEFINED NPU_TOOLS_RUNTIME_OUTPUT_DIR)
  set(NPU_SANITIZER_LIBRARY_OUTPUT_DIR "${NPU_TOOLS_LIBRARY_OUTPUT_DIR}")
  set(NPU_SANITIZER_RUNTIME_OUTPUT_DIR "${NPU_TOOLS_RUNTIME_OUTPUT_DIR}")
else()
  set(NPU_SANITIZER_LIBRARY_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/lib64")
  set(NPU_SANITIZER_RUNTIME_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/bin")
endif()

foreach(target IN ITEMS acl_san npu_check)
  set_target_properties(${target} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${NPU_SANITIZER_LIBRARY_OUTPUT_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${NPU_SANITIZER_LIBRARY_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${NPU_SANITIZER_RUNTIME_OUTPUT_DIR}"
  )
endforeach()
set_target_properties(npu_check_cli PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${NPU_SANITIZER_RUNTIME_OUTPUT_DIR}"
)

# binary: npu-check                     path: tools/npu_tools/bin/npu-check
install(TARGETS npu_check_cli
  RUNTIME DESTINATION "${NPU_SANITIZER_INSTALL_INTERNAL_DIR}/bin" ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
)

# libacl_san.so + libnpu_check.so       path: tools/npu_tools/lib64/**.so
install(TARGETS npu_check acl_san
  LIBRARY DESTINATION "${NPU_SANITIZER_INSTALL_INTERNAL_DIR}/lib64" ${INSTALL_OPTIONAL}
  COMPONENT asc-tools
)

# shell wrapper                         path: bin/npu-check => calls tools/npu_tools/bin/npu-check
install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/npu_check_cli/npu_check.sh"
  DESTINATION "${NPU_SANITIZER_INSTALL_BINDIR}" ${INSTALL_OPTIONAL}
  PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
  RENAME npu-check
  COMPONENT asc-tools
)
