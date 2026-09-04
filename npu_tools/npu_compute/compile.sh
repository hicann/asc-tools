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

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
CANN_ROOT="${NPUCOMPUTE_CANN_ROOT:-${ASCEND_HOME_PATH:-}}"
BUILD_JOBS="${NPU_COMPUTE_BUILD_JOBS:-$(nproc)}"
BOOST_VERSION="1.87.0"
BOOST_SOURCE_VERSION="1_87_0"
BOOST_ARCHIVE_SHA256="f55c340aa49763b1925ccf02b2e83f35fdcf634c9d5164a2acb87540173c741d"
BOOST_ARCHIVE_URL="https://cann-3rd.obs.cn-north-4.myhuaweicloud.com/boost/boost_${BOOST_SOURCE_VERSION}.tar.gz"
BOOST_DEPS_DIR="${BUILD_DIR}/_deps"
BOOST_DOWNLOAD_DIR="${BOOST_DEPS_DIR}/downloads"
BOOST_SOURCE_PARENT="${BOOST_DEPS_DIR}/src"
BOOST_SOURCE_DIR="${BOOST_SOURCE_PARENT}/boost_${BOOST_SOURCE_VERSION}"
BOOST_INSTALL_DIR="${BOOST_DEPS_DIR}/boost-filesystem"
BOOST_ARCHIVE="${BOOST_DOWNLOAD_DIR}/boost_${BOOST_SOURCE_VERSION}.tar.gz"
BOOST_LIBRARY="${BOOST_INSTALL_DIR}/lib/libboost_filesystem.a"
BOOST_CONFIG_DIR="${BOOST_INSTALL_DIR}/lib/cmake/Boost-${BOOST_VERSION}"
BOOST_INCLUDE_FILE="${BUILD_DIR}/npu_compute_boost.cmake"
STALE_RUNTIME_LIBRARIES=(
    "${BUILD_DIR}/bin/libnpu-compute.so"
    "${BUILD_DIR}/bin/libacl_pti.so"
)

if [[ -z "${CANN_ROOT}" ]]; then
    echo "NPUCOMPUTE_CANN_ROOT or ASCEND_HOME_PATH must be set" >&2
    exit 1
fi

download_boost() {
    mkdir -p "${BOOST_DOWNLOAD_DIR}"
    if [[ -f "${BOOST_ARCHIVE}" ]]; then
        if printf '%s  %s\n' "${BOOST_ARCHIVE_SHA256}" "${BOOST_ARCHIVE}" | sha256sum --check --status; then
            return
        fi
        mv "${BOOST_ARCHIVE}" "${BOOST_ARCHIVE}.invalid.$$"
    fi

    local temporary_archive
    temporary_archive="$(mktemp "${BOOST_ARCHIVE}.download.XXXXXX")"
    if ! curl --fail --location --retry 3 --output "${temporary_archive}" "${BOOST_ARCHIVE_URL}"; then
        rm -f "${temporary_archive}"
        return 1
    fi
    if ! printf '%s  %s\n' "${BOOST_ARCHIVE_SHA256}" "${temporary_archive}" | sha256sum --check --status; then
        echo "Boost archive checksum verification failed" >&2
        rm -f "${temporary_archive}"
        return 1
    fi
    mv "${temporary_archive}" "${BOOST_ARCHIVE}"
}

prepare_boost() {
    if [[ -f "${BOOST_LIBRARY}" && -f "${BOOST_CONFIG_DIR}/BoostConfig.cmake" ]]; then
        return
    fi

    download_boost
    mkdir -p "${BOOST_SOURCE_PARENT}" "${BOOST_INSTALL_DIR}"
    if [[ ! -x "${BOOST_SOURCE_DIR}/b2" ]]; then
        if [[ -e "${BOOST_SOURCE_DIR}" ]]; then
            mv "${BOOST_SOURCE_DIR}" "${BOOST_SOURCE_DIR}.incomplete.$$"
        fi
        tar -xzf "${BOOST_ARCHIVE}" -C "${BOOST_SOURCE_PARENT}"
        (
            cd "${BOOST_SOURCE_DIR}"
            ./bootstrap.sh --prefix="${BOOST_INSTALL_DIR}" --with-libraries=headers
        )
    fi

    (
        cd "${BOOST_SOURCE_DIR}"
        ./b2 headers install -j"${BUILD_JOBS}"
        ./b2 --with-filesystem \
            --layout=system \
            --prefix="${BOOST_INSTALL_DIR}" \
            --libdir="${BOOST_INSTALL_DIR}/lib" \
            --includedir="${BOOST_INSTALL_DIR}/include" \
            variant=release \
            link=static \
            runtime-link=shared \
            threading=multi \
            cxxstd=17 \
            "cxxflags=-fPIC -fstack-protector-all -D_FORTIFY_SOURCE=2 -fvisibility=hidden -fvisibility-inlines-hidden -fno-common -D_GLIBCXX_USE_CXX11_ABI=0 -DBOOST_FILESYSTEM_NO_CXX20_ATOMIC_REF" \
            -j"${BUILD_JOBS}" \
            install
    )

    if [[ ! -f "${BOOST_LIBRARY}" || ! -f "${BOOST_CONFIG_DIR}/BoostConfig.cmake" ]]; then
        echo "Boost filesystem build did not produce the expected artifacts" >&2
        exit 1
    fi
}

write_boost_include() {
    {
        printf 'add_library(npu_compute_boost_filesystem STATIC IMPORTED GLOBAL)\n'
        printf 'set_target_properties(npu_compute_boost_filesystem PROPERTIES\n'
        printf '  IMPORTED_LOCATION "%s"\n' "${BOOST_LIBRARY}"
        printf '  INTERFACE_INCLUDE_DIRECTORIES "%s/include"\n' "${BOOST_INSTALL_DIR}"
        printf '  INTERFACE_COMPILE_DEFINITIONS "BOOST_FILESYSTEM_NO_LIB;BOOST_FILESYSTEM_NO_CXX20_ATOMIC_REF"\n'
        printf ')\n'
        printf 'add_library(Boost::filesystem ALIAS npu_compute_boost_filesystem)\n'
        printf 'add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=0 BOOST_FILESYSTEM_NO_CXX20_ATOMIC_REF)\n'
    } > "${BOOST_INCLUDE_FILE}"
}

remove_stale_runtime_libraries() {
    for library in "${STALE_RUNTIME_LIBRARIES[@]}"; do
        if [[ -e "${library}" ]]; then
            rm -f "${library}"
        fi
    done
}

prepare_boost
write_boost_include
remove_stale_runtime_libraries

cmake -S "${SCRIPT_DIR}/.." -B "${BUILD_DIR}" \
    -U NPU_COMPUTE_BUILD_CANN_BACKEND \
    -DINJECTION_CANN_ROOT="${CANN_ROOT}" \
    -DNPUCOMPUTE_CANN_ROOT="${CANN_ROOT}" \
    -DNPU_COMPUTE_BUILD_TESTS=OFF \
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="${BOOST_INCLUDE_FILE}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build "${BUILD_DIR}" \
    --target npu_compute npu_compute_cli -j"${BUILD_JOBS}"
