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
project(op_project_templates)

if(BUILD_OPEN_PROJECT)
    set(MAKESELF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/op_project_templates/ascendc/customize/cmake/util/makeself)

    add_custom_target(ascendc_project_templates ALL
        COMMAND mkdir -p ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/COPYING ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/Makefile ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/makeself.1 ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/*.sh ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/README.md ${MAKESELF_PATH}
        COMMAND ${CMAKE_COMMAND} -E copy ${CANN_3RD_LIB_PATH}/makeself/VERSION ${MAKESELF_PATH}
    )
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/op_project_templates
        OPTIONAL DESTINATION tools/
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/new_op_project_template
        OPTIONAL DESTINATION tools/
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
endif()
