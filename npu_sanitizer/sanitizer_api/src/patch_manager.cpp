/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "api_core.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

namespace aclsan {
namespace {

void EnsureDir(const std::string& path)
{
    if (path.empty()) {
        return;
    }
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        std::cerr << "[aclsan-api] mkdir failed path=" << path << " errno=" << errno << "\n";
    }
}

std::string BaseName(const std::string& path)
{
    const std::size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool CopyFileOrWriteManifest(const std::string& src, const std::string& dst, uint32_t pipelineMask)
{
    std::ifstream input(src, std::ios::binary);
    if (input.good()) {
        std::ofstream output(dst, std::ios::binary);
        output << input.rdbuf();
        return output.good();
    }

    std::ofstream output(dst, std::ios::binary);
    output << "ACLSAN_DUMMY_PATCHED_OBJECT\n";
    output << "original=" << src << "\n";
    output << "pipelineMask=0x" << std::hex << pipelineMask << "\n";
    return output.good();
}

bool WriteMemoryImage(const void* imageData, uint64_t imageSize, const std::string& dst)
{
    std::ofstream output(dst, std::ios::binary);
    if (!output.good()) {
        return false;
    }
    output.write(static_cast<const char*>(imageData), static_cast<std::streamsize>(imageSize));
    return output.good();
}

AclsanBinaryImageData MakeFileImageView(const std::string& path, uint32_t flags)
{
    AclsanBinaryImageData image{};
    image.kind = ACLSAN_PATCH_IMAGE_FILE;
    image.flags = flags | ACLSAN_BINARY_IMAGE_FLAG_PATH_VALID;
    image.path = path.c_str();
    return image;
}

void CopyText(char* dst, std::size_t dstSize, const char* src)
{
    if (dst == nullptr || dstSize == 0) {
        return;
    }
    std::snprintf(dst, dstSize, "%s", src != nullptr ? src : "");
}

std::string SelectCacheDir(const AclsanPatchOptions* options, const AclsanLaunchConfig& config)
{
    if (options != nullptr && options->cacheDir != nullptr && options->cacheDir[0] != '\0') {
        return options->cacheDir;
    }
    if (config.probeCacheDir[0] != '\0') {
        return config.probeCacheDir;
    }
    if (config.workDir[0] != '\0') {
        return config.workDir;
    }
    return "/tmp";
}

} // namespace

AclsanStatus ApiCore::RegisterBuiltinPatchPipelines()
{
    const AclsanPatchPipelineDesc descs[] = {
        {ACLSAN_API_VERSION, sizeof(AclsanPatchPipelineDesc), ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG,
         ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC, "SET_WAIT_FLAG",
         "aclsan_dev_trace_set_wait_flag", "set_wait_flag", ACLSAN_RAW_ARG_MAX, 0},
        {ACLSAN_API_VERSION, sizeof(AclsanPatchPipelineDesc), ACLSAN_PATCH_PIPELINE_GET_RLS_BUF,
         ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_SYNC, "GET_RLS_BUF", "aclsan_dev_trace_get_rls_buf",
         "get_rls_buf", ACLSAN_RAW_ARG_MAX, 0},
        {ACLSAN_API_VERSION, sizeof(AclsanPatchPipelineDesc), ACLSAN_PATCH_PIPELINE_MTE2,
         ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS, "MTE2", "aclsan_dev_trace_mte2", "mte2",
         ACLSAN_RAW_ARG_MAX, 0},
        {ACLSAN_API_VERSION, sizeof(AclsanPatchPipelineDesc), ACLSAN_PATCH_PIPELINE_MTE3,
         ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS, "MTE3", "aclsan_dev_trace_mte3", "mte3",
         ACLSAN_RAW_ARG_MAX, 0},
        {ACLSAN_API_VERSION, sizeof(AclsanPatchPipelineDesc), ACLSAN_PATCH_PIPELINE_FIXPIPE,
         ACLSAN_CB_DOMAIN_DEVICE_INSTRUCTION, ACLSAN_CBID_DEVICE_MEMORY_ACCESS, "FIXPIPE", "aclsan_dev_trace_fixpipe",
         "fixpipe", ACLSAN_RAW_ARG_MAX, 0},
    };

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (const auto& desc : descs) {
        patchPipelines_[desc.pipeline] = desc;
    }
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::RegisterPatchImage(const AclsanPatchImageDesc* desc, uint64_t* patchImageId)
{
    if (desc == nullptr || patchImageId == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (desc->version != ACLSAN_API_VERSION || desc->size < sizeof(AclsanPatchImageDesc)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const uint64_t id = nextPatchImage_++;
    patchImages_[id] = *desc;
    *patchImageId = id;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::RegisterPatchPipeline(const AclsanPatchPipelineDesc* desc)
{
    if (desc == nullptr || desc->pipeline == ACLSAN_PATCH_PIPELINE_INVALID) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (desc->version != ACLSAN_API_VERSION || desc->size < sizeof(AclsanPatchPipelineDesc)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    patchPipelines_[desc->pipeline] = *desc;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::SetPatchOptions(const AclsanPatchOptions* options)
{
    if (options == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (options->version != ACLSAN_API_VERSION || options->size < sizeof(AclsanPatchOptions)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    patchOptions_ = *options;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::BuildPatchPlanForBinary(AclsanBinaryHandle, AclsanPatchPlanHandle* plan)
{
    if (plan == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    *plan = nextPatchPlan_++;
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::BuildDummyPatchResult(const std::string& originalPath, uint32_t pipelineMask, PatchResult& result)
{
    result.pipelineMask = pipelineMask;
    result.patchPlanId = nextPatchPlan_++;
    if (pipelineMask == 0) {
        result.patched = false;
        result.patchedPath = originalPath;
        return ACLSAN_STATUS_SUCCESS;
    }

    const std::string cacheDir = SelectCacheDir(&patchOptions_, config_);
    EnsureDir(cacheDir);
    result.patched = true;
    result.patchedPath = cacheDir + "/aclsan_dummy_patched_" + BaseName(originalPath);
    if (!CopyFileOrWriteManifest(originalPath, result.patchedPath, pipelineMask)) {
        return ACLSAN_STATUS_ERROR_IO;
    }

    const uint64_t binaryId = nextBinary_;
    const AclsanPatchPipeline pipelines[] = {
        ACLSAN_PATCH_PIPELINE_SET_WAIT_FLAG, ACLSAN_PATCH_PIPELINE_GET_RLS_BUF, ACLSAN_PATCH_PIPELINE_MTE2,
        ACLSAN_PATCH_PIPELINE_MTE3,          ACLSAN_PATCH_PIPELINE_FIXPIPE,
    };
    for (AclsanPatchPipeline pipeline : pipelines) {
        if ((pipelineMask & PipelineMask(pipeline)) == 0) {
            continue;
        }
        PatchSiteRecord site{};
        site.functionName = "<unknown>";
        site.opName = PipelineName(pipeline);
        site.sourceFile = "<unknown>";
        site.info.version = ACLSAN_API_VERSION;
        site.info.size = sizeof(site.info);
        site.info.siteId = static_cast<uint32_t>(patchSites_.size() + result.sites.size() + 1);
        site.info.pipeline = pipeline;
        site.info.binaryId = binaryId;
        site.info.functionId = 0;
        site.info.pc = 0x1000 + static_cast<uint64_t>(pipeline) * 0x10;
        site.info.functionName = site.functionName.c_str();
        site.info.opName = site.opName.c_str();
        site.info.sourceFile = site.sourceFile.c_str();
        site.info.sourceLine = 0;
        result.sites.push_back(site);
    }
    return ACLSAN_STATUS_SUCCESS;
}

void ApiCore::StorePatchSites(uint64_t binaryId, const std::vector<PatchSiteRecord>& sites)
{
    for (auto site : sites) {
        site.info.binaryId = binaryId;
        site.info.functionName = site.functionName.c_str();
        site.info.opName = site.opName.c_str();
        site.info.sourceFile = site.sourceFile.c_str();
        patchSites_[site.info.siteId] = site;
    }
}

AclsanStatus ApiCore::PatchBinaryFromImage(
    const AclsanPatchImageDesc* image, const AclsanPatchOptions* options, char* patchedPath, uint64_t patchedPathSize,
    AclsanPatchPlanHandle* plan)
{
    if (image == nullptr || patchedPath == nullptr || patchedPathSize == 0) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (image->version != ACLSAN_API_VERSION || image->size < sizeof(AclsanPatchImageDesc)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    if (options != nullptr && (options->version != ACLSAN_API_VERSION || options->size < sizeof(AclsanPatchOptions))) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }

    std::string originalPath;
    if (image->kind == ACLSAN_PATCH_IMAGE_FILE) {
        if (image->path == nullptr || image->path[0] == '\0') {
            return ACLSAN_STATUS_ERROR_INVALID_VALUE;
        }
        originalPath = image->path;
    } else if (image->kind == ACLSAN_PATCH_IMAGE_MEMORY) {
        if (image->imageData == nullptr || image->imageSize == 0) {
            return ACLSAN_STATUS_ERROR_INVALID_VALUE;
        }
    } else {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (options != nullptr) {
        patchOptions_ = *options;
    }

    if (image->kind == ACLSAN_PATCH_IMAGE_MEMORY) {
        const std::string cacheDir = SelectCacheDir(&patchOptions_, config_);
        EnsureDir(cacheDir);
        originalPath = cacheDir + "/aclsan_input_image_" + std::to_string(nextBinary_) + ".o";
        if (!WriteMemoryImage(image->imageData, image->imageSize, originalPath)) {
            return ACLSAN_STATUS_ERROR_IO;
        }
    }

    const uint32_t pipelineMask = activeHookPlan_.patchPipelineMask;
    AclsanPatchData data{};
    data.common.version = ACLSAN_API_VERSION;
    data.common.size = sizeof(data);
    data.common.apiName = "aclsanPatchBinaryFromImage";
    data.original = MakeFileImageView(
        originalPath, image->kind == ACLSAN_PATCH_IMAGE_MEMORY ? ACLSAN_BINARY_IMAGE_FLAG_MATERIALIZED_PATH :
                                                                 ACLSAN_BINARY_IMAGE_FLAG_NONE);
    if (image->kind == ACLSAN_PATCH_IMAGE_MEMORY) {
        data.original.kind = ACLSAN_PATCH_IMAGE_MEMORY;
        data.original.flags |= ACLSAN_BINARY_IMAGE_FLAG_DATA_VALID;
        data.original.imageData = image->imageData;
        data.original.imageSize = image->imageSize;
    }
    data.original.imageVersion = image->imageVersion;
    data.pipelineMask = pipelineMask;
    Dispatch(ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_BEGIN, &data);

    PatchResult result{};
    AclsanStatus status = BuildDummyPatchResult(originalPath, pipelineMask, result);
    if (status != ACLSAN_STATUS_SUCCESS) {
        data.common.result = static_cast<int>(status);
        Dispatch(ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_END, &data);
        return status;
    }

    const uint64_t binaryId = nextBinary_++;
    StorePatchSites(binaryId, result.sites);
    data.binaryId = binaryId;
    data.patchPlanId = result.patchPlanId;
    data.patched = MakeFileImageView(result.patchedPath, ACLSAN_BINARY_IMAGE_FLAG_MATERIALIZED_PATH);
    data.siteCount = static_cast<uint32_t>(result.sites.size());
    data.common.result = ACLSAN_STATUS_SUCCESS;
    if (plan != nullptr) {
        *plan = result.patchPlanId;
    }
    std::snprintf(patchedPath, static_cast<size_t>(patchedPathSize), "%s", result.patchedPath.c_str());
    Dispatch(ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_SITE_MAP_CREATED, &data);
    Dispatch(ACLSAN_CB_DOMAIN_PATCH, ACLSAN_CBID_PATCH_END, &data);
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::GetPatchSiteInfo(uint32_t siteId, AclsanPatchSiteInfo* info) const
{
    if (info == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = patchSites_.find(siteId);
    if (it == patchSites_.end()) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }
    *info = it->second.info;
    info->functionName = it->second.functionName.c_str();
    info->opName = it->second.opName.c_str();
    info->sourceFile = it->second.sourceFile.c_str();
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::SymbolizeDevicePc(
    const AclsanDevicePcQuery* query, char* payload, uint64_t payloadSize, uint64_t* payloadBytes) const
{
    if (query == nullptr || payloadBytes == nullptr) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (query->version != ACLSAN_API_VERSION || query->size < sizeof(AclsanDevicePcQuery)) {
        return ACLSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    *payloadBytes = 0;
    if (payload == nullptr || payloadSize == 0) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (query->siteId == 0) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }

    auto it = patchSites_.find(query->siteId);
    if (it == patchSites_.end()) {
        return ACLSAN_STATUS_ERROR_NOT_FOUND;
    }

    const auto& site = it->second;
    const uint64_t pc = query->pc != 0 ? query->pc : site.info.pc;
    uint32_t flags = ACLSAN_SYMBOLIZE_FLAG_FROM_SITE_MAP;
    if (site.sourceFile.empty() || site.sourceFile == "<unknown>") {
        flags |= ACLSAN_SYMBOLIZE_FLAG_FALLBACK;
    }

    std::ostringstream os;
    os << site.functionName << "\n"
       << site.sourceFile << ":" << site.info.sourceLine << ":0\n"
       << "pc=0x" << std::hex << pc << std::dec << " siteId=" << query->siteId << " binaryId=" << site.info.binaryId
       << " functionId=" << site.info.functionId << " op=" << site.opName << " flags=0x" << std::hex << flags
       << std::dec << "\n";

    const std::string text = os.str();
    *payloadBytes = static_cast<uint64_t>(text.size());
    if (payloadSize <= text.size()) {
        return ACLSAN_STATUS_ERROR_INVALID_VALUE;
    }
    CopyText(payload, static_cast<std::size_t>(payloadSize), text.c_str());
    return ACLSAN_STATUS_SUCCESS;
}

AclsanStatus ApiCore::SetLaunchUserData(AclsanLaunchHandle, void*, void*, const void*, uint64_t)
{
    return ACLSAN_STATUS_SUCCESS;
}

} // namespace aclsan
