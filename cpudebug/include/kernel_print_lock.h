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
 * \file kernel_print_lock.h
 * \brief
 */
#ifndef ASCENDC_KERNEL_PRINT_LOCK_H
#define ASCENDC_KERNEL_PRINT_LOCK_H
#include <pthread.h>

namespace AscendC {
class KernelPrintLock {
public:
    int Lock();
    int Unlock();
    void UnInit();
    static void FreeLock();
    static KernelPrintLock* CreateLock();
    static KernelPrintLock* GetLock()
    {
        if (printLock == nullptr) {
            printLock = CreateLock();
        }
        return printLock;
    }
private:
    KernelPrintLock();
    ~KernelPrintLock();

private:
    pthread_rwlock_t lock;
    pthread_rwlockattr_t attr;
    inline void Init();
    static KernelPrintLock* printLock;
};
}

#endif // ASCENDC_KERNEL_PRINT_LOCK_H
