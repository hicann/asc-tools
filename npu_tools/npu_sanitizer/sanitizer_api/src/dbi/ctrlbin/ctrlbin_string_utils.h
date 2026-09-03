/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/* Minimal string helpers for the standalone ctrl.bin writer.
 * Provides only the Join template used by ctrlbin_writer.cpp.
 */
#ifndef ACLSAN_CTRLBIN_STRING_UTILS_H
#define ACLSAN_CTRLBIN_STRING_UTILS_H

#include <string>

template <typename Iterator>
inline std::string Join(Iterator beg, Iterator end, std::string const& sep = " ")
{
    std::string ret;
    if (beg == end) {
        return ret;
    }
    ret = *beg++;
    for (; beg != end; ++beg) {
        ret.append(sep);
        ret.append(*beg);
    }
    return ret;
}

#endif // ACLSAN_CTRLBIN_STRING_UTILS_H
