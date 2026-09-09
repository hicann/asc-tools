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
cd "$(dirname "$0")"
: "${ASCEND_HOME_PATH:?source CANN set_env.sh first}"
cmake -S . -B build -DCMAKE_ASC_ARCHITECTURES=dav-3510
cmake --build build --parallel
if [[ -n "${NPU_CHECK_TOOLKIT_HOME:-}" ]]; then export ASCEND_TOOLKIT_HOME="$NPU_CHECK_TOOLKIT_HOME"; fi
export ASCEND_SLOG_PRINT_TO_STDOUT=0
for scenario in valid mem_only sync_only both same_stream multi_stream; do
    tools=(--tools memcheck --tools synccheck)
    if [[ "$scenario" == same_stream ]]; then tools+=(--tools memcheck); fi
    if [[ "$scenario" == multi_stream ]]; then tools=(--tools synccheck --tools memcheck); fi
    set +e
    "${NPU_CHECK_BIN:-npu-check}" "${tools[@]}" -- build/demo "$scenario" >"build/$scenario.log" 2>&1
    status=$?
    set -e
    python3 verify.py "$scenario" "$status" "build/$scenario.log"
done

# # Memcheck 与 Synccheck 复合用例

# 在 dav-3510 / Ascend950 上通过 `--tools memcheck --tools synccheck` 同时检查 GM 搬运和同步配对。

# | 场景 | 构造 | Memcheck 错误 | Synccheck 错误 | 显式 stream 同步次数 |
# | --- | --- | ---: | ---: | ---: |
# | valid | 合法搬运，无未消费通知 | 0 | 0 | 1 |
# | mem_only | 最后一个 GM 读取 burst 越界 | 1 | 0 | 1 |
# | sync_only | 合法搬运，留下未消费 SET_FLAG | 0 | 1 | 1 |
# | both | 同一 kernel 同时触发两类错误 | 1 | 1 | 1 |
# | same_stream | 同 stream 两次错误 launch，统一同步 | 2 | 2 | 1 |
# | multi_stream | 一个 stream 出错，另一个合法，分别同步 | 1 | 1 | 2 |

# 搬运源区间为 `[0,32)`、`[48,80)`、`[96,128)`。合法输入分配 128 字节；
# 错误输入分配 112 字节，只有最后一段 32 字节读取越界。输出分配 96 字节。
# 程序校验合法输入对应的全部 24 个输出元素；越界场景仅校验来自有效源地址的前 20 个元素，
# 不对越界读取值作假设。多 stream 的合法输出单独完整校验。

# 同步错误由 `asc_sync_notify(PIPE_V, PIPE_MTE2, EVENT_ID7)` 产生，故意不消费该通知。
# 正常搬运使用 `PipeBarrier<PIPE_ALL>()` 保证执行顺序；错误场景不使用可能挂起设备的无配对等待。
# `same_stream` 还重复指定 memcheck，检查参数去重；`multi_stream` 反向指定两个工具，检查顺序无关性。

# ## 运行

# 先加载 CANN 环境，并确保当前安装的 npu-check 支持 `--tools`：

# ```bash
# bash run.sh
# ```

# 可通过 `NPU_CHECK_BIN` 指定 CLI，通过 `NPU_CHECK_TOOLKIT_HOME` 指定包含
# `<arch>-linux/tools/npu_tools/lib64/libnpu_check.so` 的临时安装根目录。
# 脚本将逐项检查应用结果、精确诊断数量、工具汇总、无待处理/丢弃事件、CLI 标志及完整会话。
# 完整输出写入 `build/<场景>.log`，任一断言失败即停止并返回非零状态。

# ## 实机记录

# 2026-09-09，经 WezTerm 配置的跳转通道进入 3510 的 `rj` Docker，使用 CANN 9.2.0、
# `dav-3510` 和 device 0 验证，上述六项全部通过。先运行 Hello World 确认 8 个 AIV block 正常；
# 使用本分支构建的 CLI 和 libnpu_check.so，san API 源码未修改，未覆盖容器安装目录。
# 所有场景 `child_exit=0`、会话 `status=complete`，无 malformed callback、framework error 或丢失消息。
# 本地回收日志位于 `build/3510-validation/`（构建产物，不提交）。
