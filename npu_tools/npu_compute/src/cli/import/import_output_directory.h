/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef NPU_TOOLS_NPU_COMPUTE_SRC_CLI_IMPORT_IMPORT_OUTPUT_DIRECTORY_H
#define NPU_TOOLS_NPU_COMPUTE_SRC_CLI_IMPORT_IMPORT_OUTPUT_DIRECTORY_H

#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>
#include <optional>
#include <string>

namespace npucompute::cli {

class ImportOutputDirectory {
public:
    ImportOutputDirectory() = default;
    ~ImportOutputDirectory();

    ImportOutputDirectory(const ImportOutputDirectory&) = delete;
    ImportOutputDirectory& operator=(const ImportOutputDirectory&) = delete;

    static bool Create(
        const boost::filesystem::path& inputRep, const std::optional<std::string>& exportPath,
        ImportOutputDirectory* directory, std::string* error);

    const boost::filesystem::path& TemporaryPath() const;
    const boost::filesystem::path& FinalPath() const;
    bool Publish(std::string* error);

private:
    void CleanupTemporaryDirectory() noexcept;

    boost::filesystem::path temporaryPath_;
    boost::filesystem::path finalPath_;
};

} // namespace npucompute::cli

#endif // NPU_TOOLS_NPU_COMPUTE_SRC_CLI_IMPORT_IMPORT_OUTPUT_DIRECTORY_H
