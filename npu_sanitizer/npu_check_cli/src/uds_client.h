/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_CHECK_CLI_UDS_CLIENT_H
#define NPU_CHECK_CLI_UDS_CLIENT_H

#include "uds_transport.h"

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

namespace npu::sanitizer::cli {

class UdsClient {
public:
    UdsClient() = default;
    ~UdsClient();
    UdsClient(const UdsClient&) = delete;
    UdsClient& operator=(const UdsClient&) = delete;

    // timeoutMs 是本次会话唯一一次"时长 → 时刻"的换算入口：内部立刻算出绝对 deadline，
    // 之后 connect 重试、Hello 往返、Configure 发送、Ready 接收共用同一个值，
    // 而不是每步各给一份完整超时。
    bool ConnectAndConfigure(
        const std::string& udsName, uint64_t sessionId, uint32_t childPid, int timeoutMs, const ipc::ToolConfig& config,
        std::string& ready, std::string& error);
    // 采集阶段读取后续帧。deadline 传 ipc::kNoDeadline 表示不设超时 —— 应用可能跑数小时。
    ipc::IoStatus Receive(ipc::Frame& frame, ipc::DeadlineMs deadline, std::string& error);
    void Close();

private:
    bool ConnectWithRetry(const std::string& udsName, ipc::DeadlineMs deadline, pid_t childPid, std::string& error);
    bool CheckServerIdentity(uint32_t childPid, ipc::DeadlineMs deadline, std::string& error);
    bool Send(ipc::MessageType type, const std::vector<uint8_t>& payload, ipc::DeadlineMs deadline, std::string& error);

    int fd_ = -1;
    uint64_t sessionId_ = 0;
    uint64_t sendSequence_ = 1;
    uint64_t receiveSequence_ = 1;
};

} // namespace npu::sanitizer::cli

#endif
