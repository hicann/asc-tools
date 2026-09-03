/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_NPU_COMPUTE_PMU_DATA_CONSUMER_H_
#define NPU_COMPUTE_SRC_NPU_COMPUTE_PMU_DATA_CONSUMER_H_

#include "aclpti/aclpti_data.h"

#include <functional>
#include <memory>

namespace npu_compute {

class PmuDataConsumer final {
public:
    using Processor = std::function<aclptiResult(std::shared_ptr<const aclptiProfilingDataResult>)>;

    static std::shared_ptr<PmuDataConsumer> Create(Processor processor);
    ~PmuDataConsumer();

    PmuDataConsumer(const PmuDataConsumer&) = delete;
    PmuDataConsumer& operator=(const PmuDataConsumer&) = delete;

    aclptiResult Start();
    aclptiResult Submit(std::shared_ptr<const aclptiProfilingDataResult> result);
    aclptiResult ShutdownAndDrain();

private:
    class Impl;

    explicit PmuDataConsumer(Processor processor);
    std::unique_ptr<Impl> impl_;
};

} // namespace npu_compute

#endif // NPU_COMPUTE_SRC_NPU_COMPUTE_PMU_DATA_CONSUMER_H_
