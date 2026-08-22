/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef NPU_CHECK_PROBE_CCE_INSTR_STRUCT_SYNC_H_
#define NPU_CHECK_PROBE_CCE_INSTR_STRUCT_SYNC_H_

#include <cstdint>

namespace sanitizer {
// =====================================
// ========   SET/WAIT FLAG    =========
// =====================================

// 对应 CCE 指令：
// - SET_FLAG.<src_pipe>.<dst_pipe> / SET_FLAGI.<src_pipe>.<dst_pipe>
// - WAIT_FLAG.<src_pipe>.<dst_pipe> / WAIT_FLAGI.<src_pipe>.<dst_pipe>
struct FlagParamField {
    uint32_t instr_id = 0;
    uint32_t srcPipe = 0;
    uint32_t dstPipe = 0;
    uint64_t eventId = 0;
};

// 对应 CCE 指令：WAIT_FLAG_DEV.<pipe> / WAIT_FLAG_DEVI.<pipe>
struct DeviceFlagParamField {
    uint32_t instr_id = 0;
    uint32_t pipe = 0;
    uint64_t flagId = 0;
};

// TODO: 这段指令还没有被使用，对应的刷新逻辑
// 对应 CCE 指令：
// - SET_FLAG_V.<dst_pipe> / SET_FLAGI_V.<dst_pipe>
// - WAIT_FLAG_V.<src_pipe> / WAIT_FLAGI_V.<src_pipe>
struct VectorFlagParamField {
    uint32_t instr_id = 0;
    uint32_t pipe = 0;
    uint64_t eventId = 0;
};

// 对应 CCE 指令：WAIT_FLAG_DEV_V / WAIT_FLAG_DEVI_V
struct VectorDeviceFlagParamField {
    uint32_t instr_id = 0;
    uint64_t flagId = 0;
};

// =====================================
// ========     GET/RLS BUF    =========
// =====================================

// - GET_BUF.<pipe> / GET_BUFI.<pipe>
// - RLS_BUF.<pipe> / RLS_BUFI.<pipe>
struct SyncBufParamField {
    uint32_t instr_id = 0;
    uint32_t pipe = 0;
    uint64_t bufId = 0;
    uint8_t mode = 0;
};

// - GET_BUF_V / GET_BUFI_V
// - RLS_BUF_V / RLS_BUFI_V
struct SyncBufvParamField {
    uint32_t instr_id = 0;
    uint64_t bufId = 0;
    uint8_t mode = 0;
};

using BufferParamField = SyncBufParamField;
using VectorBufferParamField = SyncBufvParamField;

// =====================================
// ========    暂时用不到       =========
// =====================================

// - HSET_FLAG.<src_pipe>.<dst_pipe> / HSET_FLAGI.<src_pipe>.<dst_pipe>
// - HWAIT_FLAG.<src_pipe>.<dst_pipe> / HWAIT_FLAGI.<src_pipe>.<dst_pipe>
struct HardwareFlagParamField {
    uint32_t instr_id = 0;
    uint32_t srcPipe = 0;
    uint32_t dstPipe = 0;
    uint64_t eventId = 0;
    uint64_t memory = 0;
    bool isVirtual = false;
};

using SetFlagParamField = FlagParamField;
using SetFlagIParamField = FlagParamField;
using WaitFlagParamField = FlagParamField;
using WaitFlagIParamField = FlagParamField;
using WaitFlagDevParamField = DeviceFlagParamField;
using WaitFlagDevIParamField = DeviceFlagParamField;
using SetFlagVParamField = VectorFlagParamField;
using SetFlagIVParamField = VectorFlagParamField;
using WaitFlagVParamField = VectorFlagParamField;
using WaitFlagIVParamField = VectorFlagParamField;
using WaitFlagDevVParamField = VectorDeviceFlagParamField;
using WaitFlagDevIVParamField = VectorDeviceFlagParamField;
using HSetFlagParamField = HardwareFlagParamField;
using HSetFlagIParamField = HardwareFlagParamField;
using HWaitFlagParamField = HardwareFlagParamField;
using HWaitFlagIParamField = HardwareFlagParamField;
using GetBufParamField = SyncBufParamField;
using GetBufIParamField = SyncBufParamField;
using RlsBufParamField = SyncBufParamField;
using RlsBufIParamField = SyncBufParamField;
using GetBufVParamField = SyncBufvParamField;
using GetBufIVParamField = SyncBufvParamField;
using RlsBufVParamField = SyncBufvParamField;
using RlsBufIVParamField = SyncBufvParamField;

} // namespace sanitizer

#endif // NPU_CHECK_PROBE_CCE_INSTR_STRUCT_SYNC_H_
