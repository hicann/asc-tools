#!/usr/bin/env csh
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

if ( "-${argv}" != "-" ) then
    set CUR_FILE=${argv}
else
    set CUR_FILE=`readlink -f $0`
endif
set CUR_DIR=`dirname ${CUR_FILE}`
set version_dir=`cat "$CUR_DIR/../version.info" | grep "version_dir" | cut -d"=" -f2`
if ( "-$version_dir" == "-" ) then
    set INSTALL_DIR=`realpath ${CUR_DIR}/../..`
else
    set INSTALL_DIR=`realpath ${CUR_DIR}/../../../latest`
endif

set toolchain_path="${INSTALL_DIR}"
if ( -d ${toolchain_path} ) then
    csh_prepend_env(TOOLCHAIN_HOME, ${toolchain_path})
endif

set lib_path="${INSTALL_DIR}/python/site-packages/"
if (-d ${lib_path}) then
    csh_prepend_env(PYTHONPATH, ${lib_path})
endif

set op_tools_path="${INSTALL_DIR}/python/site-packages/bin/"
if (-d ${op_tools_path}) then
    csh_prepend_env(PATH, ${op_tools_path})
endif

set msobjdump_path="${INSTALL_DIR}/tools/msobjdump/"
if (-d ${msobjdump_path}) then
    csh_prepend_env(PATH, ${msobjdump_path})
endif

set show_kernel_debug_data_tool_path="${INSTALL_DIR}/tools/show_kernel_debug_data/"
if (-d ${show_kernel_debug_data_tool_path}) then
    csh_prepend_env(PATH, ${show_kernel_debug_data_tool_path})
endif