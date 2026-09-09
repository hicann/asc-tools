/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_TOOLS_NPU_COMPUTE_SRC_CLI_IMPORT_IMPORTED_PROFILE_RESULTS_H
#define NPU_TOOLS_NPU_COMPUTE_SRC_CLI_IMPORT_IMPORTED_PROFILE_RESULTS_H

#include <cstdint>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <vector>

#include "report/rep_format.h"

namespace npucompute::cli {

struct ImportedProfileEntry {
    std::string name;
    NpuRepFileType type = NpuRepFileType::NpuRep;
    std::vector<uint8_t> payload;
    std::vector<ImportedProfileEntry> children;
};

bool ReadImportedProfileResults(
    const boost::filesystem::path& input_path, std::vector<ImportedProfileEntry>* results, std::string* error);

bool UnpackImportedProfileResults(
    const std::vector<ImportedProfileEntry>& results, const boost::filesystem::path& output_directory,
    std::string* error);

} // namespace npucompute::cli

#endif // NPU_TOOLS_NPU_COMPUTE_SRC_CLI_IMPORT_IMPORTED_PROFILE_RESULTS_H
