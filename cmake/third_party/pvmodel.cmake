# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set(PVMODEL_PATH ${PROJECT_SOURCE_DIR}/libraries/simulator)
message(STATUS "the PVMODEL_PATH at :${PVMODEL_PATH}")
if(pvmodel_ascend910_FOUND)
    message(STATUS "pvmodel found in ${PVMODEL_PATH}")
else()
    set(PVMODEL_NAME "pvmodel")
    file(GLOB SIMULATOR_PKG
            LIST_DIRECTORIES True
            ${CANN_3RD_LIB_PATH}/simulator*.tar.gz
    )

    if(NOT EXISTS ${SIMULATOR_PKG})
         set(SIMULATOR_FILE simulator_9.0.0_linux-${CMAKE_SYSTEM_PROCESSOR}.tar.gz)
         set(SIMULATOR_URL "https://mirrors.huaweicloud.com/artifactory/cann-run/9.0.0/try/${CMAKE_SYSTEM_PROCESSOR}/${SIMULATOR_FILE}")
        message(STATUS "Pvmodel pkg not found in ${CANN_3RD_LIB_PATH}, ready to download pvmodel from ${SIMULATOR_URL}")
        set(DOWNLOAD_SIM ON) # 使用下载tar包的方式获取simulator
    else()
        set(SIMULATOR_URL ${SIMULATOR_PKG})
        set(DOWNLOAD_SIM OFF)
    endif()

    set(SIMULATOR_DIR ${CANN_3RD_LIB_PATH}/${SIMULATOR_FILE})
    find_program(CURL curl)
    if (DOWNLOAD_SIM AND CURL)
        execute_process(
                COMMAND ${CURL} --output /dev/null --silent --head --fail --max-time 10 ${SIMULATOR_URL}
                RESULT_VARIABLE curl_ok
        )
        message(STATUS "curl simulator result:${curl_ok}")
        if(curl_ok EQUAL 0)
            message("sim model is available")
            set(DOWNLOAD_SIM_AVAILABLE ON) # 网络通常，可以下载
        endif()
    endif ()
    if ((DOWNLOAD_SIM AND DOWNLOAD_SIM_AVAILABLE) OR EXISTS ${SIMULATOR_PKG})
        message(STATUS "start to Fetch sim model with DOWNLOAD_SIM=${DOWNLOAD_SIM} and DOWNLOAD_SIM_AVAILABLE=${DOWNLOAD_SIM_AVAILABLE}")
        include(FetchContent)
        FetchContent_Declare(
                ${PVMODEL_NAME}
                URL ${SIMULATOR_URL}
                TLS_VERIFY FALSE
                DOWNLOAD_DIR ${CANN_3RD_LIB_PATH}
                SOURCE_DIR ${PVMODEL_PATH}
        )
        FetchContent_MakeAvailable(${PVMODEL_NAME})
        message(STATUS "success to download :${SIMULATOR_FILE}")
    else ()
        #无法获取，使用cann-simulator安装路径下的
        set(PVMODEL_PATH ${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/simulator)
        message(STATUS "reset PVMODEL_PATH to :${PVMODEL_PATH}")
    endif ()
endif()

install(DIRECTORY ${PVMODEL_PATH}
    DESTINATION tools
)