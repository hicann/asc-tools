# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

if (pvmodel_ascend910_FOUND)
    message(STATUS "Package pvmodel has been found.")
    return()
endif()

set(_cmake_targets_defined "")
set(_cmake_targets_not_defined "")
set(_cmake_expected_targets "")
foreach(_cmake_expected_target IN ITEMS pvmodel_ascend910 pvmodel_ascend310p pem_davinci_ascend910B1 pem_davinci_ascend310B pem_davinci_ascend910_9599 pem_davinci_kirinx90 pem_davinci_kirin9030)
    list(APPEND _cmake_expected_targets "${_cmake_expected_target}")
    if(TARGET "${_cmake_expected_target}")
        list(APPEND _cmake_targets_defined "${_cmake_expected_target}")
    else()
        list(APPEND _cmake_targets_not_defined "${_cmake_expected_target}")
    endif()
endforeach()
unset(_cmake_expected_target)

if(_cmake_targets_defined STREQUAL _cmake_expected_targets)
    unset(_cmake_targets_defined)
    unset(_cmake_targets_not_defined)
    unset(_cmake_expected_targets)
    unset(CMAKE_IMPORT_FILE_VERSION)
    cmake_policy(POP)
    return()
endif()

if(NOT _cmake_targets_defined STREQUAL "")
    string(REPLACE ";" ", " _cmake_targets_defined_text "${_cmake_targets_defined}")
    string(REPLACE ";" ", " _cmake_targets_not_defined_text "${_cmake_targets_not_defined}")
    message(FATAL_ERROR "Some (but not all) targets in this export set were already defined.\nTargets Defined: ${_cmake_targets_defined_text}\nTargets not yet defined: ${_cmake_targets_not_defined_text}\n")
endif()
unset(_cmake_targets_defined)
unset(_cmake_targets_not_defined)
unset(_cmake_expected_targets)

find_library(ascend910_LIBRARY
    NAMES lib_pvmodel.so
    PATHS ${PVMODEL_PATH}/lib64/Ascend910A/lib
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH)

find_library(ascend310p_LIBRARY
    NAMES lib_pvmodel.so
    PATHS ${PVMODEL_PATH}/lib64/Ascend310P1/lib
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH)

find_library(ascend910B1_LIBRARY
    NAMES libpem_davinci.so
    PATHS ${PVMODEL_PATH}/lib64/Ascend910B1/lib
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH)

find_library(ascend310B1_LIBRARY
    NAMES libpem_davinci.so
    PATHS ${PVMODEL_PATH}/lib64/Ascend310B1/lib
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH)

find_library(ascend910_9599_LIBRARY
    NAMES libpem_davinci.so
    PATHS ${PVMODEL_PATH}/lib64/Ascend910_9599/lib
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH)

find_library(kirinx90_LIBRARY
    NAMES libpem_davinci.so
    PATHS ${PVMODEL_PATH}/lib64/KirinX90/lib
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH)

find_library(kirin9030_LIBRARY
    NAMES libpem_davinci.so
    PATHS ${PVMODEL_PATH}/lib64/Kirin9030/lib
    NO_CMAKE_SYSTEM_PATH
    NO_CMAKE_FIND_ROOT_PATH)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(pvmodel_ascend910
    FOUND_VAR
    pvmodel_ascend910_FOUND
    REQUIRED_VARS
        ascend910_LIBRARY
)

if(pvmodel_ascend910_FOUND)
    message(STATUS "Variables in pvmodel module:")
    cmake_print_variables(ascend910_LIBRARY)
    cmake_print_variables(ascend310p_LIBRARY)
    cmake_print_variables(ascend910B1_LIBRARY)
    cmake_print_variables(ascend310B1_LIBRARY)
    cmake_print_variables(ascend910_9599_LIBRARY)
    cmake_print_variables(kirinx90_LIBRARY)
    cmake_print_variables(kirin9030_LIBRARY)

    add_library(pvmodel_ascend910 SHARED IMPORTED)
    set_target_properties(pvmodel_ascend910 PROPERTIES
        IMPORTED_LOCATION "${ascend910_LIBRARY}"
    )

    add_library(pvmodel_ascend310p SHARED IMPORTED)
    set_target_properties(pvmodel_ascend310p PROPERTIES
        IMPORTED_LOCATION "${ascend310p_LIBRARY}"
    )

    add_library(pem_davinci_ascend910B1 SHARED IMPORTED)
    set_target_properties(pem_davinci_ascend910B1 PROPERTIES
        IMPORTED_LOCATION "${ascend910B1_LIBRARY}"
    )

    add_library(pem_davinci_ascend310B SHARED IMPORTED)
    set_target_properties(pem_davinci_ascend310B PROPERTIES
        IMPORTED_LOCATION "${ascend310B1_LIBRARY}"
    )

    add_library(pem_davinci_ascend910_9599 SHARED IMPORTED)
    set_target_properties(pem_davinci_ascend910_9599 PROPERTIES
        IMPORTED_LOCATION "${ascend910_9599_LIBRARY}"
    )

    if (KIRIN_BUILD_CPU_DEBUG)
        add_library(pem_davinci_kirinx90 SHARED IMPORTED)
        set_target_properties(pem_davinci_kirinx90 PROPERTIES
            IMPORTED_LOCATION "${kirinx90_LIBRARY}"
        )

        add_library(pem_davinci_kirin9030 SHARED IMPORTED)
        set_target_properties(pem_davinci_kirin9030 PROPERTIES
            IMPORTED_LOCATION "${kirin9030_LIBRARY}"
        )
    endif()
endif()

# Cleanup temporary variables.
set(ascend910_LIBRARY)
set(ascend310p_LIBRARY)
set(ascend910B1_LIBRARY)
set(ascend310B1_LIBRARY)
set(ascend910_9599_LIBRARY)
set(kirinx90_LIBRARY)
set(kirin9030_LIBRARY)
