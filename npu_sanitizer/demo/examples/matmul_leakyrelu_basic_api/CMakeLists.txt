# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------


cmake_minimum_required(VERSION 3.16)

set(CMAKE_ASC_RUN_MODE "npu" CACHE STRING "Run mode: npu, cpu, sim")
set(CMAKE_ASC_ARCHITECTURES "dav-2201" CACHE STRING "NPU architecture: dav-2201, dav-3510")

find_package(ASC REQUIRED)

project(aclsan_demo_matmul_leakyrelu_basic_api LANGUAGES ASC CXX)

find_library(ACLSAN_DEMO_ACL_RT_LIBRARY
    NAMES acl_rt
    HINTS "${ASCEND_CANN_PACKAGE_LINUX_PATH}/lib64"
    REQUIRED
)

add_executable(aclsan_demo_matmul_leakyrelu_basic_api
    matmul_leakyrelu_basic_api.asc
)

target_compile_options(aclsan_demo_matmul_leakyrelu_basic_api PRIVATE
    $<$<COMPILE_LANGUAGE:ASC>:--npu-arch=${CMAKE_ASC_ARCHITECTURES}>
)

target_link_libraries(aclsan_demo_matmul_leakyrelu_basic_api PRIVATE "${ACLSAN_DEMO_ACL_RT_LIBRARY}")
set_target_properties(aclsan_demo_matmul_leakyrelu_basic_api PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    BUILD_RPATH "${ASCEND_CANN_PACKAGE_LINUX_PATH}/lib64"
)
