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
    printf 'ASCEND_HOME_PATH must be set. Please source the target CANN set_env.sh first\n' >&2
    exit 1
fi

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
cann_home=${ASCEND_HOME_PATH%/}
cann_parent=$(dirname -- "${cann_home}")
package_arch=$(uname -m)

(cd "${repo_dir}" && bash build.sh --pkg)
run_package=$(find "${repo_dir}/build_out" -maxdepth 1 -type f \
    -name "cann-asc-tools_*_linux-${package_arch}.run" -print -quit)
if [[ -z "${run_package}" ]]; then
    printf 'missing asc-tools run package for architecture %s\n' "${package_arch}" >&2
    exit 1
fi
bash "${run_package}" --full --quiet --install-path="${cann_parent}"

installed_arch_dir="${cann_home}/${package_arch}-linux"
installed_public_cli="${cann_home}/bin/npu-compute"
installed_private_cli="${installed_arch_dir}/bin/npu-compute"
for cli in "${installed_public_cli}" "${installed_private_cli}"; do
    if [[ ! -x "${cli}" ]]; then
        printf 'missing installed NPU Compute executable: %s\n' "${cli}" >&2
        exit 1
    fi
done
for artifact in \
    "${installed_arch_dir}/tools/npu_tools/lib64/libnpu-compute.so" \
    "${installed_arch_dir}/tools/npu_tools/lib64/libacl_tool_injection.so" \
    "${installed_arch_dir}/tools/npu_tools/lib64/libacl_pti.so" \
    "${installed_arch_dir}/lib64/libprofapi.so"; do
    if [[ ! -f "${artifact}" ]]; then
        printf 'missing installed NPU Compute dependency: %s\n' "${artifact}" >&2
        exit 1
    fi
done

NPU_COMPUTE_SMOKE_CLI=$(realpath -e -- "${installed_public_cli}")
if [[ "${NPU_COMPUTE_SMOKE_CLI}" != "$(realpath -e -- "${installed_private_cli}")" ]]; then
    printf 'public NPU Compute executable does not resolve to the installed architecture executable: %s\n' \
        "${installed_public_cli}" >&2
    exit 1
fi
export NPU_COMPUTE_SMOKE_CLI

printf 'asc-tools package installed into CANN: %s\n' "${cann_home}"
