/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_IMPORT_OUTPUT_DIRECTORY_H_
#define NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_IMPORT_OUTPUT_DIRECTORY_H_

#include <filesystem>
#include <optional>
#include <string>

namespace npu_compute::compute_launcher {

class ImportOutputDirectory {
public:
    ImportOutputDirectory() = default;
    ~ImportOutputDirectory();

    ImportOutputDirectory(const ImportOutputDirectory&) = delete;
    ImportOutputDirectory& operator=(const ImportOutputDirectory&) = delete;

    static bool Create(
        const std::filesystem::path& inputRep, const std::optional<std::string>& exportPath,
        ImportOutputDirectory* directory, std::string* error);

    const std::filesystem::path& TemporaryPath() const;
    const std::filesystem::path& FinalPath() const;
    bool Publish(std::string* error);

private:
    void CleanupTemporaryDirectory() noexcept;

    std::filesystem::path temporaryPath_;
    std::filesystem::path finalPath_;
};

} // namespace npu_compute::compute_launcher

#endif // NPU_COMPUTE_SRC_COMPUTE_LAUNCHER_IMPORT_OUTPUT_DIRECTORY_H_
