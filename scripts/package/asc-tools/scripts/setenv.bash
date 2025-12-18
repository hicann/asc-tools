#!/bin/bash
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
CUR_DIR=`dirname ${BASH_SOURCE[0]}`

# shell_include("build/release/scripts/package/setenv_common.bash")

prepend_env() {
    local name="$1"
    local value="$2"
    local env_value="$(eval echo "\${${name}}" | tr ':' '\n' | grep -v "^${value}$" | tr '\n' ':' | sed 's/:$/\n/')"
    if [ "$env_value" = "" ]; then
        read $name <<EOF
$value
EOF
    else
        read $name <<EOF
$value:$env_value
EOF
    fi
    export $name
}

version_dir=`cat "$CUR_DIR/../version.info" | grep "version_dir" | cut -d"=" -f2`
if [ -z "$version_dir" ]; then
    INSTALL_DIR=`realpath ${CUR_DIR}/../..`
else
    INSTALL_DIR=`realpath ${CUR_DIR}/../../../cann`
fi

toolchain_path="${INSTALL_DIR}"
if [ -d ${toolchain_path} ]; then
    prepend_env TOOLCHAIN_HOME "$toolchain_path"
fi

lib_path="${INSTALL_DIR}/python/site-packages/"
if [ -d ${lib_path} ]; then
    prepend_env PYTHONPATH "$lib_path"
fi

op_tools_path="${INSTALL_DIR}/python/site-packages/bin/"
if [ -d ${op_tools_path} ]; then
    prepend_env PATH "$op_tools_path"
fi

msobjdump_path="${INSTALL_DIR}/tools/msobjdump/"
if [ -d ${msobjdump_path} ]; then
    prepend_env PATH "$msobjdump_path"
fi

show_kernel_debug_data_tool_path="${INSTALL_DIR}/tools/show_kernel_debug_data/"
if [ -d ${show_kernel_debug_data_tool_path} ]; then
    prepend_env PATH "$show_kernel_debug_data_tool_path"
fi
