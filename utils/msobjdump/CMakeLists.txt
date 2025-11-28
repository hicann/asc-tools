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
project(msobjdump)

set(msobjdump_path ${CMAKE_CURRENT_SOURCE_DIR})

if(BUILD_OPEN_PROJECT)
    add_custom_target(msobjdump-0.1.0-py3-none-any.whl ALL
        COMMAND ${CMAKE_COMMAND} -E chdir ${msobjdump_path} ${HI_PYTHON} setup.py bdist_wheel
        COMMAND ${CMAKE_COMMAND} -E copy ${msobjdump_path}/dist/msobjdump*.whl ${CMAKE_INSTALL_PREFIX}/lib/msobjdump-0.1.0-py3-none-any.whl
        COMMAND ${CMAKE_COMMAND} -E copy ${msobjdump_path}/msobjdump.sh ${CMAKE_INSTALL_PREFIX}/lib/msobjdump
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${msobjdump_path}/msobjdump.egg-info ${msobjdump_path}/build ${msobjdump_path}/dist
    )

    install(FILES ${CMAKE_INSTALL_PREFIX}/lib/msobjdump
        OPTIONAL DESTINATION tools/msobjdump/
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    )
    install(FILES ${CMAKE_INSTALL_PREFIX}/lib/msobjdump-0.1.0-py3-none-any.whl
        OPTIONAL DESTINATION tools/
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    )
else()
    add_custom_target(msobjdump-0.1.0-py3-none-any.whl
        COMMAND ${CMAKE_COMMAND} -E chdir ${msobjdump_path} ${HI_PYTHON} setup.py bdist_wheel
        COMMAND ${CMAKE_COMMAND} -E copy ${msobjdump_path}/dist/msobjdump*.whl ${CMAKE_INSTALL_PREFIX}/lib/msobjdump-0.1.0-py3-none-any.whl
        COMMAND ${CMAKE_COMMAND} -E copy ${msobjdump_path}/msobjdump.sh ${CMAKE_INSTALL_PREFIX}/lib/msobjdump
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${msobjdump_path}/msobjdump.egg-info ${msobjdump_path}/build ${msobjdump_path}/dist
        COMMAND echo "msobjdump-0.1.0-py3-none-any.whl is ok!"
    )


    install(FILES ${CMAKE_INSTALL_PREFIX}/lib/msobjdump
        OPTIONAL DESTINATION lib
        PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    )
endif()
