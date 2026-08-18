/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_WRITER_H_
#define NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_WRITER_H_

#include <filesystem>
#include <string>
#include <string_view>

namespace npu_compute {

enum class PublishResult {
    Published,
    AlreadyPublished,
    Failed,
};

PublishResult PublishHardwareInfoJsonl(
    const std::filesystem::path& outputDirectory, std::string_view jsonl, std::string* error);

} // namespace npu_compute

#endif // NPU_COMPUTE_SRC_NPU_COMPUTE_HARDWARE_INFO_WRITER_H_
