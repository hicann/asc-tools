#!/usr/bin/env fish
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------
# set -x PACKAGE asc-tools
set -x CUR_DIR (realpath (dirname (status --current-filename)))

# shell_include("build/release/scripts/package/setenv_common.fish")

function prepend_env
    set -l name $argv[1]
    set -l value $argv[2..-1]
    set -l MYPATH
    set -l path
    for path in $$name
        if not contains "$path" $value
            set MYPATH $MYPATH $path
        end
    end
    set -gx $name $value $MYPATH
end

function get_install_dir
    set -lx version_dir (cat "$CUR_DIR/../version.info" | grep "version_dir" | cut -d"=" -f2)
    if test "-$version_dir" = "-"
        echo (realpath $CUR_DIR/../..)
    else
        echo (realpath $CUR_DIR/../../../latest)
    end
end
set -x INSTALL_DIR (get_install_dir)

set -x toolchain_path "$INSTALL_DIR"
if test -d {$toolchain_path}
    prepend_env TOOLCHAIN_HOME "$toolchain_path"
end
set -e toolchain_path

set -x lib_path "$INSTALL_DIR/python/site-packages/"
if test -d $lib_path
    prepend_env PYTHONPATH "$lib_path"
end
set -e lib_path

set -x op_tools_path "$INSTALL_DIR/python/site-packages/bin/"
if test -d $op_tools_path
    prepend_env PATH "$op_tools_path"
end
set -e op_tools_path

set -x msobjdump_path "$INSTALL_DIR/tools/msobjdump/"
if test -d $msobjdump_path
    prepend_env PATH "$msobjdump_path"
end
set -e msobjdump_path

set -x ascendump_path "$INSTALL_DIR/tools/show_kernel_debug_data/"
if test -d $ascendump_path
    prepend_env PATH "$ascendump_path"
end
set -e ascendump_path