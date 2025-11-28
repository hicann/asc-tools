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
project(asc-tools)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

if(BUILD_OPEN_PROJECT)
    find_package(Python3 COMPONENTS Interpreter)
    set(HI_PYTHON ${Python3_EXECUTABLE})

    include(cmake/dependencies.cmake)
    include(cmake/intf_pub_linux.cmake)
    include(cmake/third_party/pvmodel.cmake)
    include(cmake/third_party/cpudebug.cmake)
    include(cmake/package.cmake)
endif()

add_subdirectory(cpudebug)
add_subdirectory(npuchk)
add_subdirectory(utils/msobjdump)
add_subdirectory(utils/show_kernel_debug_data)
add_subdirectory(utils/opbuild)
if(ENABLE_TEST)
  add_subdirectory(test)
endif()