/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

// acl_pti 单测族的最小 profapi 桩（"仅仅能跑"）。
//
// 单测族不再链接 .so 桩；场景行为由各用例内的 mock 定义还原：
//   - MsprofStart / MsprofStop / MsprofRegisterDataCallback 由用例 mock 提供；
//   - 本文件只补齐链接必需的装载器入口（runtime 桩的 aclrtInit 传递引用），
//     真实装载行为由 DSO 集成用例（prof_api_loader / runtime_callback / pytest 探针）覆盖。

#include "injection/prof_api_stub.h"

#include <cstdlib>

extern "C" int ProfApiLoadApiInjectionFromEnv() { return 0; }
