# This program is free software, you can redistribute it and/or modify it.
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This file is a part of the CANN Open Software.
# Licensed under CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
cmake_minimum_required(VERSION 3.16.0)
project(ascendc_acl_stub)

add_library(ascendc_acl_stub SHARED
  ascendc_acl_stub.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../../model/cpulib/kernel_fp16.cpp
)

if(BUILD_OPEN_PROJECT)
  target_include_directories(ascendc_acl_stub PRIVATE
    ${ASCEND_CANN_PACKAGE_PATH}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  )
else()
  target_include_directories(ascendc_acl_stub PRIVATE
    ${TOP_DIR}/inc/external
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  )
endif()

target_compile_definitions(ascendc_acl_stub PRIVATE
  ASCENDC_CPU_DEBUG=1
)

target_link_libraries(ascendc_acl_stub PRIVATE
  $<BUILD_INTERFACE:intf_pub_cxx17>
  tikicpulib_stubreg
  c_sec
)

target_link_options(ascendc_acl_stub PRIVATE
    $<$<CONFIG:Release>:-s>
)

install(TARGETS ascendc_acl_stub
    LIBRARY DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
    ARCHIVE DESTINATION ${INSTALL_LIBRARY_DIR} OPTIONAL
    RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR} OPTIONAL
)