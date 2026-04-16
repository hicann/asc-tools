#!/bin/bash
# ----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# ----------------------------------------------------------------------------------------------------------

# 安全选项：遇到错误立即退出，使用未定义变量报错，管道失败时退出
set -euo pipefail

# 获取脚本所在目录的绝对路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# 环境配置
ASCEND_HOME_DIR="${ASCEND_HOME_DIR:-/usr/local/Ascend/ascend-toolkit/latest}"

# 加载 Ascend 环境
if [ ! -f "${ASCEND_HOME_DIR}/set_env.sh" ]; then
    echo "Error: Ascend environment file not found: ${ASCEND_HOME_DIR}/set_env.sh"
    exit 1
fi
source "${ASCEND_HOME_DIR}/set_env.sh"

# 配置路径
TOOLS_PATH="${PROJECT_ROOT}/examples"
LOG_PATH="${PROJECT_ROOT}/tmplog_tools"

# 验证路径
if [ ! -d "${TOOLS_PATH}" ]; then
    echo "Error: Examples directory not found: ${TOOLS_PATH}"
    exit 1
fi

# 示例列表
EXAMPLE_LIST=(
    01_show_kernel_debug_data
    02_cpudebug
)

# 时间格式化函数（秒转换为可读格式）
format_duration() {
    local seconds=$1
    if [ "${seconds}" -eq 0 ]; then
        echo "0s"
        return
    fi
    echo "${seconds}" | awk '{
        t=split("60 s 60 m 24 h 999 d", a)
        for(n=1; n<t; n+=2) {
            if($1==0) break
            s=$1%a[n] a[n+1] s
            $1=int($1/a[n])
        }
        print s
    }'
}

# 运行示例1（使用 CMAKE_ASC_RUN_MODE）
run_example1() {
    local base_path=$1
    local example_name=$2
    
    cd "${base_path}/${example_name}/" || return 1
    rm -rf build
    cmake -B build -DCMAKE_ASC_RUN_MODE=cpu -DCMAKE_ASC_ARCHITECTURES=dav-2201 || return 1
    cmake --build build || return 1
    ./build/add || return 1
}

# 运行示例2（标准 cmake + make）
run_example2() {
    local base_path=$1
    local example_name=$2
    
    cd "${base_path}/${example_name}/" || return 1
    rm -rf build
    mkdir -p build && cd build || return 1
    cmake .. || return 1
    make -j || return 1
    ./demo || return 1
}

# 执行测试用例
run_test_case() {
    local tools_path=$1
    local example_name=$2
    local log_path=$3
    
    local case_name="${example_name}"
    local start_time end_time duration elapsed
    
    start_time=$(date +%s)
    echo ">>>>>>>>>>>>>>>>>>>>> $(date '+%Y-%m-%d %H:%M:%S') run ${case_name} start! <<<<<<<<<<<<<<<<<<<<<"
    
    if [ "${case_name}" == "01_show_kernel_debug_data" ]; then
        run_example2 "${tools_path}" "${example_name}" 2>&1 | tee "${log_path}/${case_name}.log"
    else
        run_example1 "${tools_path}" "${example_name}" 2>&1 | tee "${log_path}/${case_name}.log"
    fi
    
    local test_result=${PIPESTATUS[0]}
    end_time=$(date +%s)
    duration=$((end_time - start_time))
    elapsed=$(format_duration "${duration}")
    
    echo "test case ${case_name} duration: ${elapsed}"
    echo ">>>>>>>>>>>>>>>>>>>>> $(date '+%Y-%m-%d %H:%M:%S') run ${case_name} finished! <<<<<<<<<<<<<<<<<<<<<"
    
    return ${test_result}
}

# 主执行流程
main() {
    echo "Current directory: ${PWD}"
    echo "Tools path: ${TOOLS_PATH}"
    echo "Log path: ${LOG_PATH}"
    
    local start_time end_time total_duration total_elapsed
    
    start_time=$(date +%s)
    echo "=== $(date '+%Y-%m-%d %H:%M:%S') ==="
    
    # 准备日志目录
    mkdir -p "${LOG_PATH}"
    rm -rf "${LOG_PATH:?}"/*
    rm -f result_tools.txt tools_cases.txt
    
    # 执行所有测试用例
    for example_name in "${EXAMPLE_LIST[@]}"; do
        run_test_case "${TOOLS_PATH}" "${example_name}" "${LOG_PATH}"
    done
    
    # 分析测试结果
    cd "${LOG_PATH}"
    ls * > ../tools_cases.txt
    sed -i 's/.log//g' ../tools_cases.txt
    cd ..
    
    while read -r line; do
        if [ -z "${line}" ]; then
            continue
        fi
        prf=$(grep -E "test pass|passed|\[Block \(5\/6\)\]: OUTPUT = 24" "${LOG_PATH}/${line}.log" || true)
        if [ -n "$prf" ]; then
            echo "${line} pass" >> result_tools.txt
        else
            echo "${line} fail" >> result_tools.txt
        fi
    done < tools_cases.txt
    
    # 检查是否有失败的测试
    prf2=$(grep -E "fail" result_tools.txt || true)
    if [ -n "$prf2" ]; then
        echo "execute samples failed"
        end_time=$(date +%s)
        total_duration=$((end_time - start_time))
        total_elapsed=$(format_duration "${total_duration}")
        echo "=== $(date '+%Y-%m-%d %H:%M:%S') ==="
        echo "test cases all duration: ${total_elapsed}"
        exit 1
    else
        echo "execute samples success"
    fi
    
    # 输出总耗时
    end_time=$(date +%s)
    total_duration=$((end_time - start_time))
    total_elapsed=$(format_duration "${total_duration}")
    echo "=== $(date '+%Y-%m-%d %H:%M:%S') ==="
    echo "test cases all duration: ${total_elapsed}"
}

# 执行主函数
main
