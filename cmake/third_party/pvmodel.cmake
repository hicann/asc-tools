# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

if(pvmodel_ascend910_FOUND)
    message(STATUS "pvmodel found in ${PVMODEL_PATH}")
else()
    set(PVMODEL_NAME "pvmodel")
    file(GLOB SIMULATOR_PKG
        LIST_DIRECTORIES True
        ${DEPS_FILE_PATH}/simulator*.tar.gz
    )

    if(NOT EXISTS ${SIMULATOR_PKG})
        set(SIMULATOR_FILE simulator_8.5.0.alpha001_linux-${CMAKE_SYSTEM_PROCESSOR}.tar.gz)
        set(SIMULATOR_URL "https://mirrors.huaweicloud.com/artifactory/cann-run/8.5.0.alpha001/try/${CMAKE_SYSTEM_PROCESSOR}/${SIMULATOR_FILE}")
        message(STATUS "Pvmodel pkg not found in ${DEPS_FILE_PATH}, downloading pvmodel from ${SIMULATOR_URL}")
    else()
        set(SIMULATOR_URL ${SIMULATOR_PKG})
    endif()

    set(SIMULATOR_DIR ${DEPS_FILE_PATH}/${SIMULATOR_FILE})
    include(FetchContent)
    FetchContent_Declare(
        ${PVMODEL_NAME}
        URL ${SIMULATOR_URL}
        TLS_VERIFY FALSE
        DOWNLOAD_DIR ${DEPS_FILE_PATH}
        SOURCE_DIR ${PVMODEL_PATH}
    )
    FetchContent_MakeAvailable(${PVMODEL_NAME})

    find_package(pvmodel MODULE)
    if (NOT pvmodel_ascend910_FOUND)
        message(FATAL_ERROR "pvmodel not found, please check if the simulator is complete.")
    endif()
endif()

install(DIRECTORY ${PVMODEL_PATH}
    DESTINATION tools
)
