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

if [[ -z "${ASCEND_HOME_PATH:-}" ]]; then
    printf 'ASCEND_HOME_PATH is not set; source the target CANN set_env.sh first\n' >&2
    exit 2
fi

case_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build_dir="${case_dir}/build"

cmake -S "${case_dir}" -B "${build_dir}" -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build "${build_dir}" --parallel
exec "${build_dir}/demo"
