/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_COLLECTION_FILE_VALIDATOR_H_
#define NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_COLLECTION_FILE_VALIDATOR_H_

#include "rep_format.h"

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <string>

namespace npu_compute::compute_launcher {

bool ResolveCollectionFileType(const boost::filesystem::path& path, NpuRepFileType* type, std::string* error);

bool ValidateCollectionFile(const boost::filesystem::path& path, NpuRepFileType type, std::string* error);

} // namespace npu_compute::compute_launcher

#endif // NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_COLLECTION_FILE_VALIDATOR_H_
