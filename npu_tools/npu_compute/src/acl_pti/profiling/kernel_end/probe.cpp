/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
extern __attribute__((noinline, weak))[aicore] void __npu_compute_before_kernel_end(
    __gm__ unsigned char*, unsigned long long, unsigned int)
{
    asm volatile("bar.all" ::: "memory");
    asm volatile(".rept 32\n\tnop\n\t.endr");
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xd88));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xd99));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdaa));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdbb));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdcc));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xddd));
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdee));
    asm volatile("DFX_REGION.S %0" ::"l"(0xdff));
    asm volatile(".rept 3500\n\tnop\n\t.endr");
    asm volatile("DFX_REGION.S %0\n\tnop" ::"l"(0xdff));
}
