# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16.0)
project(optype_collector)

set(optype_collector_path ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_OPEN_PROJECT)
    add_custom_target(optype_collector-0.1.0-py3-none-any.whl ALL
        COMMAND ${CMAKE_COMMAND} -E chdir ${optype_collector_path} ${HI_PYTHON} setup.py bdist_wheel
        COMMAND ${CMAKE_COMMAND} -E copy ${optype_collector_path}/dist/optype_collector*.whl ${CMAKE_INSTALL_PREFIX}/lib/optype_collector-0.1.0-py3-none-any.whl
        COMMAND ${CMAKE_COMMAND} -E copy ${optype_collector_path}/optype_collector.sh ${CMAKE_INSTALL_PREFIX}/lib/optype_collector
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${optype_collector_path}/optype_collector.egg-info ${optype_collector_path}/build ${optype_collector_path}/dist
    )

    install(FILES ${CMAKE_INSTALL_PREFIX}/lib/optype_collector
        ${INSTALL_OPTIONAL} DESTINATION tools/optype_collector/
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
        COMPONENT asc-tools
    )
    install(FILES ${CMAKE_INSTALL_PREFIX}/lib/optype_collector-0.1.0-py3-none-any.whl
        ${INSTALL_OPTIONAL} DESTINATION tools/
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
        COMPONENT asc-tools
    )
else()
    add_custom_target(optype_collector-0.1.0-py3-none-any.whl
        COMMAND ${CMAKE_COMMAND} -E chdir ${optype_collector_path} ${HI_PYTHON} setup.py bdist_wheel
        COMMAND ${CMAKE_COMMAND} -E copy ${optype_collector_path}/dist/optype_collector*.whl ${CMAKE_INSTALL_PREFIX}/lib/optype_collector-0.1.0-py3-none-any.whl
        COMMAND ${CMAKE_COMMAND} -E copy ${optype_collector_path}/optype_collector.sh ${CMAKE_INSTALL_PREFIX}/lib/optype_collector
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${optype_collector_path}/optype_collector.egg-info ${optype_collector_path}/build ${optype_collector_path}/dist
        COMMAND echo "optype_collector-0.1.0-py3-none-any.whl is ok!"
    )

    install(FILES ${CMAKE_INSTALL_PREFIX}/lib/optype_collector
        ${INSTALL_OPTIONAL} DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
        COMPONENT asc-tools
    )
endif()
