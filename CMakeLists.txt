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

add_compile_options(-Werror)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_LIST_DIR)
  message(STATUS "compile project asc-tools with library")
  option(BUILD_WITH_INSTALLED_DEPENDENCY_CANN_PKG "Build asc-tools with cann pkg" ON)
else()
  message(STATUS "compile project asc-tools with src")
  option(BUILD_WITH_INSTALLED_DEPENDENCY_CANN_PKG "Build asc-tools with cann source" OFF)
endif()

set(KIRIN_BUILD_CPU_DEBUG OFF)
set(CMAKE_C_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_RELEASE "")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

if(BUILD_OPEN_PROJECT)
    find_package(Python3 COMPONENTS Interpreter)
    set(HI_PYTHON ${Python3_EXECUTABLE})
    set(PYTHON ${Python3_EXECUTABLE})
    set(PVMODEL_PATH ${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/simulator)
    message(STATUS "the PVMODEL_PATH is :${PVMODEL_PATH}")

    include(cmake/dependencies.cmake) # 其中会根据PVMODEL_PATH作为pvmodel相关依赖路径
    include(cmake/intf_pub_linux.cmake)
    include(cmake/third_party/cpudebug.cmake)
    include(cmake/package.cmake)
endif()

set(ASC_TOOLS_VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/_CPack_Packages/makeself_staging/share/info/asc-tools/version.info)
add_custom_command(
    OUTPUT ${ASC_TOOLS_VERSION_FILE}
    COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/scripts/update_version_info/update_version_info.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/version.info
            ${ASC_TOOLS_VERSION_FILE}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/version.info
    VERBATIM
)

add_custom_target(generate_asc_tools_version_file ALL
    DEPENDS ${ASC_TOOLS_VERSION_FILE}
)
add_subdirectory(cpudebug)
add_subdirectory(npuchk)
add_subdirectory(utils/msobjdump)
add_subdirectory(utils/templates)
if(BUILD_OPEN_PROJECT AND NOT ENABLE_TEST)
  add_subdirectory(third_party)
endif()
add_subdirectory(utils/show_kernel_debug_data)
add_subdirectory(utils/opbuild)
if(ENABLE_TEST)
  add_subdirectory(tests)
endif()
