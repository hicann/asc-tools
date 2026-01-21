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

TOP_DIR=$1
INSTALL_PATH=$2
DST_DIR=$3

DST_PATH=${INSTALL_PATH}/${DST_DIR}
echo "TOP_DIR is $TOP_DIR"
echo "INSTALL_PATH is $INSTALL_PATH"
echo "DST_PATH is ---- $DST_PATH"

[ -n "$DST_PATH" ] && rm -rf $DST_PATH
mkdir -p $DST_PATH

# copy basic api
mkdir -p $DST_PATH/basic_api
cp -rf ${TOP_DIR}/atc/opcompiler/ascendc_compiler/framework/tikcfw/impl ${DST_PATH}/basic_api/impl
cp -rf ${TOP_DIR}/atc/opcompiler/ascendc_compiler/framework/tikcfw/interface ${DST_PATH}/basic_api/interface
cp -rf ${TOP_DIR}/atc/opcompiler/ascendc_compiler/framework/tikcfw/kernel_operator.h ${DST_PATH}/basic_api/kernel_operator.h
cp -rf ${TOP_DIR}/atc/opcompiler/ascendc_compiler/framework/tikcfw/op_frame ${DST_PATH}/basic_api/op_frame
cp -rf ${TOP_DIR}/atc/opcompiler/ascendc_compiler/framework/host_api/include/tiling/template_argument.h ${DST_PATH}/basic_api/template_argument.h

#copy highlevel_api
mkdir -p $DST_PATH/highlevel_api
cp -rf ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/lib ${DST_PATH}/highlevel_api/lib
cp -rf ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/impl ${DST_PATH}/highlevel_api/impl
cp -rf ${TOP_DIR}/atc/opcompiler/ascendc_compiler/api/tiling ${DST_PATH}/highlevel_api/tiling
ln -s ${DST_PATH}/highlevel_api/lib/matmul/matmul_intf.h  ${DST_PATH}/highlevel_api/lib/matmul_intf.h

