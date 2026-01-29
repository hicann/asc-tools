/**
* Copyright (c) 2025 Huawei Technologies Co., Ltd.
* This program is free software, you can redistribute it and/or modify it under the terms and conditions of
* CANN Open Software License Agreement Version 2.0 (the "License").
* Please refer to the License for details. You may not use this file except in compliance with the License.
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
* See LICENSE in the root of the software repository for the full text of the License.
*/

/*!
 * \file stub_reg.cpp
 * \brief
 */
#include "stub_reg.h"
#include <string>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <csignal>
#include "securec.h"

const char* g_regStubs[INTRI_TYPE_MAX] {
    "AscendC",
    "cceprint",
    "npuchk",
};

namespace AscendC {
const int SYM_LEN_MAX = 511;

void StubReg(IntriTypeT type, const char* stub)
{
    g_regStubs[type] = stub;
}

void StubInit(void)
{
    char buf[SYM_LEN_MAX + 1];
    static bool gStubInited = false;
    if (gStubInited) {
        return;
    }
    gStubInited = true;

    int32_t logfd = open("stub_reg.log", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    for (int32_t s = 0; s < INTRI_TYPE_MAX; s++) {
        const char* stub = g_regStubs[s];
        if (stub == nullptr) {
            continue;
        }
        int32_t slen = strnlen(stub, SYM_LEN_MAX);
        for (int32_t i = 0; i < INTRI_FMT_NUM; i++) {
            IntriFmtT* fmt = IntriFmtGet(i);
            int ret = snprintf_s(buf, SYM_LEN_MAX, SYM_LEN_MAX, fmt->fmt, slen, stub);
            if (ret <= 0) {
                std::cout << "Get intri format error!" << std::endl;
                raise(SIGABRT);
            }
            PfIntriFun fun = (PfIntriFun)dlsym(RTLD_DEFAULT, buf);
            IntriFunAdd(i, static_cast<IntriTypeT>(s), fun);
            dprintf(logfd, "%s: [%s] -> %p\n", stub, buf, fun);
#if defined (__NPU_ARCH__) && ((__NPU_ARCH__ == 3003) || (__NPU_ARCH__ == 3103) || (__NPU_ARCH__ == 3113))
            memset_s(buf, SYM_LEN_MAX + 1, '\0', SYM_LEN_MAX + 1);
#endif
        }
    }
    close(logfd);
    logfd = -1;
}
} // namespace AscendC