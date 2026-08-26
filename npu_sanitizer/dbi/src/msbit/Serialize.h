/* -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * ------------------------------------------------------------------------- */


#ifndef __UTILS__SERIALIZER_H__
#define __UTILS__SERIALIZER_H__

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
template<typename Strings = std::string, typename T,
         typename = typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
inline Strings Serialize(const T &val)
{
    auto valPtr = static_cast<char const *>(static_cast<void const *>(&val));
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
template<typename Strings = std::string, typename T, typename... Ts,
         typename = typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
inline Strings Serialize(const T &val, const Ts &... vals)
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
template<typename Strings, typename T,
         typename = typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
inline bool Deserialize(const Strings &msg, T &val)
{
    constexpr std::size_t size = sizeof(T);
    if (msg.size() < size) {
        return false;
    }
    std::copy_n(msg.data(), size, static_cast<char *>(static_cast<void *>(&val)));
    return true;
}

#endif  // __UTILS__SERIALIZER_H__
