/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_REP_REPORT_WRITER_H_
#define NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_REP_REPORT_WRITER_H_

#include "report_name.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <sys/types.h>

namespace npu_compute::compute_launcher {

using ReportWriteOperation = ssize_t (*)(int descriptor, const void* data, std::size_t size, void* context);
using ReportDescriptorOperation = int (*)(int descriptor, void* context);
using ReportRenameOperation = int (*)(const char* source, const char* target, void* context);

struct ReportFileOperations {
    ReportWriteOperation write = nullptr;
    ReportDescriptorOperation sync = nullptr;
    ReportDescriptorOperation close = nullptr;
    ReportRenameOperation rename = nullptr;
    void* context = nullptr;
};

bool PublishRepReport(const std::vector<uint8_t>& encoded, const ReportTarget& target, std::string* error);

bool PublishRepReportWithOperations(
    const std::vector<uint8_t>& encoded, const ReportTarget& target, const ReportFileOperations& operations,
    std::string* error);

} // namespace npu_compute::compute_launcher

#endif // NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_REP_REPORT_WRITER_H_
