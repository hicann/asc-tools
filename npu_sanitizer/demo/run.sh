#!/usr/bin/env bash

set -euo pipefail

demo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
build_dir="${demo_dir}/build"
bin_dir="${build_dir}/npu_compute/bin"
default_cann_root="/home/cty/cann_0729/cann"
cmake_args=(
    -S "${demo_dir}"
    -B "${build_dir}"
)

if [[ "${build_dir}" != "${demo_dir}/build" ]]; then
    printf 'unexpected build directory: %s\n' "${build_dir}" >&2
    exit 1
fi

rm -rf -- "${build_dir}"
if [[ -z "${NPUCOMPUTE_CANN_ROOT:-}" &&
    -f "${default_cann_root}/include/acl/acl.h" &&
    -f "${default_cann_root}/pkg_inc/profiling/prof_common.h" ]]; then
    NPUCOMPUTE_CANN_ROOT="${default_cann_root}"
fi
if [[ -n "${NPUCOMPUTE_CANN_ROOT:-}" ]]; then
    cmake_args+=("-DNPUCOMPUTE_CANN_ROOT=${NPUCOMPUTE_CANN_ROOT}")
fi
cmake "${cmake_args[@]}"
cmake --build "${build_dir}" --parallel

for artifact in \
    "${bin_dir}/libacl_san.so" \
    "${bin_dir}/libacl_tool_injection.so" \
    "${bin_dir}/libruntime.so" \
    "${bin_dir}/libnpu_check.so" \
    "${bin_dir}/npucheck"; do
    if [[ ! -f "${artifact}" ]]; then
        printf 'missing demo artifact: %s\n' "${artifact}" >&2
        exit 1
    fi
done

export ASCEND_GLOBAL_LOG_LEVEL=0
export ASCEND_SLOG_PRINT_TO_STDOUT=1
export LD_LIBRARY_PATH="${bin_dir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
"${bin_dir}/npucheck" --tool memcheck --tool synccheck -- "${bin_dir}/aclsan_demo_app"
