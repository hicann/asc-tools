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
project(ascendc_py_ut)

message(STATUS "tikcpp_python_ut compile start")

run_python_llt_test(
        TARGET tikcpp_utest_msobjdump
        SRC_FILES_DIR ${ASCENDC_TOOLS_ROOT_DIR}/utils/msobjdump/msobjdump/
        TEST_FILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/testcase/msobjdump/
        EXPORT_PYTHONPATH ${CMAKE_CURRENT_SOURCE_DIR}/testcase/stub/
        PRE_TEST_COMMAND
        COVERAGERC_DIR
)

run_python_llt_test(
        TARGET tikcpp_utest_show_kernel_debug_data
        SRC_FILES_DIR ${ASCENDC_TOOLS_ROOT_DIR}/utils/show_kernel_debug_data/show_kernel_debug_data
        TEST_FILES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/testcase/show_kernel_debug_data
        EXPORT_PYTHONPATH ${CMAKE_CURRENT_SOURCE_DIR}/testcase/stub/
        PRE_TEST_COMMAND
        COVERAGERC_DIR
)

if(NOT DEFINED PYTHON_PYVERSION)
    set(PYTHON_PYVERSION python3)
endif()

add_custom_target(
    python_llt_coverage
    ALL
    COMMAND echo "Start generating the LLT coverage rate for Python."
    COMMAND ${PYTHON_PYVERSION} -m coverage combine .coverage.tikcpp_utest_show_kernel_debug_data .coverage.tikcpp_utest_msobjdump
    COMMAND ${PYTHON_PYVERSION} -m coverage html --data-file=".coverage"
    DEPENDS tikcpp_utest_msobjdump tikcpp_utest_show_kernel_debug_data
    WORKING_DIRECTORY ${ASCENDC_TOOLS_ROOT_DIR}/build_out/coverage_result/
    COMMENT "Succeed to generating the LLT coverage rate for Python."
)
