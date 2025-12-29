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
project(msopgen)

if(BUILD_OPEN_PROJECT)
    set(MSOPGEN_WHEEL_BUILD ${CMAKE_CURRENT_SOURCE_DIR}/build)
    set(MSOPGEN_WHEEL_OUT ${CMAKE_CURRENT_SOURCE_DIR}/output)
    set(MSOPGEN_WHEEL_IMAGE msopgen-1.0.0-py3-none-any.whl)
    set(MAKESELF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/op_project_templates/ascendc/customize/cmake/util/makeself)

    if (NOT DEFINED ASCEND_PYTHON_EXECUTABLE)
        set(ASCEND_PYTHON_EXECUTABLE python3 CACHE STRING "")
    endif()
    add_custom_target(${MSOPGEN_WHEEL_IMAGE} ALL
        COMMAND mkdir -p ${MAKESELF_PATH}
        COMMAND rm -rf ${MSOPGEN_WHEEL_OUT}
        COMMAND rm -rf ${MSOPGEN_WHEEL_BUILD}
        COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR} && ${ASCEND_PYTHON_EXECUTABLE} build.py
        COMMAND ${CMAKE_COMMAND} -E copy ${MSOPGEN_WHEEL_OUT}/${MSOPGEN_WHEEL_IMAGE} ${CMAKE_CURRENT_BINARY_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/COPYING ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/Makefile ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/makeself.1 ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/*.sh ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/README.md ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/VERSION ${MAKESELF_PATH}
        COMMAND rm -rf ${MSOPGEN_WHEEL_OUT}
        COMMAND rm -rf ${MSOPGEN_WHEEL_BUILD}
        COMMAND echo "${MSOPGEN_WHEEL_IMAGE} is ok!"
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/op_project_templates
        OPTIONAL DESTINATION tools/
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MSOPGEN_WHEEL_IMAGE}
        OPTIONAL DESTINATION tools/
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/msopgen
        OPTIONAL DESTINATION tools/
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )

endif()
