/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "rep_directory_packer.h"

#include "collection_file_validator.h"
#include "rep_encoder.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <new>
#include <string>
#include <unordered_set>
#include <utility>

namespace npu_compute::compute_launcher {
namespace {

constexpr char kHardwareInfoLockName[] = ".hardware_info.lock";

struct DirectoryItem {
    std::filesystem::path path;
    std::string stored_name;
    bool is_directory = false;
};

bool Fail(const std::string& message, std::string* error)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

bool IsTemporaryName(const std::string& name)
{
    if (name.find(".tmp.") != std::string::npos) {
        return true;
    }
    constexpr char kTemporarySuffix[] = ".tmp";
    return name.size() >= sizeof(kTemporarySuffix) - 1U &&
           name.compare(
               name.size() - (sizeof(kTemporarySuffix) - 1U), sizeof(kTemporarySuffix) - 1U, kTemporarySuffix) == 0;
}

bool StoredNameLess(const DirectoryItem& left, const DirectoryItem& right)
{
    return std::lexicographical_compare(
        left.stored_name.begin(), left.stored_name.end(), right.stored_name.begin(), right.stored_name.end(),
        [](unsigned char left_byte, unsigned char right_byte) { return left_byte < right_byte; });
}

bool ReadFile(const std::filesystem::path& path, std::vector<uint8_t>* payload, std::string* error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return Fail("open collection file for packing failed: " + path.string(), error);
    }
    payload->assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    if (input.bad()) {
        payload->clear();
        return Fail("read collection file for packing failed: " + path.string(), error);
    }
    if (payload->empty()) {
        return Fail("collection file became empty while packing: " + path.string(), error);
    }
    return true;
}

bool CollectDirectoryItems(
    const std::filesystem::path& directory, std::vector<DirectoryItem>* items, std::string* error)
{
    std::error_code iterator_error;
    std::filesystem::directory_iterator iterator(directory, iterator_error);
    const std::filesystem::directory_iterator end;
    if (iterator_error) {
        return Fail("open collection directory failed: " + directory.string() + ": " + iterator_error.message(), error);
    }

    for (; iterator != end; iterator.increment(iterator_error)) {
        if (iterator_error) {
            return Fail(
                "iterate collection directory failed: " + directory.string() + ": " + iterator_error.message(), error);
        }
        const std::filesystem::path path = iterator->path();
        const std::string name = path.filename().string();
        std::error_code status_error;
        const std::filesystem::file_status status = std::filesystem::symlink_status(path, status_error);
        if (status_error) {
            return Fail(
                "inspect collection directory item failed: " + path.string() + ": " + status_error.message(), error);
        }
        if (std::filesystem::is_symlink(status)) {
            return Fail("symbolic link is not allowed in collection directory: " + path.string(), error);
        }
        if (name == kHardwareInfoLockName) {
            if (!std::filesystem::is_regular_file(status)) {
                return Fail("HardwareInfo lock path is not a regular file: " + path.string(), error);
            }
            continue;
        }
        if (IsTemporaryName(name)) {
            return Fail("temporary collection item remains: " + path.string(), error);
        }

        DirectoryItem item;
        item.path = path;
        if (std::filesystem::is_directory(status)) {
            item.is_directory = true;
            item.stored_name = name + ".npu.rep";
        } else if (std::filesystem::is_regular_file(status)) {
            item.stored_name = name;
        } else {
            return Fail("unsupported collection directory item: " + path.string(), error);
        }
        items->push_back(std::move(item));
    }
    if (iterator_error) {
        return Fail(
            "iterate collection directory failed: " + directory.string() + ": " + iterator_error.message(), error);
    }

    std::unordered_set<std::string> stored_names;
    stored_names.reserve(items->size());
    for (const DirectoryItem& item : *items) {
        if (!stored_names.insert(item.stored_name).second) {
            return Fail("collection stored name conflict: " + item.stored_name, error);
        }
    }
    std::sort(items->begin(), items->end(), StoredNameLess);
    return true;
}

bool PackDirectoryInternal(const std::filesystem::path& directory, std::vector<uint8_t>* encoded, std::string* error)
{
    std::vector<DirectoryItem> items;
    if (!CollectDirectoryItems(directory, &items, error)) {
        return false;
    }

    std::vector<RepEntry> entries;
    entries.reserve(items.size());
    for (const DirectoryItem& item : items) {
        RepEntry entry;
        entry.file_name = item.stored_name;
        if (item.is_directory) {
            entry.file_type = NpuRepFileType::NpuRep;
            if (!PackDirectoryInternal(item.path, &entry.payload, error)) {
                return false;
            }
        } else {
            if (!ResolveCollectionFileType(item.path, &entry.file_type, error) ||
                !ValidateCollectionFile(item.path, entry.file_type, error) ||
                !ReadFile(item.path, &entry.payload, error)) {
                return false;
            }
        }
        entries.push_back(std::move(entry));
    }

    std::vector<uint8_t> complete_rep;
    if (!EncodeRep(entries, &complete_rep, error)) {
        return false;
    }
    *encoded = std::move(complete_rep);
    return true;
}

} // namespace

bool PackDirectoryToRep(const std::filesystem::path& directory, std::vector<uint8_t>* encoded, std::string* error)
{
    if (error != nullptr) {
        error->clear();
    }
    if (encoded == nullptr) {
        return Fail("encoded rep output is null", error);
    }
    encoded->clear();

    try {
        std::error_code status_error;
        const std::filesystem::file_status status = std::filesystem::symlink_status(directory, status_error);
        if (status_error) {
            return Fail(
                "inspect collection directory failed: " + directory.string() + ": " + status_error.message(), error);
        }
        if (!std::filesystem::is_directory(status)) {
            return Fail("collection path is not a directory: " + directory.string(), error);
        }
        return PackDirectoryInternal(directory, encoded, error);
    } catch (const std::bad_alloc&) {
        encoded->clear();
        return Fail("allocate recursive rep buffer failed", error);
    }
}

} // namespace npu_compute::compute_launcher
