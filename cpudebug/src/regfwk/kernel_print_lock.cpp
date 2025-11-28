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
 * \file kernel_print_lock.cpp
 * \brief
 */
#include <pthread.h>
#include <sys/mman.h>
#include <cstdint>
#include <cerrno>
#include <csignal>
#include "stub_def.h"

namespace AscendC {
KernelPrintLock* KernelPrintLock::printLock = nullptr;
void KernelPrintLock::Init()
{
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_rwlock_init(&lock, &attr);
}

void KernelPrintLock::UnInit()
{
    pthread_rwlock_destroy(&lock);
    pthread_rwlockattr_destroy(&attr);
}

int KernelPrintLock::Lock()
{
    return pthread_rwlock_wrlock(&lock);
}
int KernelPrintLock::Unlock()
{
    return pthread_rwlock_unlock(&lock);
}

KernelPrintLock* KernelPrintLock::CreateLock()
{
    if (printLock == nullptr) {
        printLock = (KernelPrintLock*)mmap(nullptr, sizeof(KernelPrintLock), PROT_READ | PROT_WRITE,
            MAP_SHARED | MAP_ANON, -1, 0);
        if (static_cast<int>(reinterpret_cast<intptr_t>(printLock)) == -1) {
            printf("CreateLock Fail : KernelPrintLock map fail. Error Code : %d", errno);
            raise(SIGABRT);
        } else {
            printLock->Init();
        }
    }
    return printLock;
}
void KernelPrintLock::FreeLock()
{
    if (printLock == nullptr) {
        return;
    }
    printLock->UnInit();
    if (munmap(printLock, sizeof(KernelPrintLock)) == -1) {
        printf("FreeLock Fail : KernelPrintLock Unmap fail. Error Code : %d", errno);
    }
    printLock = nullptr;
}
KernelPrintLock::~KernelPrintLock()
{
    if (printLock == nullptr) {
        return;
    }
    pthread_rwlock_destroy(&lock);
    pthread_rwlockattr_destroy(&attr);
}
} // namespace AscendC