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
 * \file kernel_raise_signal.h
 * \brief
 */
#ifndef ASCENDC_KERNEL_RAISE_SIGNAL_H
#define ASCENDC_KERNEL_RAISE_SIGNAL_H
#include <csignal>
#include <cstdint>

namespace AscendC {

class KernelRaise {
public:
    static KernelRaise& GetInstance()
    {
        static KernelRaise instance;
        return instance;
    }

    void Raise(const int sig)
    {
        count += 1;
        if (status) {
            (void)raise(sig);
        }
    }

    uint64_t GetRaiseCount() const
    {
        return count;
    }

    void SetRaiseMode(const bool isRaise)
    {
        this->status = isRaise;
    }

private:
    KernelRaise() = default;
    ~KernelRaise() = default;
    KernelRaise(const KernelRaise&) = delete;
    KernelRaise& operator=(const KernelRaise&) = delete;

private:
    uint64_t count = 0;
    bool status = true;
};

}

#endif // ASCENDC_KERNEL_RAISE_SIGNAL_H
