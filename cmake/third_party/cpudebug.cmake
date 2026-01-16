# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set(PKG_NAME "cpudebug_deps")
string(TOLOWER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_LOWER)

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
    message(STATUS "Detected architecture: x86_64")
    set(TAR_ARCH x86)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|arm")
    message(STATUS "Detected architecture: ARM64")
    set(TAR_ARCH aarch64)
else ()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif ()

if (IS_DIRECTORY ${CMAKE_SOURCE_DIR}/libraries/lib AND NOT EXISTS ${CMAKE_SOURCE_DIR}/libraries/lib/cmake/targets-tikicpulib-${BUILD_TYPE_LOWER}.cmake)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${CMAKE_SOURCE_DIR}/libraries/lib
        COMMENT "Removing ${CMAKE_SOURCE_DIR}/libraries/lib directory..." 
    )
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/libraries/lib/include/stub_fun.h)
  set(CPUDEBUG_PKG_NAME cann-asc-tools-cpudebug-deps-lib_${BUILD_TYPE_LOWER}_8.5.0_linux-${CMAKE_SYSTEM_PROCESSOR}.tar.gz)
  set(CPUDEBUG_PKG_PATH ${DEPS_FILE_PATH}/${CPUDEBUG_PKG_NAME})
  if (NOT EXISTS ${CPUDEBUG_PKG_PATH})
      set(CPUDEBUG_PKG_URL "https://container-obsfs-filesystem.obs.cn-north-4.myhuaweicloud.com/package/cann/asc-tools-dev/version_compile/master/202601/20260116/ubuntu_${TAR_ARCH}/${CPUDEBUG_PKG_NAME}")
      message(STATUS "${PKG_NAME} pkg not found in ${DEPS_FILE_PATH}, downloading from ${CPUDEBUG_PKG_URL}")
  else()
      set(CPUDEBUG_PKG_URL ${CPUDEBUG_PKG_PATH})
  endif()

  include(FetchContent)
  FetchContent_Declare(
      ${PKG_NAME}
      URL ${CPUDEBUG_PKG_URL}
      TLS_VERIFY FALSE
      DOWNLOAD_DIR ${DEPS_FILE_PATH}
      DOWNLOAD_NO_EXTRACT TRUE
  )
  FetchContent_MakeAvailable(${PKG_NAME})

  execute_process(
    COMMAND tar -xf ${CPUDEBUG_PKG_PATH} -C ${CMAKE_SOURCE_DIR}/libraries --strip-components 1
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/libraries
    RESULT_VARIABLE result
  )
endif()