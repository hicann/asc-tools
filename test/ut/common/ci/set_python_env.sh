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

set -e

top_dir=$1
test_type=$2
soc_info=$3
base_path=$top_dir/tmp/llt_gccnative-prefix/src/llt_gccnative-build/

#PYTHONPATH
export PYTHONPATH=$top_dir/open_source/tvm/python:$top_dir/open_source/tvm/topi/python:$PYTHONPATH

#PATH
export PATH=$top_dir/build/bin/toolchain/x86/ubuntu/ccec_libs/ccec_x86_ubuntu_20_04_adk/bin:$PATH

#LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$base_path/llt/tensor_engine/ut:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$base_path/metadef/register:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH=$base_path/air/graph_metadef/register:$LD_LIBRARY_PATH
export ASCEND_OPP_PATH=$top_dir/out/onetrack/llt/$test_type/obj
mkdir -p "$ASCEND_OPP_PATH"
echo "os=linux" > "$ASCEND_OPP_PATH/scene.info"


# fast execute
# top_dir=~/davinci_trunk
# test_type=py_st # py_ut
# base_path=$top_dir/tmp/llt_gccnative-prefix/src/llt_gccnative-build/

# export PATH=$top_dir/build/bin/toolchain/x86/ubuntu/ccec_libs/ccec_x86_ubuntu_20_04_adk/bin:$PATH
# export PYTHONPATH=$top_dir/open_source/tvm/python:$top_dir/open_source/tvm/topi/python:$PYTHONPATH
# export LD_LIBRARY_PATH=$base_path/llt/tensor_engine/ut:$LD_LIBRARY_PATH
# export LD_LIBRARY_PATH=$base_path/metadef/register:$LD_LIBRARY_PATH
# export ASCEND_OPP_PATH=$top_dir/out/onetrack/llt/$test_type/obj

