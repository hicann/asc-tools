/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rep_decoder.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

namespace npu_compute::compute_launcher {
namespace {

bool Fail(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

uint16_t ReadLe16(const uint8_t* data) { return static_cast<uint16_t>(data[0]) | static_cast<uint16_t>(data[1]) << 8U; }

uint32_t ReadLe32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) | static_cast<uint32_t>(data[1]) << 8U |
           static_cast<uint32_t>(data[2]) << 16U | static_cast<uint32_t>(data[3]) << 24U;
}

uint64_t ReadLe64(const uint8_t* data)
{
    uint64_t value = 0;
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        value |= static_cast<uint64_t>(data[index]) << (index * 8U);
    }
    return value;
}

bool HasMagic(const uint8_t* data) { return std::equal(kNpuRepMagic.begin(), kNpuRepMagic.end(), data); }

bool IsKnownFileType(NpuRepFileType type)
{
    switch (type) {
        case NpuRepFileType::NpuRep:
        case NpuRepFileType::Json:
        case NpuRepFileType::Jsonl:
        case NpuRepFileType::Csv:
        case NpuRepFileType::Sqlite3:
        case NpuRepFileType::Protobuf:
            return true;
    }
    return false;
}

bool ReadFileName(const uint8_t* data, std::string* name, std::string* error)
{
    const char* begin = reinterpret_cast<const char*>(data);
    const char* end = static_cast<const char*>(std::memchr(begin, '\0', kNpuRepFileNameSize));
    if (end == nullptr) {
        return Fail("rep file info name is not null terminated", error);
    }
    if (end == begin) {
        return Fail("rep file info name is empty", error);
    }
    name->assign(begin, end);
    if (*name == "." || *name == "..") {
        return Fail("rep file info name is unsafe", error);
    }
    if (name->find('/') != std::string::npos || name->find('\\') != std::string::npos) {
        return Fail("rep file info name contains a path separator", error);
    }
    return true;
}

} // namespace

bool DecodeRep(const std::vector<uint8_t>& encoded, DecodedRep* decoded, std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (decoded == nullptr) {
        return Fail("decoded rep output is null", error);
    }
    *decoded = {};

    try {
        if (encoded.size() < kNpuRepHeadSize) {
            return Fail("rep is shorter than its header", error);
        }
        const uint8_t* head = encoded.data();
        if (!HasMagic(head)) {
            return Fail("invalid rep header magic", error);
        }
        const uint32_t version = ReadLe32(head + 8U);
        const uint16_t origin = ReadLe16(head + 12U);
        const uint16_t head_length = ReadLe16(head + 14U);
        const uint32_t file_count = ReadLe32(head + 16U);
        const uint32_t file_info_length = ReadLe32(head + 20U);
        const uint32_t reserved = ReadLe32(head + 24U);
        const uint64_t rep_length = ReadLe64(head + 28U);

        if (version != kNpuRepVersion) {
            return Fail("unsupported rep version", error);
        }
        if (origin != kNpuRepOrigin) {
            return Fail("unsupported rep origin", error);
        }
        if (head_length != kNpuRepHeadSize) {
            return Fail("invalid rep header length", error);
        }
        if (file_info_length != kNpuRepFileInfoSize) {
            return Fail("invalid rep file info length", error);
        }
        if (reserved != 0U) {
            return Fail("rep header reserved field is not zero", error);
        }
        if (rep_length != encoded.size()) {
            return Fail("rep length does not match input size", error);
        }

        if (file_count > (std::numeric_limits<uint64_t>::max() - head_length) / file_info_length) {
            return Fail("rep file info table length overflows", error);
        }
        const uint64_t payload_start = head_length + static_cast<uint64_t>(file_count) * file_info_length;
        if (payload_start > encoded.size()) {
            return Fail("rep file info table exceeds input", error);
        }

        decoded->version = version;
        decoded->origin = origin;
        decoded->entries.reserve(file_count);
        std::unordered_set<std::string> names;
        names.reserve(file_count);
        uint64_t expected_payload_offset = payload_start;
        for (uint32_t index = 0; index < file_count; ++index) {
            const uint64_t info_offset = head_length + static_cast<uint64_t>(index) * file_info_length;
            const uint8_t* info = encoded.data() + info_offset;
            if (!HasMagic(info)) {
                *decoded = {};
                return Fail("invalid rep file info magic", error);
            }

            DecodedRepEntry entry;
            if (!ReadFileName(info + 8U, &entry.file_name, error)) {
                *decoded = {};
                return false;
            }
            if (!names.insert(entry.file_name).second) {
                *decoded = {};
                return Fail("rep contains duplicate file names: " + entry.file_name, error);
            }

            entry.file_type = static_cast<NpuRepFileType>(ReadLe16(info + 136U));
            if (!IsKnownFileType(entry.file_type)) {
                *decoded = {};
                return Fail("rep entry has an unknown file type", error);
            }
            if (ReadLe16(info + 138U) != 0U || ReadLe32(info + 140U) != 0U) {
                *decoded = {};
                return Fail("rep file info reserved field is not zero", error);
            }

            const uint64_t file_length = ReadLe64(info + 144U);
            const uint64_t file_offset = ReadLe64(info + 152U);
            if (file_offset != expected_payload_offset || file_offset > encoded.size() ||
                file_length > encoded.size() - file_offset) {
                *decoded = {};
                return Fail("invalid rep file payload range", error);
            }
            const auto payload_begin = encoded.begin() + static_cast<std::size_t>(file_offset);
            entry.payload.assign(payload_begin, payload_begin + static_cast<std::size_t>(file_length));
            expected_payload_offset += file_length;
            decoded->entries.push_back(std::move(entry));
        }

        if (expected_payload_offset != encoded.size()) {
            *decoded = {};
            return Fail("rep contains unreferenced payload bytes", error);
        }
        return true;
    } catch (const std::bad_alloc&) {
        *decoded = {};
        return Fail("allocate decoded rep data failed", error);
    }
}

} // namespace npu_compute::compute_launcher
