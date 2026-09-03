// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#ifndef ACLSAN_CTRLBIN_SERIALIZE_H
#define ACLSAN_CTRLBIN_SERIALIZE_H

#include <string>
#include <type_traits>
#include <algorithm>

/**
 * @brief 将类型 T 的值序列化为字符串
 * @constraint 此处类型 T 必须为 POD 类型
 * @tparam Strings 指定返回的字符串类型，此处可以是任意包含 char 类型的容器，
 * 但有以下约束：
 * 1. 需要实现 Strings::Strings(char const *, char const *) 构造函数
 */
template <
    typename Strings = std::string, typename T,
    typename = typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
inline Strings Serialize(const T& val)
{
    auto valPtr = static_cast<char const*>(static_cast<void const*>(&val));
    return Strings(valPtr, valPtr + sizeof(T));
}

/**
 * @brief 将若干个值序列化为字符串，并拼接成一个字符串
 * @constraint 此处类型 T 和 Ts... 必须为 POD 类型
 * @tparam Strings 指定返回的字符串类型，此处可以是任意包含 char 类型的容器，
 * 但有以下约束：
 * 1. 需要实现 Strings::Strings(char const *, char const *) 构造函数
 * 2. 容器需要实现 operator+ 实现拼接的语义
 */
template <
    typename Strings = std::string, typename T, typename... Ts,
    typename = typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
inline Strings Serialize(const T& val, const Ts&... vals)
{
    return Serialize(val) + Serialize(vals...);
}

/**
 * @brief 将字符串反序列化为类型 T 的值
 * @constraint 此处类型 T 必须为 POD 类型
 * @tparam Strings 指定返回的字符串类型，此处可以是任意包含 char 类型的容器，
 * 但有以下约束：
 * 1. 需要实现 Strings::size() -> std::size 函数用于获取容器长度
 * 2. 需要实现 Strings::data() -> char const * 函数用于获取容器的首迭代器
 */
template <typename Strings, typename T, typename = typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
inline bool Deserialize(const Strings& msg, T& val)
{
    constexpr std::size_t size = sizeof(T);
    if (msg.size() < size) {
        return false;
    }
    std::copy_n(msg.data(), size, static_cast<char*>(static_cast<void*>(&val)));
    return true;
}

#endif // ACLSAN_CTRLBIN_SERIALIZE_H
