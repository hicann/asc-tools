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

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build_dir="${demo_dir}/build"
bin_dir="${build_dir}/npu_compute/bin"
default_cann_root="/usr/local/Ascend/cann/x86_64-linux"
example_name=${1:-add}
example_tool="memcheck"

case "${example_name}" in
    add)
        example_target="aclsan_demo_add"
        example_executable="${bin_dir}/${example_target}"
        example_work_dir="${demo_dir}"
        asc_architecture="dav-3510"
        ;;
    datacopy_stride)
        example_target="aclsan_demo_datacopy_stride"
        example_executable="${bin_dir}/${example_target}"
        example_work_dir="${demo_dir}"
        asc_architecture="dav-3510"
        ;;
    padding_register_state)
        example_target="aclsan_demo_padding_register_state"
        example_executable="${bin_dir}/${example_target}"
        example_work_dir="${demo_dir}"
        asc_architecture="dav-3510"
        ;;
    matmul_basic_api)
        example_target="aclsan_demo_matmul_basic_api"
        example_executable="${demo_dir}/examples/matmul_basic_api/build/bin/${example_target}"
        example_work_dir="${demo_dir}/examples/matmul_basic_api/build/run"
        asc_architecture="dav-3510"
        ;;
    matmul_leakyrelu_basic_api)
        example_target="aclsan_demo_matmul_leakyrelu_basic_api"
        example_executable="${demo_dir}/examples/matmul_leakyrelu_basic_api/build/bin/${example_target}"
        example_work_dir="${demo_dir}/examples/matmul_leakyrelu_basic_api/build/run"
        asc_architecture="dav-3510"
        ;;
    synccheck/*)
        synccheck_example=${example_name#synccheck/}
        if [[ ! "${synccheck_example}" =~ ^[a-z0-9_]+$ ||
            ! -f "${demo_dir}/examples/synccheck/${synccheck_example}.asc" ]]; then
            printf 'unsupported example: %s\n' "${example_name}" >&2
            exit 2
        fi
        example_target="aclsan_demo_synccheck_${synccheck_example}"
        example_executable="${bin_dir}/${example_target}"
        example_work_dir="${demo_dir}"
        example_tool="synccheck"
        asc_architecture="dav-3510"
        ;;
    *)
        printf 'unsupported example: %s\n' "${example_name}" >&2
        exit 2
        ;;
esac

case "${asc_architecture}" in
    dav-3510)
        dbi_architecture="dav-c310"
        ;;
    *)
        printf 'unsupported ASC architecture for DBI: %s\n' "${asc_architecture}" >&2
        exit 2
        ;;
esac

if [[ $# -gt 1 ]]; then
    printf 'usage: %s [add|datacopy_stride|padding_register_state|matmul_basic_api|' \
        'matmul_leakyrelu_basic_api|synccheck/<sample>]\n' \
        "${BASH_SOURCE[0]}" >&2
    exit 2
fi

cmake_args=(
    -S "${demo_dir}"
    -B "${build_dir}"
    "-DCMAKE_ASC_ARCHITECTURES=${asc_architecture}"
)

if [[ "${build_dir}" != "${demo_dir}/build" ]]; then
    printf 'unexpected build directory: %s\n' "${build_dir}" >&2
    exit 1
fi

rm -rf -- "${build_dir}"
NPUCOMPUTE_CANN_ROOT="${NPUCOMPUTE_CANN_ROOT:-${default_cann_root}}"
if [[ ! -f "${NPUCOMPUTE_CANN_ROOT}/include/acl/acl.h" ||
    ! -f "${NPUCOMPUTE_CANN_ROOT}/pkg_inc/profiling/prof_common.h" ]]; then
    printf 'invalid CANN root: %s\n' "${NPUCOMPUTE_CANN_ROOT}" >&2
    exit 1
fi
cann_install_dir=$(cd "${NPUCOMPUTE_CANN_ROOT}/.." && pwd)
if [[ ! -f "${cann_install_dir}/set_env.sh" ]]; then
    printf 'missing CANN environment script: %s/set_env.sh\n' "${cann_install_dir}" >&2
    exit 1
fi
set +u
source "${cann_install_dir}/set_env.sh"
set -u
cmake_args+=("-DNPUCOMPUTE_CANN_ROOT=${NPUCOMPUTE_CANN_ROOT}")
cmake "${cmake_args[@]}"
if [[ "${example_name}" == "add" || "${example_name}" == "datacopy_stride" ||
    "${example_name}" == "padding_register_state" || "${example_name}" == synccheck/* ]]; then
    cmake --build "${build_dir}" --target npu_check_cli "${example_target}" --parallel
else
    cmake --build "${build_dir}" --target npu_check_cli --parallel
    "${demo_dir}/examples/${example_name}/run.sh"
fi

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

if [[ -e "${bin_dir}/npucheck" ]]; then
    printf 'unexpected legacy demo artifact: %s\n' "${bin_dir}/npucheck" >&2
    exit 1
fi

if [[ ! -x "${example_executable}" ]]; then
    printf 'missing example executable: %s\n' "${example_executable}" >&2
    exit 1
fi

cann_lib_dir="${NPUCOMPUTE_CANN_ROOT}/lib64"
for library in libacl_rt.so libruntime.so libprofapi.so; do
    if [[ ! -f "${cann_lib_dir}/${library}" ]]; then
        printf 'missing CANN Runtime library: %s\n' "${cann_lib_dir}/${library}" >&2
        exit 1
    fi
done

export ASCEND_GLOBAL_LOG_LEVEL=0
# export ASCEND_SLOG_PRINT_TO_STDOUT=1
export NPU_SAN_DEBUG=1                       # npu_check的日志开启
# export NPU_COMPUTE_DEBUG=1                 # lib_acl_tool_injection.so的日志开启

export NPU_CHECK_DBI_ARCH="${dbi_architecture}"
export NPU_CHECK_DBI_TOOLCHAIN_ROOT="${cann_install_dir}"
export NPU_CHECK_DBI_SOURCE_ROOT="${build_dir}/dbi_runtime_sources"
export LD_LIBRARY_PATH="${bin_dir}:${cann_lib_dir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

(cd "${example_work_dir}" && "${bin_dir}/npu_check" --tool "${example_tool}" -- "${example_executable}")
if [[ "${example_name}" == "matmul_basic_api" || "${example_name}" == "matmul_leakyrelu_basic_api" ]]; then
    python3 "${demo_dir}/examples/${example_name}/scripts/verify_result.py" \
        "${example_work_dir}/output/output.bin" \
        "${example_work_dir}/output/golden.bin"
fi
