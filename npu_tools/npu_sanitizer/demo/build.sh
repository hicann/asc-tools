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

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build_dir="${demo_dir}/build"
bin_dir="${build_dir}/npu_tools/bin"
default_cann_home="/usr/local/Ascend/cann"

ASCEND_HOME_PATH="${ASCEND_HOME_PATH:-${default_cann_home}}"
if [[ ! -f "${ASCEND_HOME_PATH}/set_env.sh" ]]; then
    printf 'missing CANN environment script: %s/set_env.sh\n' "${ASCEND_HOME_PATH}" >&2
    exit 1
fi

set +u
source "${ASCEND_HOME_PATH}/set_env.sh"
set -u

rm -rf -- "${build_dir}"
cmake -S "${demo_dir}" -B "${build_dir}" -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build "${build_dir}" --target npu_check_cli --parallel

for artifact in \
    "${bin_dir}/libacl_san.so" \
    "${bin_dir}/libacl_tool_injection.so" \
    "${bin_dir}/libnpu_check.so" \
    "${bin_dir}/npu_check"; do
    if [[ ! -f "${artifact}" ]]; then
        printf 'missing demo artifact: %s\n' "${artifact}" >&2
        exit 1
    fi
done

printf 'demo build completed: %s\n' "${build_dir}"
