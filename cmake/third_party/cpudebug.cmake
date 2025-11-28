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

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/libraries/lib/include/stub_fun.h)
  set(CPUDEBUG_PKG_NAME cpudebug_deps_linux-${CMAKE_SYSTEM_PROCESSOR}.tar.gz)
  set(CPUDEBUG_PKG_PATH ${DEPS_FILE_PATH}/${CPUDEBUG_PKG_NAME})
  if (NOT EXISTS ${CPUDEBUG_PKG_PATH})
      set(CPUDEBUG_PKG_URL "https://mirrors.huaweicloud.com/artifactory/cann-run/8.5.0.alpha001/try/${CMAKE_SYSTEM_PROCESSOR}/${CPUDEBUG_PKG_NAME}")
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
    COMMAND tar -xf ${CPUDEBUG_PKG_PATH} -C ${CMAKE_SOURCE_DIR}/libraries --strip-components 3
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/libraries
    RESULT_VARIABLE result
  )
endif()