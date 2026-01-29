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
project(opbuild)

add_executable(op_build op_build.cpp op_generator.cpp op_generator_factory.cpp op_cfg_generator.cpp
  op_proto_generator.cpp op_aclnn_generator.cpp op_custom_registry_generator.cpp op_aclnn_fallback_generator.cpp
  op_cpu_cfg_generator.cpp
  op_build_params.cpp
)

target_compile_options(op_build PRIVATE -Wextra -Wfloat-equal -fno-common -fpie -Werror)

if(BUILD_OPEN_PROJECT)
  target_include_directories(op_build PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/pkg_inc/base
    ${ASCEND_CANN_PACKAGE_PATH}/include
    ${ASCEND_CANN_PACKAGE_PATH}/include/base
    ${ASCEND_CANN_PACKAGE_PATH}/pkg_inc/base
    ${ASCEND_CANN_PACKAGE_PATH}/include/ge
    ${CMAKE_CURRENT_SOURCE_DIR}/common
  )
else()
  target_include_directories(op_build PRIVATE
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/abl/mmpa/inc
    ${TOP_DIR}/abl/slog/inc/toolchain
    ${TOP_DIR}/asc/asc-devkit/tools/build/common
  )
endif()

target_link_libraries(op_build PRIVATE
  $<BUILD_INTERFACE:intf_pub>
  $<BUILD_INTERFACE:mmpa_headers>
  metadef_headers
  graph
  graph_base
  register
  dl
  unified_dlog
  mmpa
  error_manager
)

target_link_options(op_build PRIVATE
    -pie
    $<$<CONFIG:Release>:-s>
)

install(TARGETS op_build
  RUNTIME DESTINATION lib OPTIONAL
)

if(NOT BUILD_OPEN_PROJECT)
    install_package(
    PACKAGE op_build
    TARGETS op_build
    )
endif()
