# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set(CMAKE_PREFIX_PATH ${ASCEND_CANN_PACKAGE_PATH}/)

set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
  ${CMAKE_MODULE_PATH}
)
message(STATUS "CMAKE_MODULE_PATH            :${CMAKE_MODULE_PATH}")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type(default Release)"
    FORCE)
endif ()

# 变量PVMODEL_PATH在pvmodel.cmake中进行设置，适配simulator的tar包和run包两种形式
message(STATUS "the PVMODEL_PATH is :${PVMODEL_PATH}")

find_package(unified_dlog MODULE REQUIRED)
find_package(securec MODULE REQUIRED)
find_package(mmpa MODULE REQUIRED)
find_package(metadef MODULE REQUIRED)

find_package(pvmodel MODULE REQUIRED)
