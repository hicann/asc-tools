# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software; you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16.0)

get_filename_component(ASC_TOOLS_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
string(RANDOM LENGTH 8 ALPHABET 0123456789abcdef TEST_SUFFIX)
set(TEST_ROOT "/tmp/asc_tools_mockcpp_patch_${TEST_SUFFIX}")
set(TEST_THIRD_PARTY_DIR "${TEST_ROOT}/opensource")
set(TEST_SOURCE_DIR "${TEST_ROOT}/source")
set(TEST_BUILD_DIR "${TEST_ROOT}/build")
set(TEST_PATCH_CONTENT "mockcpp patch fixture\n")

file(MAKE_DIRECTORY "${TEST_THIRD_PARTY_DIR}" "${TEST_SOURCE_DIR}")
file(WRITE "${TEST_THIRD_PARTY_DIR}/mockcpp-2.7-h5.patch" "${TEST_PATCH_CONTENT}")
file(WRITE "${TEST_SOURCE_DIR}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.16.0)
project(mockcpp_patch_test NONE)

set(ASCENDC_TOOLS_ROOT_DIR "@ASC_TOOLS_ROOT@")
set(CANN_3RD_LIB_PATH "@TEST_THIRD_PARTY_DIR@")
set(CMAKE_INSTALL_PREFIX "@TEST_ROOT@/install")
set(third_party_TEM_DIR "@TEST_ROOT@/tmp")
set(BOOST_PATH "@TEST_ROOT@/boost")

include("@ASC_TOOLS_ROOT@/tests/third_party/mockcpp.cmake")
]=])

file(READ "${TEST_SOURCE_DIR}/CMakeLists.txt" TEST_CMAKELISTS)
string(CONFIGURE "${TEST_CMAKELISTS}" TEST_CMAKELISTS @ONLY)
file(WRITE "${TEST_SOURCE_DIR}/CMakeLists.txt" "${TEST_CMAKELISTS}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${TEST_SOURCE_DIR}" -B "${TEST_BUILD_DIR}"
    RESULT_VARIABLE CONFIGURE_RESULT
    OUTPUT_VARIABLE CONFIGURE_OUTPUT
    ERROR_VARIABLE CONFIGURE_ERROR
)

if(NOT CONFIGURE_RESULT EQUAL 0)
    file(REMOVE_RECURSE "${TEST_ROOT}")
    message(FATAL_ERROR "mockcpp CMake configure failed:\n${CONFIGURE_OUTPUT}\n${CONFIGURE_ERROR}")
endif()

set(STAGED_PATCH "${TEST_ROOT}/tmp/mockcpp-2.7_py3.patch")
if(NOT EXISTS "${STAGED_PATCH}")
    file(REMOVE_RECURSE "${TEST_ROOT}")
    message(FATAL_ERROR "Expected staged patch does not exist: ${STAGED_PATCH}")
endif()

file(READ "${STAGED_PATCH}" STAGED_PATCH_CONTENT)
file(REMOVE_RECURSE "${TEST_ROOT}")
if(NOT STAGED_PATCH_CONTENT STREQUAL TEST_PATCH_CONTENT)
    message(FATAL_ERROR "Staged patch content differs from the selected local patch")
endif()
