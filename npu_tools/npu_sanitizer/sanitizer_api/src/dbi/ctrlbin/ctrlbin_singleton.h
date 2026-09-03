/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#pragma once

template <typename T, bool threadLocal, typename... Args>
class Singleton {
public:
    inline static T& Instance(Args&&... args);
    Singleton(Singleton const&) = delete;
    Singleton& operator=(Singleton const&) = delete;

protected:
    Singleton(void) = default;
    ~Singleton(void) = default;
};

template <typename T, bool threadLocal, typename... Args>
T& Singleton<T, threadLocal, Args...>::Instance(Args&&... args)
{
    static T instance(std::forward<Args>(args)...);
    return instance;
}

template <typename T, typename... Args>
class Singleton<T, true, Args...> {
public:
    inline static T& Instance(Args&&... args);
    Singleton(Singleton const&) = delete;
    Singleton& operator=(Singleton const&) = delete;

protected:
    Singleton(void) = default;
    ~Singleton(void) = default;
};

template <typename T, typename... Args>
T& Singleton<T, true, Args...>::Instance(Args&&... args)
{
    thread_local static T instance(std::forward<Args>(args)...);
    return instance;
}
