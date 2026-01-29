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
project(ascendc_stubreg)

gen_intris(tar_intri
  ${CPULIB_GEN_DIR}/stub_fun.h
  ${CPULIB_GEN_DIR}/intri_fun.cc
  ${CPULIB_GEN_DIR}/intri_fmt.cc)
add_dependencies(tar_intri tar_stub)

file(GLOB REGFWK_FILFS
  stub_base.cpp
  stub_reg.cpp
  stub_backtrace.cpp
  kernel_print_lock.cpp
)

add_library(cpudebug_stubreg SHARED
  $<$<NOT:$<BOOL:${ENABLE_TAR}>>:${REGFWK_FILFS}>
  ${CPULIB_SRC_DIR}/model/cpulib/kernel_fp16.cpp
  ${CPULIB_GEN_DIR}/intri_fun.cc
  ${CPULIB_GEN_DIR}/intri_fmt.cc)
add_dependencies(cpudebug_stubreg tar_intri)

if(BUILD_OPEN_PROJECT)
  target_include_directories(cpudebug_stubreg PRIVATE
    ${CPULIB_SRC_DIR}/include
    ${CPULIB_SRC_DIR}/model/cpulib
    ${CPULIB_GEN_DIR}
    ${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/pkg_inc/base
    ${ASCEND_CANN_PACKAGE_PATH}/include
    ${ASCEND_CANN_PACKAGE_PATH}/include/base
  )
else()
  target_include_directories(cpudebug_stubreg PRIVATE
    ${CPULIB_SRC_DIR}/include
    ${CPULIB_SRC_DIR}/model/cpulib
    ${CPULIB_GEN_DIR}
    ${TOP_DIR}/abl/libc_sec/include
    ${TOP_DIR}/metadef/inc
    ${TOP_DIR}/abl/slog/inc/toolchain
    ${TOP_DIR}/metadef/inc/external
  )
endif()

target_link_libraries(cpudebug_stubreg PRIVATE
  $<BUILD_INTERFACE:intf_pub>
  $<BUILD_INTERFACE:tikcfw_headers>
  $<BUILD_INTERFACE:mmpa_headers>
  c_sec
  dl
)

target_link_options(cpudebug_stubreg PRIVATE
  $<$<CONFIG:Release>:-s>
)

if(BUILD_OPEN_PROJECT)
  install(TARGETS cpudebug_stubreg EXPORT targets-tikicpulib
    LIBRARY DESTINATION lib OPTIONAL)
else()
  install(TARGETS cpudebug_stubreg EXPORT targets-tikicpulib
    LIBRARY DESTINATION lib OPTIONAL)
endif()