/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef ACLSAN_PROBE_RECORD_H_
#define ACLSAN_PROBE_RECORD_H_

#include <cstdint>
#include <type_traits>

namespace sanitizer {

inline constexpr uint64_t kProbeBlockBytes = 1024ULL * 1024ULL;
inline constexpr uint64_t kProbeRecordStartBytes = 24;

enum class ProbeInstrType : uint32_t {
    CopyGmToUbuf = 1,
    CopyGmToUbufAlignB16 = 6,
    CopyGmToUbufAlignB32 = 7,
    CopyGmToUbufAlignB8 = 8,
    CopyUbufToGmAlignV2 = 83,
    CopyGmToUbufAlignV2B8 = 84,
    CopyGmToUbufAlignV2B16 = 85,
    CopyGmToUbufAlignV2B32 = 86,
    SetFlag = 440,
    SetFlagI = 441,
    WaitFlag = 442,
    WaitFlagI = 443,
    GetBuf = 448,
    GetBufI = 449,
    RlsBuf = 450,
    RlsBufI = 451,
    SetFlagV = 456,
    SetFlagIV = 457,
    WaitFlagV = 458,
    WaitFlagIV = 459,
    GetBufV = 460,
    GetBufIV = 461,
    RlsBufV = 462,
    RlsBufIV = 463,
};

struct alignas(8) ProbeBlockHeader {
    uint64_t instrNum;
    uint64_t curAddr;
    uint32_t blockId;
    uint32_t phyCoreId;
};

struct alignas(8) ProbeRecordCommon {
    uint32_t instrType;
    int64_t pc;
};

struct alignas(8) CopyRecord {
    ProbeRecordCommon common;
    uint64_t dst;
    uint64_t src;
    uint64_t len;
};

struct alignas(8) CopyAlignRecord {
    ProbeRecordCommon common;
    uint64_t dst;
    uint64_t src;
    uint64_t config0;
    uint64_t config1;
};

struct alignas(8) FlagRecord {
    ProbeRecordCommon common;
    uint32_t srcPipe;
    uint32_t dstPipe;
    uint64_t eventId;
};

struct alignas(8) BufferRecord {
    ProbeRecordCommon common;
    uint64_t bufId;
    uint32_t pipe;
    uint8_t mode;
    uint8_t reserved[3];
};

static_assert(sizeof(ProbeBlockHeader) == 24);
static_assert(sizeof(ProbeRecordCommon) == 16);
static_assert(sizeof(CopyRecord) == 40);
static_assert(sizeof(CopyAlignRecord) == 48);
static_assert(sizeof(FlagRecord) == 32);
static_assert(sizeof(BufferRecord) == 32);
static_assert(kProbeRecordStartBytes == sizeof(ProbeBlockHeader));
static_assert(std::is_standard_layout<ProbeBlockHeader>::value);

} // namespace sanitizer

#endif // ACLSAN_PROBE_RECORD_H_
