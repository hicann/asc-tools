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
project(tools_third_party)

set(MSOPGEN_WHEEL_IMAGE msopgen-1.0.0-py3-none-any.whl)
set(MSOPST_WHEEL_IMAGE msopst-1.0.0-py3-none-any.whl)

# Try hard to reuse an existing msot checkout before downloading.
set(_ms_root "")
set(_ms_candidates
    "${CANN_3RD_LIB_PATH}"
    "${CANN_3RD_LIB_PATH}/../mindstudio" # built with mirror layouts
    "${CMAKE_CURRENT_LIST_DIR}" # same as ${CANN_3RD_LIB_PATH} in default
)

foreach(_cand IN LISTS _ms_candidates)
    if(EXISTS "${_cand}/msot/cmake/module/msopgen.cmake")
        set(_ms_root "${_cand}")
        break()
    endif()
endforeach()

if(_ms_root)
    message(STATUS "Reusing existing msot at: ${_ms_root}")
else()
    message(STATUS "No reusable msot found, fetching into repo third_party...")
    set(_ms_root "${CMAKE_CURRENT_LIST_DIR}")

    set(SUB_ZIP_PREFIXES "mskl" "mskpp" "mssanitizer" "msopgen" "msdebug" "msopprof")
    set(ZIP_PREFIXES "msot" ${SUB_ZIP_PREFIXES})
    set(ALL_ZIP_EXIST TRUE)
    foreach(PREFIX ${ZIP_PREFIXES})
        set(ZIP_FILE "${CANN_3RD_LIB_PATH}/${PREFIX}-master.zip")
        if(NOT EXISTS ${ZIP_FILE})
            message(STATUS "${ZIP_FILE} does not exist.")
            set(ALL_ZIP_EXIST FALSE)
        endif()
    endforeach()

    if(ALL_ZIP_EXIST)
        foreach(PREFIX ${ZIP_PREFIXES})
            set(ZIP_FILE "${CANN_3RD_LIB_PATH}/${PREFIX}-master.zip")
            execute_process(
                COMMAND unzip -q ${ZIP_FILE} -d ${_ms_root}
                WORKING_DIRECTORY ${_ms_root}
            )
        endforeach()
        if(EXISTS ${_ms_root}/msot)
            file(REMOVE_RECURSE ${_ms_root}/msot)
        endif()
        execute_process(
            COMMAND mv ${_ms_root}/msot-master ${_ms_root}/msot
            WORKING_DIRECTORY ${_ms_root}
        )
        foreach(PREFIX ${SUB_ZIP_PREFIXES})
            if(EXISTS ${_ms_root}/msot/${PREFIX})
                file(REMOVE_RECURSE ${_ms_root}/msot/${PREFIX})
            endif()
            execute_process(
                COMMAND mv ${_ms_root}/${PREFIX}-master ${_ms_root}/msot/${PREFIX}
                WORKING_DIRECTORY ${_ms_root}
            )
        endforeach()
    elseif(NOT EXISTS "${_ms_root}/msot/cmake/module/msopgen.cmake")
        include(FetchContent)
        FetchContent_Declare(
            third_party_msot
            GIT_REPOSITORY https://gitcode.com/Ascend/msot.git
            GIT_TAG        3bc33761478c28c0cb53303e3ccd9da5b6239975
            GIT_SUBMODULES msopgen
            GIT_PROGRESS   TRUE
            SOURCE_DIR "${_ms_root}/msot"
        )
        FetchContent_GetProperties(third_party_msot)
        if(NOT third_party_msot_POPULATED)
            FetchContent_Populate(third_party_msot)
        endif()
    endif()
endif()

# In mirror layouts, msopgen can live alongside msot
if(EXISTS "${_ms_root}/msopgen" AND NOT EXISTS "${_ms_root}/msot/msopgen")
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_ms_root}/msot"
  )
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E create_symlink
            "${_ms_root}/msopgen"
            "${_ms_root}/msot/msopgen"
  )
endif()

set(ROOT_DIR "${_ms_root}/msot")
include("${ROOT_DIR}/cmake/module/msopgen.cmake")

install(FILES ${CMAKE_INSTALL_PREFIX}/msopgen/${MSOPGEN_WHEEL_IMAGE}
    OPTIONAL DESTINATION tools/
    PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
)
install(FILES ${CMAKE_INSTALL_PREFIX}/msopgen/${MSOPST_WHEEL_IMAGE}
    OPTIONAL DESTINATION tools/
    PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
)
