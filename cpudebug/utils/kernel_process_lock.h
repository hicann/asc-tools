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
 * \file kernel_process_lock.h
 * \brief
 */
#ifndef __KERNEL_PROCESS_LOCK_H__
#define __KERNEL_PROCESS_LOCK_H__
#ifdef ASCENDC_CPU_DEBUG
#include <pthread.h>
#include <sys/types.h>
#include <cstdlib>

namespace AscendC {
class ProcessLock {
public:
    int Read();
    int Write();
    int Unlock();
    void UnInit();
    static void FreeLock();
    ProcessLock();
    static ProcessLock* CreateLock();

    static ProcessLock* GetProcessLock()
    {
        if (processLock == nullptr) {
            processLock = CreateLock();
        }
        return processLock;
    }
    ~ProcessLock();

private:
    pthread_rwlock_t lock;
    pthread_rwlockattr_t attr;
    inline void Init();
    static ProcessLock* processLock;
};
} // namespace AscendC
#endif
#endif // __KERNEL_PROCESS_LOCK_H__
