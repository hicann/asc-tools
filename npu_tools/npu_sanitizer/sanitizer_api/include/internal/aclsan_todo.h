/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_TODO_H
#define ACLSAN_TODO_H

// 1. 场景有<<<>>>在main调用前就执行__constructor__类型函数调用，
//    导致aclrt接口在main执行前就被调用
//    因此需要一个__constructor__类型函数，抢先把aclrt接口hook成功
//    通过.so的加载顺序来保证会先调用我们的这个initialize函数
//    调用方式1：命令行调用 npucheck 可执行                        不涉及这个函数
//    调用方式2：用户在自己的可执行中调用Subscribe、EnableCallback  需要这个函数
// 生命周期由 aclsanSubscribe/aclsanUnsubscribe 管理。

#endif
