/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_PROBE_IMAGE_TRANSFORMER_H_
#define ACLSAN_PROBE_IMAGE_TRANSFORMER_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace aclsan::probe {

struct ImageTransformConfig {
    std::string probeObject;
    std::string ctrlBinary;
    std::string symbolOrdering;
    std::string workRoot;
    uint32_t argumentBytes = 0;
};

struct ImageTransformResult {
    std::vector<uint8_t> image;
    std::string originalImage;
    std::string sessionDirectory;
    std::string tuneLog;
};

using CommandRunner =
    std::function<bool(const std::vector<std::string>& command, const std::string& logPath, std::string& error)>;

using ImageTransformFunction = bool (*)(
    const void* data, size_t length, const ImageTransformConfig& config, ImageTransformResult& result,
    std::string& error);

bool RunCommand(const std::vector<std::string>& command, const std::string& logPath, std::string& error);

bool TransformDeviceImageWithRunner(
    const void* data, size_t length, const ImageTransformConfig& config, ImageTransformResult& result,
    std::string& error, const CommandRunner& runner);

bool TransformDeviceImage(
    const void* data, size_t length, const ImageTransformConfig& config, ImageTransformResult& result,
    std::string& error);

} // namespace aclsan::probe

#endif // ACLSAN_PROBE_IMAGE_TRANSFORMER_H_
