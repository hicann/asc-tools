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
project(OpTestFrame)

if(BUILD_OPEN_PROJECT)
  set(CMAKE_CXX_FLAGS "-std=c++11 -Wl,-z,relro,-z,now,-z,noexecstack -s -fPIC -fPIE -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-all")
  add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)

  set(OP_TEST_FRAME_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  set(OP_TEST_FRAME_WHEEL_PACKAGE_REAL_NAME msopst-1.0.0-py3-none-any.whl)

  add_custom_target(OpTestFrameFiles ALL)

  add_custom_command(TARGET OpTestFrameFiles
    PRE_BUILD
    COMMAND echo ${OP_TEST_FRAME_SRC_DIR}
    COMMAND python3 ${OP_TEST_FRAME_SRC_DIR}/build.py
    COMMAND cp ${OP_TEST_FRAME_SRC_DIR}/output/* ${CMAKE_CURRENT_BINARY_DIR}
    COMMAND rm -rf ${OP_TEST_FRAME_SRC_DIR}/build
    COMMAND rm -rf ${OP_TEST_FRAME_WHEEL_PACKAGE_REAL_NAME}
    COMMAND mv ${OP_TEST_FRAME_SRC_DIR}/output/msopst-*.whl ${OP_TEST_FRAME_WHEEL_PACKAGE_REAL_NAME}
  )

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${OP_TEST_FRAME_WHEEL_PACKAGE_REAL_NAME}
    OPTIONAL DESTINATION tools/
    PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
  )

endif()
