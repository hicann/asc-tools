#include "api_core.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>

namespace ascsan {
namespace {

void EnsureDir(const std::string &path)
{
    if (path.empty()) {
        return;
    }
    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        std::cerr << "[ascsan-api] mkdir failed path=" << path << " errno=" << errno << "\n";
    }
}

std::string BaseName(const std::string &path)
{
    const std::size_t pos = path.find_last_of('/');
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool CopyFileOrWriteManifest(const std::string &src, const std::string &dst, uint32_t pipelineMask)
{
    std::ifstream input(src, std::ios::binary);
    if (input.good()) {
        std::ofstream output(dst, std::ios::binary);
        output << input.rdbuf();
        return output.good();
    }

    std::ofstream output(dst, std::ios::binary);
    output << "ASCSAN_DUMMY_PATCHED_OBJECT\n";
    output << "original=" << src << "\n";
    output << "pipelineMask=0x" << std::hex << pipelineMask << "\n";
    return output.good();
}

bool WriteMemoryImage(const void *imageData, uint64_t imageSize, const std::string &dst)
{
    std::ofstream output(dst, std::ios::binary);
    if (!output.good()) {
        return false;
    }
    output.write(static_cast<const char *>(imageData), static_cast<std::streamsize>(imageSize));
    return output.good();
}

std::string SelectCacheDir(const AscsanPatchOptions *options, const AscsanLaunchConfig &config)
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

AscsanStatus ApiCore::RegisterBuiltinPatchPipelines()
{
    const AscsanPatchPipelineDesc descs[] = {
        {ASCSAN_API_VERSION,
         sizeof(AscsanPatchPipelineDesc),
         ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG,
         ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION,
         ASCSAN_CBID_DEVICE_INSTRUCTION_SET_WAIT_FLAG,
         "SET_WAIT_FLAG",
         "ascsan_dev_trace_set_wait_flag",
         "set_wait_flag",
         ASCSAN_RAW_ARG_MAX,
         0},
        {ASCSAN_API_VERSION,
         sizeof(AscsanPatchPipelineDesc),
         ASCSAN_PATCH_PIPELINE_GET_RLS_BUF,
         ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION,
         ASCSAN_CBID_DEVICE_INSTRUCTION_GET_RLS_BUF,
         "GET_RLS_BUF",
         "ascsan_dev_trace_get_rls_buf",
         "get_rls_buf",
         ASCSAN_RAW_ARG_MAX,
         0},
        {ASCSAN_API_VERSION,
         sizeof(AscsanPatchPipelineDesc),
         ASCSAN_PATCH_PIPELINE_MTE2,
         ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION,
         ASCSAN_CBID_DEVICE_INSTRUCTION_MTE2,
         "MTE2",
         "ascsan_dev_trace_mte2",
         "mte2",
         ASCSAN_RAW_ARG_MAX,
         0},
        {ASCSAN_API_VERSION,
         sizeof(AscsanPatchPipelineDesc),
         ASCSAN_PATCH_PIPELINE_MTE3,
         ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION,
         ASCSAN_CBID_DEVICE_INSTRUCTION_MTE3,
         "MTE3",
         "ascsan_dev_trace_mte3",
         "mte3",
         ASCSAN_RAW_ARG_MAX,
         0},
        {ASCSAN_API_VERSION,
         sizeof(AscsanPatchPipelineDesc),
         ASCSAN_PATCH_PIPELINE_FIXPIPE,
         ASCSAN_CB_DOMAIN_DEVICE_INSTRUCTION,
         ASCSAN_CBID_DEVICE_INSTRUCTION_FIXPIPE,
         "FIXPIPE",
         "ascsan_dev_trace_fixpipe",
         "fixpipe",
         ASCSAN_RAW_ARG_MAX,
         0},
    };

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    for (const auto &desc : descs) {
        patchPipelines_[desc.pipeline] = desc;
    }
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::RegisterPatchImage(const AscsanPatchImageDesc *desc, uint64_t *patchImageId)
{
    if (desc == nullptr || patchImageId == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (desc->version != ASCSAN_API_VERSION || desc->size < sizeof(AscsanPatchImageDesc)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const uint64_t id = nextPatchImage_++;
    patchImages_[id] = *desc;
    *patchImageId = id;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::RegisterPatchPipeline(const AscsanPatchPipelineDesc *desc)
{
    if (desc == nullptr || desc->pipeline == ASCSAN_PATCH_PIPELINE_INVALID) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (desc->version != ASCSAN_API_VERSION || desc->size < sizeof(AscsanPatchPipelineDesc)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    patchPipelines_[desc->pipeline] = *desc;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::SetPatchOptions(const AscsanPatchOptions *options)
{
    if (options == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (options->version != ASCSAN_API_VERSION || options->size < sizeof(AscsanPatchOptions)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    patchOptions_ = *options;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::BuildPatchPlanForBinary(AscsanBinaryHandle, AscsanPatchPlanHandle *plan)
{
    if (plan == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    *plan = nextPatchPlan_++;
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::BuildDummyPatchResult(const std::string &originalPath,
                                            uint32_t pipelineMask,
                                            PatchResult *result)
{
    if (result == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    result->pipelineMask = pipelineMask;
    result->patchPlanId = nextPatchPlan_++;
    if (pipelineMask == 0) {
        result->patched = false;
        result->patchedPath = originalPath;
        return ASCSAN_STATUS_SUCCESS;
    }

    const std::string cacheDir = SelectCacheDir(&patchOptions_, config_);
    EnsureDir(cacheDir);
    result->patched = true;
    result->patchedPath = cacheDir + "/ascsan_dummy_patched_" + BaseName(originalPath);
    if (!CopyFileOrWriteManifest(originalPath, result->patchedPath, pipelineMask)) {
        return ASCSAN_STATUS_ERROR_IO;
    }

    const uint64_t binaryId = nextBinary_;
    const AscsanPatchPipeline pipelines[] = {
        ASCSAN_PATCH_PIPELINE_SET_WAIT_FLAG,
        ASCSAN_PATCH_PIPELINE_GET_RLS_BUF,
        ASCSAN_PATCH_PIPELINE_MTE2,
        ASCSAN_PATCH_PIPELINE_MTE3,
        ASCSAN_PATCH_PIPELINE_FIXPIPE,
    };
    for (AscsanPatchPipeline pipeline : pipelines) {
        if ((pipelineMask & PipelineMask(pipeline)) == 0) {
            continue;
        }
        PatchSiteRecord site{};
        site.functionName = "<unknown>";
        site.opName = PipelineName(pipeline);
        site.sourceFile = "<unknown>";
        site.info.version = ASCSAN_API_VERSION;
        site.info.size = sizeof(site.info);
        site.info.siteId = static_cast<uint32_t>(patchSites_.size() + result->sites.size() + 1);
        site.info.pipeline = pipeline;
        site.info.binaryId = binaryId;
        site.info.functionId = 0;
        site.info.pc = 0x1000 + static_cast<uint64_t>(pipeline) * 0x10;
        site.info.functionName = site.functionName.c_str();
        site.info.opName = site.opName.c_str();
        site.info.sourceFile = site.sourceFile.c_str();
        site.info.sourceLine = 0;
        result->sites.push_back(site);
    }
    return ASCSAN_STATUS_SUCCESS;
}

void ApiCore::StorePatchSites(uint64_t binaryId, const std::vector<PatchSiteRecord> &sites)
{
    for (auto site : sites) {
        site.info.binaryId = binaryId;
        site.info.functionName = site.functionName.c_str();
        site.info.opName = site.opName.c_str();
        site.info.sourceFile = site.sourceFile.c_str();
        patchSites_[site.info.siteId] = site;
    }
}

AscsanStatus ApiCore::PatchBinaryFromImage(const AscsanPatchImageDesc *image,
                                           const AscsanPatchOptions *options,
                                           char *patchedPath,
                                           uint64_t patchedPathSize,
                                           AscsanPatchPlanHandle *plan)
{
    if (image == nullptr || patchedPath == nullptr || patchedPathSize == 0) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    if (image->version != ASCSAN_API_VERSION || image->size < sizeof(AscsanPatchImageDesc)) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }
    if (options != nullptr &&
        (options->version != ASCSAN_API_VERSION || options->size < sizeof(AscsanPatchOptions))) {
        return ASCSAN_STATUS_ERROR_VERSION_MISMATCH;
    }

    std::string originalPath;
    if (image->kind == ASCSAN_PATCH_IMAGE_FILE) {
        if (image->path == nullptr || image->path[0] == '\0') {
            return ASCSAN_STATUS_ERROR_INVALID_VALUE;
        }
        originalPath = image->path;
    } else if (image->kind == ASCSAN_PATCH_IMAGE_MEMORY) {
        if (image->imageData == nullptr || image->imageSize == 0) {
            return ASCSAN_STATUS_ERROR_INVALID_VALUE;
        }
    } else {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (options != nullptr) {
        patchOptions_ = *options;
    }

    if (image->kind == ASCSAN_PATCH_IMAGE_MEMORY) {
        const std::string cacheDir = SelectCacheDir(&patchOptions_, config_);
        EnsureDir(cacheDir);
        originalPath = cacheDir + "/ascsan_input_image_" + std::to_string(nextBinary_) + ".o";
        if (!WriteMemoryImage(image->imageData, image->imageSize, originalPath)) {
            return ASCSAN_STATUS_ERROR_IO;
        }
    }

    const uint32_t pipelineMask = activeHookPlan_.patchPipelineMask;
    AscsanPatchData data{};
    data.version = ASCSAN_API_VERSION;
    data.size = sizeof(data);
    data.apiName = "ascsanPatchBinaryFromImage";
    data.originalPath = originalPath.c_str();
    data.pipelineMask = pipelineMask;
    Dispatch(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_BEGIN, &data);

    PatchResult result{};
    AscsanStatus status = BuildDummyPatchResult(originalPath.c_str(), pipelineMask, &result);
    if (status != ASCSAN_STATUS_SUCCESS) {
        data.result = static_cast<int>(status);
        Dispatch(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_END, &data);
        return status;
    }

    const uint64_t binaryId = nextBinary_++;
    StorePatchSites(binaryId, result.sites);
    data.binaryId = binaryId;
    data.patchPlanId = result.patchPlanId;
    data.patchedPath = result.patchedPath.c_str();
    data.result = ASCSAN_STATUS_SUCCESS;
    if (plan != nullptr) {
        *plan = result.patchPlanId;
    }
    std::snprintf(patchedPath, static_cast<size_t>(patchedPathSize), "%s", result.patchedPath.c_str());
    Dispatch(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_SITE_MAP_CREATED, &data);
    Dispatch(ASCSAN_CB_DOMAIN_PATCH, ASCSAN_CBID_PATCH_END, &data);
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::GetPatchSiteInfo(uint32_t siteId, AscsanPatchSiteInfo *info) const
{
    if (info == nullptr) {
        return ASCSAN_STATUS_ERROR_INVALID_VALUE;
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto it = patchSites_.find(siteId);
    if (it == patchSites_.end()) {
        return ASCSAN_STATUS_ERROR_NOT_FOUND;
    }
    *info = it->second.info;
    info->functionName = it->second.functionName.c_str();
    info->opName = it->second.opName.c_str();
    info->sourceFile = it->second.sourceFile.c_str();
    return ASCSAN_STATUS_SUCCESS;
}

AscsanStatus ApiCore::SetLaunchUserData(AscsanLaunchHandle,
                                        void *,
                                        void *,
                                        const void *,
                                        uint64_t)
{
    return ASCSAN_STATUS_SUCCESS;
}

} // namespace ascsan
