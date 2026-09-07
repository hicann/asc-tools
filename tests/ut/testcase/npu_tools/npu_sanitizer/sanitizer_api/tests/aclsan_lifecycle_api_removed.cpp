/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cassert>
#include <dlfcn.h>

int main()
{
    void* library = dlopen("./libacl_san.so", RTLD_NOW | RTLD_LOCAL);
    assert(library != nullptr);
    assert(dlsym(library, "aclsanInitialize") == nullptr);
    assert(dlsym(library, "aclsanFinalize") == nullptr);
    assert(dlsym(library, "NpuCheckInstallAclHooks") == nullptr);
    assert(dlsym(library, "NpuCheckUninstallAclHooks") == nullptr);
    assert(dlclose(library) == 0);
    return 0;
}
