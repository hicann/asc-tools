#!/usr/bin/env bash
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set -euo pipefail

if [[ $# -ne 1 ]]; then
    printf 'usage: %s <npu-tools-build-directory>\n' "$0" >&2
    exit 2
fi

build_dir=$(readlink -m -- "$1")
library_dir="${build_dir}/lib64"
binary_dir="${build_dir}/bin"

for library in \
    libacl_tool_injection.so \
    libacl_pti.so \
    libnpu-compute.so \
    libacl_san.so \
    libnpu_check.so; do
    if [[ ! -f "${library_dir}/${library}" ]]; then
        printf 'missing product library: %s\n' "${library_dir}/${library}" >&2
        exit 1
    fi
done

for binary in npu-compute npu-check; do
    if [[ ! -x "${binary_dir}/${binary}" ]]; then
        printf 'missing product executable: %s\n' "${binary_dir}/${binary}" >&2
        exit 1
    fi
done

if find "${library_dir}" -maxdepth 1 -type f -name '*.a' -print -quit | grep -q .; then
    printf 'internal static library leaked into product library directory\n' >&2
    exit 1
fi

printf 'product build output layout check passed\n'
