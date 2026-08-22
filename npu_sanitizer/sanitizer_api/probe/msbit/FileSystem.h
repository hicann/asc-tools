/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef MSBIT_FILESYSTEM_H
#define MSBIT_FILESYSTEM_H

#include <string>
#include <sys/stat.h>

const mode_t SAVE_DATA_FILE_AUTHORITY = 0640;

inline bool CheckWriteFilePathValid(std::string& path) { return !path.empty(); }

inline bool Chmod(const std::string& path, mode_t /*mode*/)
{
    (void)path;
    return true;
}

#endif // MSBIT_FILESYSTEM_H
