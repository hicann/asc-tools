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
project(ascendc_ut)

set(TIKCPP_PRODUCT_TYPE_LIST ascend910 ascend610 ascend310p ascend910B1_AIC ascend910B1_AIV ascend310B1 ascend610Lite)

set(TIKICPULIB_PRODUCT_TYPE_LIST ascend910 ascend610 ascend310p ascend910B1_AIC ascend910B1_AIV)

add_compile_definitions(ASCENDC_CPU_DEBUG=1 ASCENDC_DUMP=0)

message(STATUS "tikcpp_ut_test compile start")
set(CMAKE_CXX_STANDARD 17)

set(LOCAL_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ASCENDC_SYS_DIR ${ASCEND_CANN_PACKAGE_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux)

# include(${ASCENDC_TOOLS_ROOT_DIR}/cmake/dependencies.cmake)

####################################################################
####################################################################
set(ASCENDC_TEST_HEADER_FILES
    ${LOCAL_DIR}/common/
    ${LOCAL_DIR}/testcase/
    ${ASCENDC_SYS_DIR}/tikcpp/tikcfw
    ${ASCENDC_SYS_DIR}/tikcpp/tikcfw/interface
    ${ASCENDC_SYS_DIR}/tikcpp/tikcfw/impl
    ${ASCENDC_TOOLS_ROOT_DIR}/cpudebug
    ${ASCENDC_TOOLS_ROOT_DIR}/cpudebug/include/
    ${ASCENDC_TOOLS_ROOT_DIR}/cpudebug/src/
    ${ASCENDC_TOOLS_ROOT_DIR}/cpudebug/src/api_check
    ${ASCENDC_TOOLS_ROOT_DIR}/cpudebug/src/api_check/inc
    ${CMAKE_CURRENT_BINARY_DIR}/stub
    ${ASCENDC_TOOLS_ROOT_DIR}/libraries/lib/include
)

# common ut src files
file(GLOB ASCENDC_TEST_COMMON_SRC_FILES
    ${LOCAL_DIR}/common/common.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/cpudebug/src/regfwk/stub_base.cpp
)

# common test cases
file(GLOB ASCENDC_TEST_COMMON_CASE_SRC_FILES
    ${LOCAL_DIR}/testcase/tikcpp_case_common/*.cpp
)

file(GLOB ASCENDC_SRC_FILES
    ${ASCENDC_TOOLS_ROOT_DIR}/cpudebug/src/api_check/*.cpp
)

file(GLOB ASCENDC_API_CHECK_CASE_SRC_FILES
    ${LOCAL_DIR}/testcase/tikcpp_api_check/*.cpp
)

foreach(product_type ${TIKCPP_PRODUCT_TYPE_LIST})
    if (${product_type} STREQUAL "ascend910B1_AIC" OR ${product_type} STREQUAL "ascend910B1_AIV")
        set(deps_product_type "ascend910B1")
    else()
        set(deps_product_type ${product_type})
    endif()

    add_executable(tikcpp_utest_${product_type}
        ${ASCENDC_SRC_FILES}
        ${LOCAL_DIR}/main_global.cpp
        ${ASCENDC_TEST_COMMON_SRC_FILES}
        $<$<STREQUAL:${product_type},ascend910>:${ASCENDC_TEST_COMMON_CASE_SRC_FILES};${ASCENDC_API_CHECK_CASE_SRC_FILES}>
        $<$<STREQUAL:${product_type},ascend610>:${ASCENDC_TEST_COMMON_CASE_SRC_FILES};${ASCENDC_API_CHECK_CASE_SRC_FILES}>
        $<$<STREQUAL:${product_type},ascend310p>:${ASCENDC_TEST_COMMON_CASE_SRC_FILES};${ASCENDC_API_CHECK_CASE_SRC_FILES}>
        $<$<STREQUAL:${product_type},ascend910B1_AIC>:${ASCENDC_API_CHECK_CASE_SRC_FILES}>
        $<$<STREQUAL:${product_type},ascend910B1_AIV>:${ASCENDC_API_CHECK_CASE_SRC_FILES}>
        ${LOCAL_DIR}/common/dlog_stub.cpp
        $<$<STREQUAL:${product_type},ascend910>:${LOCAL_DIR}/common/tik_pv_wrapper.cpp;${LOCAL_DIR}/common/alog_stub.cpp>
        $<$<STREQUAL:${product_type},ascend610>:${LOCAL_DIR}/common/tik_pv_wrapper.cpp;${LOCAL_DIR}/common/alog_stub.cpp>
        $<$<STREQUAL:${product_type},ascend310p>:${LOCAL_DIR}/common/tik_pv_wrapper.cpp;${LOCAL_DIR}/common/alog_stub.cpp>
        $<$<STREQUAL:${product_type},ascend910B1_AIC>:${LOCAL_DIR}/common/k3_pvwrap.cpp>
        $<$<STREQUAL:${product_type},ascend910B1_AIV>:${LOCAL_DIR}/common/k3_pvwrap.cpp>
        $<$<STREQUAL:${product_type},ascend310B1>:${LOCAL_DIR}/common/k3_pvwrap.cpp>
        $<$<STREQUAL:${product_type},ascend610Lite>:${LOCAL_DIR}/common/k3_pvwrap.cpp>
        ${ASCENDC_TEST_${product_type}_CASE_SRC_FILES}
    )

    add_dependencies(tikcpp_utest_${product_type} prepare_target_ut mockcpp_static cpudebug_${deps_product_type})
    # add soc version flags
    if(${product_type} STREQUAL "ascend910")
        target_compile_definitions(tikcpp_utest_${product_type} PRIVATE
            UT_TEST
            __CCE_AICORE__=100
            __NPU_ARCH__=1001
            __DAV_C100__
            __UT_CHIP_VER__=910
    )
    elseif(${product_type} STREQUAL "ascend610")
        target_compile_definitions(tikcpp_utest_${product_type} PRIVATE
            UT_TEST
            __CCE_AICORE__=200
            __NPU_ARCH__=2002
            __DAV_M200__
            __UT_CHIP_VER__=610
        )
    elseif(${product_type} STREQUAL "ascend310p")
        target_compile_definitions(tikcpp_utest_${product_type} PRIVATE
            UT_TEST
            __CCE_AICORE__=200
            __NPU_ARCH__=2002
            __DAV_M200__
            __UT_CHIP_VER__=710
        )
    elseif(${product_type} STREQUAL "ascend910B1_AIC")
        target_compile_definitions(tikcpp_utest_${product_type} PRIVATE
            UT_TEST
            __CCE_AICORE__=220
            __NPU_ARCH__=2201
            __DAV_C220__
            __DAV_C220_CUBE__
        )
    elseif(${product_type} STREQUAL "ascend910B1_AIV")
        target_compile_definitions(tikcpp_utest_${product_type} PRIVATE
            UT_TEST
            __CCE_AICORE__=220
            __NPU_ARCH__=2201
            __DAV_C220__
            __DAV_C220_VEC__
        )
    elseif(${product_type} STREQUAL "ascend310B1")
        target_compile_definitions(tikcpp_utest_${product_type} PRIVATE
            UT_TEST
            __CCE_AICORE__=300
            __NPU_ARCH__=3002
            __DAV_M300__
            __UT_CHIP_VER__=300
        )
    elseif(${product_type} STREQUAL "ascend610Lite")
        target_compile_definitions(tikcpp_utest_${product_type} PRIVATE
            UT_TEST
            __CCE_AICORE__=310
            __DAV_M310__
            __NPU_ARCH__=3102
            __UT_CHIP_VER__=310
        )
    endif()

    target_compile_definitions(tikcpp_utest_${product_type} PRIVATE ASCENDC_CPU_DEBUG=1
                                                                    ASCENDC_OOM=1)


    target_include_directories(tikcpp_utest_${product_type} PRIVATE
        ${ASCENDC_TEST_HEADER_FILES}
        ${ASCENDC_SYS_DIR}/include/base/
        ${ASCENDC_SYS_DIR}/include/experiment/metadef/
        mmpa_headers
        slog_headers
    )

    target_compile_options(tikcpp_utest_${product_type} PRIVATE
        -g
        -fno-access-control
        -DCMAKE_C_COMPILER_LAUNCHER=${CCACHE_PROGRAM}
        -DCMAKE_CXX_COMPILER_LAUNCHER=${CCACHE_PROGRAM}
    )

    target_link_directories(tikcpp_utest_${product_type} PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/../../cpudebug/${deps_product_type}
        ${ASCENDC_TOOLS_ROOT_DIR}/libraries/lib
    )

    target_link_libraries(tikcpp_utest_${product_type} PRIVATE
        $<BUILD_INTERFACE:intf_llt_pub>
        $<BUILD_INTERFACE:mmpa_headers>
        -Wl,--no-as-needed
        c_sec
        mmpa
        error_manager
        tikcpp_debug
        tikicpulib_cceprint
        tikicpulib_npuchk
        tikicpulib_stubreg
        -Wl,--as-needed
    )

    run_llt_test(
        TARGET tikcpp_utest_${product_type}
        TASK_NUM 1
    )
endforeach()

set(src_file ${ASCENDC_TOOLS_ROOT_DIR}/libraries/lib/include/stub_fun.h)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/stub/stub_fun.h
    COMMAND ${LOCAL_DIR}/../cmake/tools/prepare.sh ${src_file} ${CMAKE_CURRENT_BINARY_DIR}/stub
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(prepare_target_ut ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/stub/stub_fun.h
)

# ###################################opbuild ut#################################
add_library(ops_utest SHARED ${LOCAL_DIR}/testcase/opbuild/test_ops.cpp)

target_include_directories(ops_utest PRIVATE
    ${ASCENDC_SYS_DIR}/include
    ${ASCENDC_TEST_HEADER_FILES}
    ${ASCENDC_SYS_DIR}/include/base/
    ${ASCENDC_SYS_DIR}/include/experiment/metadef/
    ${ASCENDC_SYS_DIR}/include/experiment/
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/common/
    ${ASCEND_HOME_PATH}/tools/msopgen/template/custom_operator_sample/DSL/Onnx/metadef/inc
    mmpa_headers
    slog_headers
)

target_link_directories(ops_utest PRIVATE
    ${ASCEND_DEP_LIB_DIR}
)

target_link_libraries(ops_utest PRIVATE
    # $<BUILD_INTERFACE:metadef_headers>
    -Wl,--no-as-needed
    c_sec graph graph_base register
    -Wl,--as-needed
)

add_custom_target(utest_env
    COMMAND echo "export OPS_DSO_FILE_PATH=$<TARGET_FILE:ops_utest>" > ${CMAKE_CURRENT_BINARY_DIR}/opbuild_env.sh
    COMMAND echo "export OPS_SRC_FILE_PATH=${LOCAL_DIR}/testcase/opbuild" >> ${CMAKE_CURRENT_BINARY_DIR}/opbuild_env.sh
)

add_executable(tikcpp_utest_opbuild
    ${LOCAL_DIR}/main.cpp
    ${LOCAL_DIR}/testcase/opbuild/test_opbuild.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/op_build.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/op_build_params.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/op_generator.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/op_generator_factory.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/op_cfg_generator.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/op_proto_generator.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/op_aclnn_generator.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/op_custom_registry_generator.cpp
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/op_aclnn_fallback_generator.cpp
)

add_dependencies(tikcpp_utest_opbuild ops_utest utest_env)

target_compile_definitions(tikcpp_utest_opbuild PRIVATE
    UT_TEST
)

target_compile_options(tikcpp_utest_opbuild PRIVATE
    -g
    -Werror
)

target_include_directories(tikcpp_utest_opbuild PRIVATE
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild
    ${ASCENDC_TOOLS_ROOT_DIR}/utils/opbuild/common/
    ${ASCENDC_TEST_HEADER_FILES}
    ${ASCENDC_SYS_DIR}/include/base/
    ${ASCENDC_SYS_DIR}/include/experiment/metadef/
    ${ASCENDC_SYS_DIR}/include/experiment/
    ${ASCENDC_SYS_DIR}/include
    mmpa_headers
    slog_headers
    ${ASCEND_HOME_PATH}/tools/msopgen/template/custom_operator_sample/DSL/Onnx/metadef/inc
)

target_link_directories(tikcpp_utest_opbuild PRIVATE
    ${ASCEND_DEP_LIB_DIR}
)

target_link_libraries(tikcpp_utest_opbuild PRIVATE
    $<BUILD_INTERFACE:intf_llt_pub>
    # $<BUILD_INTERFACE:metadef_headers>
    -Wl,--no-as-needed
    c_sec graph graph_base register dl
    -Wl,--as-needed
)

run_llt_test(
    TARGET tikcpp_utest_opbuild
    ENV_FILE ${CMAKE_CURRENT_BINARY_DIR}/opbuild_env.sh
    TASK_NUM 1
)