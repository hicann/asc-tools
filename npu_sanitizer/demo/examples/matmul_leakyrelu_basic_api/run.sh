#!/usr/bin/env bash
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

set -euo pipefail

example_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build_dir="${example_dir}/build"
run_dir="${build_dir}/run"

rm -rf -- "${build_dir}"
cmake -S "${example_dir}" -B "${build_dir}" -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build "${build_dir}" --target aclsan_demo_matmul_leakyrelu_basic_api --parallel

mkdir -p "${run_dir}"
(cd "${run_dir}" && python3 "${example_dir}/scripts/gen_data.py")
