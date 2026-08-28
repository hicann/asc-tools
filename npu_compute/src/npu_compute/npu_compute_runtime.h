/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_NPU_COMPUTE_NPU_COMPUTE_RUNTIME_H_
#define NPU_COMPUTE_SRC_NPU_COMPUTE_NPU_COMPUTE_RUNTIME_H_

#include "aclpti/aclpti.h"
#include "hardware_info_collector.h"
#include "pmu_data_consumer.h"
#include "pmu_csv_writer.h"
#include "section_config.h"

#include <cstddef>
#include <memory>
#include <mutex>

namespace npu_compute {

inline constexpr int kInitializeFailed = -1;

class NpuComputeRuntime {
public:
    static NpuComputeRuntime& Instance();

    int Initialize();
    void Stop() noexcept;
    int ShutdownAfterPtiDrain();

private:
    static void HardwareInfoTriggerCallback(
        void* userData, aclptiCallbackDomain domain, aclptiCallbackId cbid,
        const aclptiCallbackData* callbackData) noexcept;

    void DisableHardwareCallbacks() noexcept;
    aclptiResult ProcessPmuData(std::shared_ptr<const aclptiPmuDataResult> result);

    std::mutex mutex_;
    std::shared_ptr<PmuDataConsumer> pmu_consumer_;
    aclptiSubscribeHandle subscriber_ = nullptr;
    std::size_t enabled_hardware_callback_count_ = 0;
    bool csv_frequency_override_ = false;
    bool csv_hardware_metadata_loaded_ = false;
    SectionConfig section_config_;
    PmuCsvConfig csv_config_;
    HardwareInfoCollector hardware_info_collector_;
};

} // namespace npu_compute

#endif // NPU_COMPUTE_SRC_NPU_COMPUTE_NPU_COMPUTE_RUNTIME_H_
