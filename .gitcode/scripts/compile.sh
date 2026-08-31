#!/bin/bash
# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
set -e
set -o pipefail
cd ${WORKSPACE}
echo $(grep -E "^VERSION_ID=" /etc/os-release | cut -d'"' -f2)
if [[ "${task_name}" =~ x86_compile_ubuntu24 ]] && [ "${TARGET_BRANCH}" == master ]; then
    echo "api-check=compile" >> "${ATOMGIT_OUTPUT}"
else
    echo "api-check=continue" >> "${ATOMGIT_OUTPUT}"
fi
if [[ "${task_name}" == *ubuntu24* ]]; then
    if [ "${TARGET_BRANCH}" == "master" ]; then
        sudo update-alternatives --set gcc /usr/bin/gcc-15
    else
        sudo update-alternatives --set gcc /usr/bin/gcc-14
    fi
else
    if [[ -f "/opt/rh/devtoolset-7/enable" ]]; then
        echo "source devtoolset"
        source /opt/rh/devtoolset-7/enable
    fi
fi
if gcc --version | head -n1 | grep -q "15\."; then
    rm -rf /home/jenkins/opensource/lib_cache
    if [ -d /home/jenkins/opensource/gcc15 ]; then
        rm -rf /home/jenkins/opensource/gcc15/lib_cache/abseil-cpp
        rm -rf /home/jenkins/opensource/gcc15/lib_cache/device/abseil-cpp
        ln -s /home/jenkins/opensource/gcc15/lib_cache/ /home/jenkins/opensource/lib_cache
    elif [ -d /home/jenkins/opensource/gcc15x86 ]; then
        rm -rf /home/jenkins/opensource/gcc15x86/lib_cache/abseil-cpp
        rm -rf /home/jenkins/opensource/gcc15x86/lib_cache/device/abseil-cpp
        ln -s /home/jenkins/opensource/gcc15x86/lib_cache/ /home/jenkins/opensource/lib_cache
    fi
elif gcc --version | head -n1 | grep -q "14\."; then
    gcc --version
else
    gcc --version
    rm -rf /home/jenkins/opensource/lib_cache
    ln -s /home/jenkins/opensource/ubuntu20/lib_cache /home/jenkins/opensource/lib_cache
fi
gcc --version
source /home/jenkins/Ascend/cann/bin/setenv.bash
case "${task_name}" in
    x86_compile_ubuntu24)
        sed -i "1i set(CMAKE_EXPORT_COMPILE_COMMANDS ON)" "CMakeLists.txt"
        bash build.sh --cann_3rd_lib_path=/home/jenkins/opensource
        echo "exec cmd: [bash build.sh --cann_3rd_lib_path=/home/jenkins/opensource]"
        ;;
    x86_compile)
        bash build.sh --cann_3rd_lib_path=/home/jenkins/opensource
        echo "exec cmd: [bash build.sh --cann_3rd_lib_path=/home/jenkins/opensource]"
        exit 0
        ;;
    arm_compile)
        bash build.sh --cann_3rd_lib_path=/home/jenkins/opensource
        echo "exec cmd: [bash build.sh --cann_3rd_lib_path=/home/jenkins/opensource]"
        exit 0
        ;;
esac
