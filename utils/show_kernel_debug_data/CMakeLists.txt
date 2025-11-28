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
project(show_kernel_debug_data)

set(ascendump_path ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_OPEN_PROJECT)
    add_custom_target(show_kernel_debug_data-0.1.0-py3-none-any.whl ALL
        COMMAND ${CMAKE_COMMAND} -E chdir ${ascendump_path} ${HI_PYTHON} setup.py bdist_wheel
        COMMAND ${CMAKE_COMMAND} -E copy ${ascendump_path}/dist/show_kernel_debug_data*.whl ${CMAKE_INSTALL_PREFIX}/lib/show_kernel_debug_data-0.1.0-py3-none-any.whl
        COMMAND ${CMAKE_COMMAND} -E copy ${ascendump_path}/show_kernel_debug_data.sh ${CMAKE_INSTALL_PREFIX}/lib/show_kernel_debug_data
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${ascendump_path}/show_kernel_debug_data.egg-info ${ascendump_path}/build ${ascendump_path}/dist
        COMMAND echo "show_kernel_debug_data-0.1.0-py3-none-any.whl is ok!"
    )
    install(FILES ${CMAKE_INSTALL_PREFIX}/lib/show_kernel_debug_data
        OPTIONAL DESTINATION tools/show_kernel_debug_data/
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    )
    install(FILES ${CMAKE_INSTALL_PREFIX}/lib/show_kernel_debug_data-0.1.0-py3-none-any.whl
        OPTIONAL DESTINATION tools/
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    )
else()
    add_custom_target(show_kernel_debug_data-0.1.0-py3-none-any.whl
        COMMAND ${CMAKE_COMMAND} -E chdir ${ascendump_path} ${HI_PYTHON} setup.py bdist_wheel
        COMMAND ${CMAKE_COMMAND} -E copy ${ascendump_path}/dist/show_kernel_debug_data*.whl ${CMAKE_INSTALL_PREFIX}/lib/show_kernel_debug_data-0.1.0-py3-none-any.whl
        COMMAND ${CMAKE_COMMAND} -E copy ${ascendump_path}/show_kernel_debug_data.sh ${CMAKE_INSTALL_PREFIX}/lib/show_kernel_debug_data
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${ascendump_path}/show_kernel_debug_data.egg-info ${ascendump_path}/build ${ascendump_path}/dist
        COMMAND echo "show_kernel_debug_data-0.1.0-py3-none-any.whl is ok!"
    )


    install(FILES ${CMAKE_INSTALL_PREFIX}/lib/show_kernel_debug_data
        OPTIONAL DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    )
endif()
