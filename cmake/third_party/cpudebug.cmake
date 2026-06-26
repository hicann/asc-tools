# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set(PKG_NAME "cpudebug_deps")
set(ASC_TOOLS_VERSION "9.0.0-beta.2")
string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message(STATUS "Detected architecture: x86_64")
    set(TAR_ARCH x86)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm")
    message(STATUS "Detected architecture: ARM64")
    set(TAR_ARCH aarch64)
else ()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif ()

if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libraries/lib AND NOT EXISTS ${CMAKE_SOURCE_DIR}/libraries/lib/cmake/targets-tikicpulib-${BUILD_TYPE_LOWER}.cmake)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_SOURCE_DIR}/libraries/lib
        COMMENT "Removing ${CMAKE_SOURCE_DIR}/libraries/lib directory..." 
    )
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/libraries/lib/include/stub_fun.h)
    set(CPUDEBUG_PKG_TYPES ${BUILD_TYPE_LOWER})
    if (BUILD_TYPE_LOWER STREQUAL "debug")
        list(APPEND CPUDEBUG_PKG_TYPES release)
    endif()

    unset(CPUDEBUG_PKG)
    foreach(CPUDEBUG_PKG_TYPE IN LISTS CPUDEBUG_PKG_TYPES)
        set(CPUDEBUG_PKG_NAME cann-asc-tools-cpudebug-deps-lib_${CPUDEBUG_PKG_TYPE}_${ASC_TOOLS_VERSION}_linux-${CMAKE_SYSTEM_PROCESSOR}.tar.gz)
        set(CPUDEBUG_PKG_PATH ${CANN_3RD_LIB_PATH}/${CPUDEBUG_PKG_NAME})
        if(EXISTS ${CPUDEBUG_PKG_PATH})
            set(CPUDEBUG_PKG ${CPUDEBUG_PKG_PATH})
            message(STATUS "Using local cpudebug pkg: ${CPUDEBUG_PKG}")
            break()
        endif()

        if(CPUDEBUG_PKG_TYPE STREQUAL "debug")
            message(STATUS "Local debug cpudebug pkg not found in ${CANN_3RD_LIB_PATH}, fallback to release cpudebug pkg.")
            continue()
        endif()

        set(CPUDEBUG_PKG_URL "https://cann-3rd.obs.cn-north-4.myhuaweicloud.com/cann/asc-tools-cpudebug/${ASC_TOOLS_VERSION}/${CPUDEBUG_PKG_NAME}")
        message(STATUS "cpudebug pkg not found in ${CANN_3RD_LIB_PATH}, downloading from ${CPUDEBUG_PKG_URL}")
        file(DOWNLOAD ${CPUDEBUG_PKG_URL} ${CPUDEBUG_PKG_PATH}
            STATUS CPUDEBUG_DOWNLOAD_STATUS
            TLS_VERIFY FALSE
            SHOW_PROGRESS
        )
        list(GET CPUDEBUG_DOWNLOAD_STATUS 0 CPUDEBUG_DOWNLOAD_CODE)
        if(CPUDEBUG_DOWNLOAD_CODE EQUAL 0)
            set(CPUDEBUG_PKG ${CPUDEBUG_PKG_PATH})
            break()
        endif()

        file(REMOVE ${CPUDEBUG_PKG_PATH})
        list(GET CPUDEBUG_DOWNLOAD_STATUS 1 CPUDEBUG_DOWNLOAD_MSG)
        message(WARNING "Download ${CPUDEBUG_PKG_NAME} failed: ${CPUDEBUG_DOWNLOAD_MSG}")
    endforeach()

    if(NOT CPUDEBUG_PKG)
        message(FATAL_ERROR "Failed to get cpudebug deps from ${CANN_3RD_LIB_PATH}.")
    endif()

    execute_process(
        COMMAND tar -xf ${CPUDEBUG_PKG} -C ${CMAKE_SOURCE_DIR}/libraries --strip-components 1
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/libraries
        RESULT_VARIABLE result
    )
endif()
