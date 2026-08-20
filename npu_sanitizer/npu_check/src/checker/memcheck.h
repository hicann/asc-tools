// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef NPU_CHECK_CHECKER_MEMCHECK_H
#define NPU_CHECK_CHECKER_MEMCHECK_H

#include "aclsan/aclsan_api.h"
#include "checker/allocation_registry.h"
#include "diagnostic/device_protocol.h"
#include "diagnostic/report_renderer.h"

#include <cstdint>
#include <vector>

namespace npu::sanitizer {

struct MemcheckStats {
    uint64_t allocations = 0;
    uint64_t frees = 0;
    uint64_t deviceOperations = 0;
    uint64_t synchronizationEvents = 0;
    uint64_t errors = 0;
    uint64_t warnings = 0;
    uint64_t pendingDeviceOperations = 0;
    uint64_t droppedDeviceOperations = 0;
};

class Memcheck {
public:
    explicit Memcheck(bool strictUnknown);

    void OnAllocation(const AclsanResourceData& data);
    void OnFree(const AclsanResourceData& data);
    void QueueDeviceMemoryAccess(const AclsanDeviceMemoryAccessData& data);
    std::vector<aclsan::cann::NpusanMemcheckReport> OnSynchronization();
    MemcheckStats Stats() const;

private:
    std::vector<aclsan::cann::NpusanMemcheckReport> CheckAccess(
        const AclsanDeviceMemoryAccessData& data, aclsan::cann::NpusanReportAccessMode accessMode, uint64_t address,
        uint64_t bytes);
    std::vector<aclsan::cann::NpusanMemcheckReport> CheckDeviceMemoryAccess(const AclsanDeviceMemoryAccessData& data);
    void Count(const std::vector<aclsan::cann::NpusanMemcheckReport>& reports);

    bool strictUnknown_ = true;
    AllocationRegistry allocations_;
    std::vector<AclsanDeviceMemoryAccessData> pendingDeviceAccesses_;
    MemcheckStats stats_{};
    uint64_t nextReportId_ = 1;
    static constexpr size_t kMaxPendingDeviceOperations = 1u << 20u;
};

} // namespace npu::sanitizer

#endif
