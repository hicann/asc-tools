// Copyright (c) 2025 Huawei Technologies Co., Ltd.
// This program is free software, you can redistribute it and/or modify it under the terms and conditions of
// CANN Open Software License Agreement Version 2.0 (the "License").
// Please refer to the License for details. You may not use this file except in compliance with the License.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
// See LICENSE in the root of the software repository for the full text of the License.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aclsan {

enum class ProbeGroup : uint8_t { Mte1, Mte2, Mte3, Fixpipe, Scalar, Sync };

enum ProbeGroupMask : uint32_t {
    PROBE_GROUP_MTE1 = 1U << 0U,
    PROBE_GROUP_MTE2 = 1U << 1U,
    PROBE_GROUP_MTE3 = 1U << 2U,
    PROBE_GROUP_FIXPIPE = 1U << 3U,
    PROBE_GROUP_SYNC = 1U << 4U,
    PROBE_GROUP_SCALAR = 1U << 5U,
    PROBE_GROUP_ALL = PROBE_GROUP_MTE1 | PROBE_GROUP_MTE2 | PROBE_GROUP_MTE3 | PROBE_GROUP_FIXPIPE | PROBE_GROUP_SYNC |
                      PROBE_GROUP_SCALAR,
};

struct ToolchainPaths {
    std::string bisheng;
    std::string bishengTune;
    std::string ldLld;
    std::string llvmObjdump;

    bool Complete() const;
};

struct DbiRequest {
    std::string inputKernel;
    std::string outputKernel;
    std::string arch;
    uint32_t traceArgumentOffset = 0;
    std::vector<ProbeGroup> probeGroups;
    std::string toolchainRoot;
    std::string workDirectory;
    std::string cacheDirectory;
    bool strict = false;
    bool keepTemp = false;
    std::vector<std::string> extraTuneArgs;
};

struct DbiResult {
    bool success = false;
    std::string patchedPath;
    std::string stage;
    std::string diagnostic;
    uint32_t traceArgumentOffset = 0;
};

std::vector<ProbeGroup> NormalizeProbeGroups(const std::vector<ProbeGroup>& groups);
std::vector<ProbeGroup> ProbeGroupsFromMask(uint32_t mask);
std::string ProbeGroupName(ProbeGroup group);
std::string ValidateRequest(const DbiRequest& request);
ToolchainPaths ResolveToolchain(const std::string& cannRoot);
std::string CannRootFromRuntimeLibrary(const std::string& runtimeLibrary);
std::string MakeCacheKey(
    const std::string& arch, const std::vector<ProbeGroup>& groups, const std::string& objectIdentity);
DbiResult RunDbiPipeline(const DbiRequest& request);

} // namespace aclsan
