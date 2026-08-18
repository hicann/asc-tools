/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_STAGING_DIRECTORY_H_
#define NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_STAGING_DIRECTORY_H_

#include <string>

namespace npu_compute::compute_launcher {

class StagingDirectory {
public:
    static bool Create(StagingDirectory* result, std::string* error);

    const std::string& Path() const noexcept;

private:
    std::string path_;
};

} // namespace npu_compute::compute_launcher

#endif // NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_STAGING_DIRECTORY_H_
